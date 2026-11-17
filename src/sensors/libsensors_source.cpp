// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/libsensors_source.h"
#include "sensors/sensors_policy.h"

#include <sensors/error.h>
#include <sensors/sensors.h>

#include <QDebug>
#include <QSet>

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <optional>

// PWM outputs need libsensors API 0x501, frequency inputs API 0x510 (lm-sensors 3.6.2);
// older libsensors (e.g. API 0x440, lm-sensors 3.4 on EL8) has neither.
#define QSENSORS_HAVE_PWM (SENSORS_API_VERSION >= 0x501)
#define QSENSORS_HAVE_FREQ (SENSORS_API_VERSION >= 0x510)

namespace {
    // libsensors defines no hard name-length limit; 256 covers all real-world chip names.
    constexpr size_t kChipNameBufferSize = 256;

#if QSENSORS_HAVE_PWM
    // API 0x510 (lm-sensors 3.6.2) divides pwm* by a scaling factor of 2.55 that its
    // integer return type truncates to 2; API 0x501 reports the raw duty. Multiplying back
    // gives the kernel's 0..255 value.
#if SENSORS_API_VERSION >= 0x510
    constexpr double kLibsensorsPwmDivisor = 2.0;
#else
    constexpr double kLibsensorsPwmDivisor = 1.0;
#endif
#endif

    /** Maps libsensors input types to visible category columns. */
    [[nodiscard]] SensorCategory categoryForType(const sensors_subfeature_type type) {
        switch (type) {
            case SENSORS_SUBFEATURE_TEMP_INPUT:
                return SensorCategory::Temperatures;
            case SENSORS_SUBFEATURE_IN_INPUT:
                return SensorCategory::Voltages;
            case SENSORS_SUBFEATURE_FAN_INPUT:
                return SensorCategory::Fans;
            case SENSORS_SUBFEATURE_CURR_INPUT:
                return SensorCategory::Currents;
            case SENSORS_SUBFEATURE_POWER_INPUT:
            case SENSORS_SUBFEATURE_POWER_AVERAGE:
            case SENSORS_SUBFEATURE_ENERGY_INPUT:
                return SensorCategory::Power;
#if QSENSORS_HAVE_PWM
            case SENSORS_SUBFEATURE_PWM_IO:
                // Fan control outputs sit next to the fans they drive.
                return SensorCategory::Fans;
#endif
            default:
                return SensorCategory::Other;
        }
    }

    /** Unit kind used by the LCD renderer and range logic. */
    [[nodiscard]] SensorUnit unitForType(const sensors_subfeature_type type) {
        switch (type) {
            case SENSORS_SUBFEATURE_TEMP_INPUT:
                return SensorUnit::Celsius;
            case SENSORS_SUBFEATURE_IN_INPUT:
                return SensorUnit::Volt;
            case SENSORS_SUBFEATURE_FAN_INPUT:
                return SensorUnit::Rpm;
            case SENSORS_SUBFEATURE_CURR_INPUT:
                return SensorUnit::Ampere;
            case SENSORS_SUBFEATURE_POWER_INPUT:
            case SENSORS_SUBFEATURE_POWER_AVERAGE:
                return SensorUnit::Watt;
            case SENSORS_SUBFEATURE_ENERGY_INPUT:
                return SensorUnit::Joule;
            case SENSORS_SUBFEATURE_HUMIDITY_INPUT:
                return SensorUnit::RelativeHumidity;
#if QSENSORS_HAVE_PWM
            case SENSORS_SUBFEATURE_PWM_IO:
                return SensorUnit::Percent;
#endif
#if QSENSORS_HAVE_FREQ
            case SENSORS_SUBFEATURE_FREQ_INPUT:
                return SensorUnit::Hertz;
#endif
            default:
                return SensorUnit::Unknown;
        }
    }

    /** @p value as libsensors returns it for @p type, in the unit of unitForType(). */
    [[nodiscard]] std::optional<double> toSourceUnit(const sensors_subfeature_type type, const std::optional<double> value) {
#if QSENSORS_HAVE_PWM
        if (value && type == SENSORS_SUBFEATURE_PWM_IO)
            return SensorsPolicy::pwmDutyPercent(*value * kLibsensorsPwmDivisor);
#else
        Q_UNUSED(type);
#endif
        return value;
    }

    /** Value of @p sf; nullopt when unreadable or not finite. */
    [[nodiscard]] std::optional<double> readSubfeatureValue(const sensors_chip_name *chip, const sensors_subfeature *sf) {
        double value = 0.0;
        if (sensors_get_value(chip, sf->number, &value) != 0) {
            return std::nullopt;
        }
        if (!std::isfinite(value)) {
            return std::nullopt;
        }
        return value;
    }

