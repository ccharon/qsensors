// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors_backend.h"
#include "sensors_policy.h"

#include <sensors/sensors.h>

#include <cmath>
#include <cstdlib>
#include <optional>

namespace {
    [[nodiscard]] double celsiusToFahrenheit(const double valueC) {
        return (valueC * (9.0 / 5.0)) + 32.0;
    }

    /** Maps libsensors input types to visible category columns. */
    SensorCategory categoryForType(const sensors_subfeature_type type) {
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
                return SensorCategory::Power;
            default:
                return SensorCategory::Other;
        }
    }

    /** Unit kind used by the LCD renderer and range logic. */
    SensorUnit unitForType(const sensors_subfeature_type type) {
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
                return SensorUnit::Watt;
            default:
                return SensorUnit::Unknown;
        }
    }

    /**  Helper that returns nullopt when a subfeature is missing or unreadable. */
    std::optional<double> readSubfeatureValue(const sensors_chip_name *chip, const sensors_subfeature *sf) {
        double value = 0.0;
        if (sensors_get_value(chip, sf->number, &value) != 0) {
            return std::nullopt;
        }
        if (!std::isfinite(value)) {
            return std::nullopt;
        }
        return value;
    }

    std::optional<double> readSubfeatureValue(const sensors_chip_name *chip, const sensors_feature *feature, const sensors_subfeature_type type) {
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
    RangeInfo readRange(const sensors_chip_name *chip, const sensors_feature *feature, const sensors_subfeature_type type) {
        RangeInfo range;

        switch (type) {
            case SENSORS_SUBFEATURE_TEMP_INPUT:
                range.min = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_TEMP_MIN);
                range.max = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_TEMP_MAX);
                if (!range.max) {
                    range.max = readSubfeatureValue(chip, feature, SENSORS_SUBFEATURE_TEMP_CRIT);
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
            default:
                break;
        }
        return range;
    }

    InputSelection selectInputSubfeature(const sensors_chip_name *chip, const sensors_feature *feature) {
        InputSelection selection{};
        // Priority order defines which "input" is shown when a feature exposes multiple candidates.
        constexpr sensors_subfeature_type candidates[] = {
            SENSORS_SUBFEATURE_TEMP_INPUT,
            SENSORS_SUBFEATURE_IN_INPUT,
            SENSORS_SUBFEATURE_FAN_INPUT,
            SENSORS_SUBFEATURE_CURR_INPUT,
            SENSORS_SUBFEATURE_POWER_INPUT
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

    QString resolveFeatureLabel(const sensors_chip_name *chip, const sensors_feature *feature) {
        const char *labelRaw = sensors_get_label(chip, feature);
        const QString label = labelRaw != nullptr ? QString::fromUtf8(labelRaw) : QString::fromUtf8(feature->name != nullptr ? feature->name : "unknown");

        if (labelRaw != nullptr) {
            // libsensors allocates this label buffer; caller must free it.
            free(const_cast<char *>(labelRaw));
        }

        return label;
    }

    void applyRangeToReading(SensorReading &reading, const std::optional<double> &min, const std::optional<double> &max) {
        if (!min.has_value() && !max.has_value()) {
            return;
        }
        reading.hasRange = true;
        if (min.has_value()) {
            reading.hasMin = true;
            reading.minValue = *min;
        }
        if (max.has_value()) {
            reading.hasMax = true;
            reading.maxValue = *max;
        }
    }

    void applyTemperatureUnitToReading(SensorReading &reading, const TemperatureUnit temperatureUnit) {
        if (temperatureUnit != TemperatureUnit::Fahrenheit || reading.unit != SensorUnit::Celsius) {
            return;
        }

        reading.value = celsiusToFahrenheit(reading.value);
        if (reading.hasMin) {
            reading.minValue = celsiusToFahrenheit(reading.minValue);
        }
        if (reading.hasMax) {
            reading.maxValue = celsiusToFahrenheit(reading.maxValue);
        }
        reading.unit = SensorUnit::Fahrenheit;
    }

    bool appendFeatureReading(const sensors_chip_name *chip, const QString &chipName, const sensors_feature *feature,
                              QVector<SensorReading> &readings, const int defaultFanMaxRpm, const TemperatureUnit temperatureUnit) {
        const InputSelection selected = selectInputSubfeature(chip, feature);

        if (selected.subfeature == nullptr) {
            return false;
        }

        const std::optional<double> value = readSubfeatureValue(chip, selected.subfeature);
        if (!value.has_value()) {
            return false;
        }

        SensorReading reading{
            .chip = chipName,
            .category = categoryForType(selected.type),
            .feature = resolveFeatureLabel(chip, feature),
            .featureNumber = feature->number,
            .subfeatureNumber = selected.subfeature->number,
            .value = *value,
            .unit = unitForType(selected.type),
        };

        const RangeInfo nativeRange = readRange(chip, feature, selected.type);
        std::optional<double> min = nativeRange.min;
        std::optional<double> max = nativeRange.max;
        SensorsPolicy::applyDefaultRangePolicy(reading.category, reading.value, min, max, defaultFanMaxRpm);
        applyRangeToReading(reading, min, max);
        applyTemperatureUnitToReading(reading, temperatureUnit);
        readings.push_back(reading);
        return true;
    }

    QString chipNameFrom(const sensors_chip_name *chip) {
        char chipNameBuffer[256] = {0};
        if (sensors_snprintf_chip_name(chipNameBuffer, sizeof(chipNameBuffer), chip) < 0) {
            return {};
        }
        return QString::fromUtf8(chipNameBuffer);
    }

    void appendChipReadings(const sensors_chip_name *chip, QVector<SensorReading> &readings,
                            const int defaultFanMaxRpm, const TemperatureUnit temperatureUnit) {
        const QString chipName = chipNameFrom(chip);
        if (chipName.isEmpty()) {
            return;
        }

        const sensors_feature *feature = nullptr;
        int featureNr = 0;
        while ((feature = sensors_get_features(chip, &featureNr)) != nullptr) {
            appendFeatureReading(chip, chipName, feature, readings, defaultFanMaxRpm, temperatureUnit);
        }
    }
}

SensorsBackend::SensorsBackend() : m_initialized(false) {
    const int rc = sensors_init(nullptr);
    if (rc != 0) {
        m_lastError = QStringLiteral("sensors_init failed (%1)").arg(rc);
        return;
    }
    m_initialized = true;
}

SensorsBackend::~SensorsBackend() {
    if (m_initialized) {
        sensors_cleanup();
    }
}

bool SensorsBackend::isInitialized() const {
    return m_initialized;
}

QString SensorsBackend::lastError() const {
    return m_lastError;
}

QVector<SensorReading> SensorsBackend::readAll(const int defaultFanMaxRpm, const TemperatureUnit temperatureUnit) const {
    QVector<SensorReading> readings;
    if (!m_initialized) {
        return readings;
    }

    const sensors_chip_name *chip = nullptr;
    int chipNr = 0;
    while ((chip = sensors_get_detected_chips(nullptr, &chipNr)) != nullptr) {
        appendChipReadings(chip, readings, defaultFanMaxRpm, temperatureUnit);
    }

    return readings;
}
