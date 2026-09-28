// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/reading_pipeline.h"
#include "sensors/sensors_policy.h"

#include <optional>

namespace {
    [[nodiscard]] double celsiusToFahrenheit(const double valueC) {
        return (valueC * (9.0 / 5.0)) + 32.0;
    }

    void applyTemperatureUnit(SensorReading &reading, const TemperatureUnit temperatureUnit) {
        if (temperatureUnit != TemperatureUnit::Fahrenheit || reading.unit != SensorUnit::Celsius) {
            return;
        }

        reading.value = celsiusToFahrenheit(reading.value);
        for (std::optional<double> *limit: {&reading.minValue, &reading.maxValue, &reading.firmwareMin, &reading.firmwareMax}) {
            if (*limit)
                *limit = celsiusToFahrenheit(**limit);
        }
        reading.unit = SensorUnit::Fahrenheit;
    }
}

QVector<SensorReading> ReadingPipeline::process(const QVector<SensorReading> &raw, const RuntimeConfig &config) {
    QVector<SensorReading> readings = raw;
    for (SensorReading &reading: readings) {
        const QString latchKey = reading.chip + QLatin1Char(':') + QString::number(reading.featureNumber);
        const auto latchIt = m_milliScaleLatch.constFind(latchKey);
        std::optional<bool> latchedMilli = latchIt != m_milliScaleLatch.cend() ? std::optional(*latchIt) : std::nullopt;
        SensorsPolicy::applyCurrentPowerUnitScaling(reading.unit, reading.value, reading.firmwareMin,
                                                    reading.firmwareMax, &latchedMilli);
        if (latchedMilli.has_value()) {
            m_milliScaleLatch.insert(latchKey, *latchedMilli);
        }
        // The bar graph starts from the firmware limits; the policy fills in what is missing.
        reading.minValue = reading.firmwareMin;
        reading.maxValue = reading.firmwareMax;
        SensorsPolicy::applyDefaultRangePolicy(reading.category, reading.value, reading.minValue, reading.maxValue,
                                               config.fanDefaultMaxRpm);
        applyTemperatureUnit(reading, config.temperatureUnit);
    }
    return readings;
}