    [[nodiscard]] std::optional<double> readSubfeatureValue(const sensors_chip_name *chip, const sensors_feature *feature, const sensors_subfeature_type type) {
        const sensors_subfeature *sf = sensors_get_subfeature(chip, feature, type);
        if (sf == nullptr) {
            return std::nullopt;
        }
        return readSubfeatureValue(chip, sf);
    }

    struct RangeInfo {
        std::optional<double> min;
        std::optional<double> max;
    };

    struct InputSelection {
        const sensors_subfeature *subfeature = nullptr;
        sensors_subfeature_type type = SENSORS_SUBFEATURE_UNKNOWN;
    };

    /** Reads native min/max limits where available for the given input type. */
    [[nodiscard]] RangeInfo readRange(const sensors_chip_name *chip, const sensors_feature *feature, const sensors_subfeature_type type) {
        RangeInfo range;

        switch (type) {
            case SENSORS_SUBFEATURE_TEMP_INPUT:
                range.min = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_TEMP_MIN);
                range.max = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_TEMP_MAX);
                // crit stands in for a missing or placeholder max (NVMe reports 65261.85 °C).
                if (!range.max || !SensorsPolicy::isPlausibleTemperatureLimitC(*range.max)) {
                    if (const auto crit = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_TEMP_CRIT))
                        range.max = crit;
                }
                break;
            case SENSORS_SUBFEATURE_IN_INPUT:
                range.min = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_IN_MIN);
                range.max = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_IN_MAX);
                break;
            case SENSORS_SUBFEATURE_FAN_INPUT:
                range.min = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_FAN_MIN);
                range.max = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_FAN_MAX);
                break;
            case SENSORS_SUBFEATURE_CURR_INPUT:
                range.min = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_CURR_MIN);
                range.max = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_CURR_MAX);
                break;
            case SENSORS_SUBFEATURE_POWER_INPUT:
            case SENSORS_SUBFEATURE_POWER_AVERAGE:
#if SENSORS_API_VERSION >= 0x510
                // power*_min is missing in older libsensors (API 0x440, lm-sensors 3.4 on EL8).
                range.min = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_POWER_MIN);
