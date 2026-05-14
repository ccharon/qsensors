// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors_backend.h"

#include <sensors/sensors.h>

#include <cstdlib>
#include <optional>

namespace {
// Maps libsensors input types to visible category columns.
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

// Unit symbol used by the LCD renderer and range logic.
QString unitForType(const sensors_subfeature_type type) {
    switch (type) {
        case SENSORS_SUBFEATURE_TEMP_INPUT:
            return QStringLiteral("°C");
        case SENSORS_SUBFEATURE_IN_INPUT:
            return QStringLiteral("V");
        case SENSORS_SUBFEATURE_FAN_INPUT:
            return QStringLiteral("RPM");
        case SENSORS_SUBFEATURE_CURR_INPUT:
            return QStringLiteral("A");
        case SENSORS_SUBFEATURE_POWER_INPUT:
            return QStringLiteral("W");
        default:
            return QString();
    }
}

// Helper that returns nullopt when a subfeature is missing or unreadable.
std::optional<double> readSubfeatureValue(const sensors_chip_name *chip, const sensors_feature *feature, const sensors_subfeature_type type) {
    const sensors_subfeature *sf = sensors_get_subfeature(chip, feature, type);
    if (sf == nullptr) {
        return std::nullopt;
    }
    double value = 0.0;
    if (sensors_get_value(chip, sf->number, &value) != 0) {
        return std::nullopt;
    }
    return value;
}

struct RangeInfo {
    std::optional<double> min;
    std::optional<double> max;
};

// Reads native min/max limits where available for the given input type.
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
        case SENSORS_SUBFEATURE_POWER_INPUT:
            break;
        default:
            break;
    }
    return range;
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

QVector<SensorReading> SensorsBackend::readAll() const {
    QVector<SensorReading> readings;
    if (!m_initialized) {
        return readings;
    }

    const sensors_chip_name *chip = nullptr;
    int chipNr = 0;
    while ((chip = sensors_get_detected_chips(nullptr, &chipNr)) != nullptr) {
        char chipNameBuffer[256] = {0};
        if (sensors_snprintf_chip_name(chipNameBuffer, sizeof(chipNameBuffer), chip) < 0) {
            continue;
        }
        const QString chipName = QString::fromUtf8(chipNameBuffer);

        const sensors_feature *feature = nullptr;
        int featureNr = 0;
        while ((feature = sensors_get_features(chip, &featureNr)) != nullptr) {
            // Select the first supported input subfeature in priority order.
            sensors_subfeature_type inputType = SENSORS_SUBFEATURE_UNKNOWN;
            const sensors_subfeature *input = sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_TEMP_INPUT);
            if (input != nullptr) inputType = SENSORS_SUBFEATURE_TEMP_INPUT;
            if (input == nullptr) {
                input = sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_IN_INPUT);
                if (input != nullptr) inputType = SENSORS_SUBFEATURE_IN_INPUT;
            }
            if (input == nullptr) {
                input = sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_FAN_INPUT);
                if (input != nullptr) inputType = SENSORS_SUBFEATURE_FAN_INPUT;
            }
            if (input == nullptr) {
                input = sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_CURR_INPUT);
                if (input != nullptr) inputType = SENSORS_SUBFEATURE_CURR_INPUT;
            }
            if (input == nullptr) {
                input = sensors_get_subfeature(chip, feature, SENSORS_SUBFEATURE_POWER_INPUT);
                if (input != nullptr) inputType = SENSORS_SUBFEATURE_POWER_INPUT;
            }

            if (input == nullptr) {
                continue;
            }

            double value = 0.0;
            if (sensors_get_value(chip, input->number, &value) != 0) {
                continue;
            }

            const char *labelRaw = sensors_get_label(chip, feature);
            const QString label = labelRaw != nullptr
                                      ? QString::fromUtf8(labelRaw)
                                      : QString::fromUtf8(feature->name != nullptr ? feature->name : "unknown");
            if (labelRaw != nullptr) {
                free(const_cast<char *>(labelRaw));
            }

            SensorReading reading{
                .chip = chipName,
                .category = categoryForType(inputType),
                .feature = label,
                .value = value,
                .unit = unitForType(inputType),
            };

            const RangeInfo range = readRange(chip, feature, inputType);
            std::optional<double> min = range.min;
            std::optional<double> max = range.max;

            // Sensible defaults when firmware does not expose limits.
            if (inputType == SENSORS_SUBFEATURE_TEMP_INPUT) {
                if (!min.has_value() && !max.has_value()) {
                    min = 0.0;
                    max = 100.0;
                } else if (!min.has_value() && max.has_value()) {
                    min = 0.0;
                }
            }

            if (inputType == SENSORS_SUBFEATURE_FAN_INPUT) {
                if (!min.has_value()) {
                    min = 0.0;
                }
                if (!max.has_value()) {
                    max = 10000.0;
                }
            }

            if (min.has_value() || max.has_value()) {
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
            readings.push_back(reading);
        }
    }

    return readings;
}