#endif
                range.max = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_POWER_MAX);
                if (!range.max) {
                    range.max = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_POWER_CAP);
                }
                if (!range.max) {
                    range.max = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_POWER_CRIT);
                }
                break;
            default:
                break;
        }
        return range;
    }

    [[nodiscard]] InputSelection selectInputSubfeature(const sensors_chip_name *chip, const sensors_feature *feature) {
        InputSelection selection{};
        // Priority order defines which "input" is shown when a feature exposes multiple candidates.
        constexpr sensors_subfeature_type candidates[] = {
            SENSORS_SUBFEATURE_TEMP_INPUT,
            SENSORS_SUBFEATURE_IN_INPUT,
            SENSORS_SUBFEATURE_FAN_INPUT,
            SENSORS_SUBFEATURE_CURR_INPUT,
            SENSORS_SUBFEATURE_POWER_INPUT,
            // amdgpu reports only power1_average (PPT).
            SENSORS_SUBFEATURE_POWER_AVERAGE,
            SENSORS_SUBFEATURE_ENERGY_INPUT,
            SENSORS_SUBFEATURE_HUMIDITY_INPUT,
#if QSENSORS_HAVE_PWM
            SENSORS_SUBFEATURE_PWM_IO,
#endif
#if QSENSORS_HAVE_FREQ
            SENSORS_SUBFEATURE_FREQ_INPUT,
#endif
        };

        for (const sensors_subfeature_type type: candidates) {
            if (const sensors_subfeature *sf = sensors_get_subfeature(chip, feature, type); sf != nullptr) {
                selection.subfeature = sf;
                selection.type = type;
                return selection;
            }
        }

        return selection;
    }

    [[nodiscard]] QString resolveFeatureLabel(const sensors_chip_name *chip, const sensors_feature *feature) {
        // sensors_get_label allocates with malloc; unique_ptr gives exception-safe cleanup.
        std::unique_ptr<char, decltype(&std::free)> label(sensors_get_label(chip, feature), std::free);
        if (label)
            return QString::fromUtf8(label.get());
        return QString::fromUtf8(feature->name != nullptr ? feature->name : "unknown");
    }

    // Polling repeats every few seconds; each problem is logged once per session.
    void reportOnce(QSet<QString> &reportedProblems, const QString &key, const QString &message) {
        if (!reportedProblems.contains(key)) {
            reportedProblems.insert(key);
            qWarning("qsensors: %s", qPrintable(message));
        }
    }

    void appendFeatureReading(const sensors_chip_name *chip, const QString &chipName, const sensors_feature *feature,
                              QVector<SensorReading> &readings, QSet<QString> &reportedProblems) {
        const InputSelection selected = selectInputSubfeature(chip, feature);
        // An input without read permission never delivers a value; it gets no card.
        if (selected.subfeature == nullptr || (selected.subfeature->flags & SENSORS_MODE_R) == 0) {
            return;
        }

        // Intermittent failures (runtime suspend, stopped fan, drive standby) keep the card;
        // the reading carries no value until the input can be read again.
        const std::optional<double> value = toSourceUnit(selected.type, readSubfeatureValue(chip, selected.subfeature));
        if (!value.has_value()) {
            reportOnce(reportedProblems, chipName + QLatin1Char('/') + QString::number(feature->number),
                       QStringLiteral("cannot read sensor %1 on %2")
                           .arg(QString::fromUtf8(feature->name != nullptr ? feature->name : "?"), chipName));
        }

        SensorReading reading{
            .chip = chipName,
            .category = categoryForType(selected.type),
            .feature = resolveFeatureLabel(chip, feature),
            .featureNumber = feature->number,
            .subfeatureNumber = selected.subfeature->number,
            .value = value,
            .unit = unitForType(selected.type),
        };

        const RangeInfo nativeRange = readRange(chip, feature, selected.type);
        reading.firmwareMin = nativeRange.min;
        reading.firmwareMax = nativeRange.max;
        readings.push_back(reading);
    }

    /** Chip name as printed by `sensors`; nullopt when formatting fails or would be truncated. */
    [[nodiscard]] std::optional<QString> chipNameFrom(const sensors_chip_name *chip) {
        char chipNameBuffer[kChipNameBufferSize] = {0};
        const int length = sensors_snprintf_chip_name(chipNameBuffer, sizeof(chipNameBuffer), chip);
        if (length < 0 || static_cast<size_t>(length) >= sizeof(chipNameBuffer)) {
            return std::nullopt;
        }
        return QString::fromUtf8(chipNameBuffer, length);
    }

    void appendChipReadings(const sensors_chip_name *chip, QVector<SensorReading> &readings, QSet<QString> &reportedProblems) {
        const std::optional<QString> chipName = chipNameFrom(chip);
        if (!chipName.has_value() || chipName->isEmpty()) {
            reportOnce(reportedProblems, QStringLiteral("chip:%1").arg(chip->addr),
                       QStringLiteral("skipping chip with unusable name (prefix %1)")
                           .arg(QString::fromUtf8(chip->prefix != nullptr ? chip->prefix : "?")));
            return;
        }

        const sensors_feature *feature = nullptr;
        int featureNr = 0;
        while ((feature = sensors_get_features(chip, &featureNr)) != nullptr) {
            appendFeatureReading(chip, *chipName, feature, readings, reportedProblems);
        }
    }

    // sensors_init()/sensors_cleanup() manage process-wide state; one source at a time.
    std::atomic_bool g_backendActive{false};
}

LibsensorsSource::LibsensorsSource() : m_initialized(false) {
    if (g_backendActive.exchange(true)) {
        m_lastError = QStringLiteral("another LibsensorsSource instance is already active");
        return;
    }
    // nullptr reads the system configuration (/etc/sensors3.conf, /etc/sensors.d/); its
    // compute expressions affect values, so it must only be writable by root.
    const int rc = sensors_init(nullptr);
    if (rc != 0) {
        m_lastError = QStringLiteral("sensors_init failed: %1 (%2)").arg(QString::fromUtf8(sensors_strerror(rc)), QString::number(rc));
        g_backendActive = false;
        return;
    }
    m_initialized = true;
}

LibsensorsSource::~LibsensorsSource() {
    if (m_initialized) {
        sensors_cleanup();
        g_backendActive = false;
    }
}

bool LibsensorsSource::isInitialized() const {
    return m_initialized;
}

QString LibsensorsSource::lastError() const {
    return m_lastError;
}

QVector<SensorReading> LibsensorsSource::readAll() {
    QVector<SensorReading> readings;
    if (!m_initialized) {
        return readings;
    }

    const sensors_chip_name *chip = nullptr;
    int chipNr = 0;
    while ((chip = sensors_get_detected_chips(nullptr, &chipNr)) != nullptr) {
        appendChipReadings(chip, readings, m_reportedProblems);
    }

    return readings;
}
