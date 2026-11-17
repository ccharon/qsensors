// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/reading_pipeline.h"
#include "sensors/sensors_policy.h"

#include <cmath>
#include <optional>

namespace {
    [[nodiscard]] double celsiusToFahrenheit(const double valueC) {
        return (valueC * (9.0 / 5.0)) + 32.0;
    }

    void applyTemperatureUnit(SensorReading &reading, const TemperatureUnit temperatureUnit) {
        if (temperatureUnit != TemperatureUnit::Fahrenheit || reading.unit != SensorUnit::Celsius) {
            return;
        }

        for (std::optional<double> *limit: {&reading.value, &reading.minValue, &reading.maxValue, &reading.firmwareMin, &reading.firmwareMax}) {
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
        const auto latchIt = m_scaleLatch.constFind(latchKey);
        std::optional<SensorsPolicy::ScaleLatch> latch;
        if (latchIt != m_scaleLatch.cend() && latchIt->unit == reading.unit)
            latch = latchIt->latch;
        // A non-finite value is no value: no alert, no bar graph, placeholder on the LCD.
        if (reading.value && !std::isfinite(*reading.value)) {
            reading.value.reset();
        }
        // Before the °F conversion: the rule works on raw source units.
        SensorsPolicy::dropImplausibleFirmwareLimits(reading.unit, reading.firmwareMin, reading.firmwareMax);
        // The bar graph starts from the firmware limits; the policy fills in what is missing.
        reading.minValue = reading.firmwareMin;
        reading.maxValue = reading.firmwareMax;
        SensorsPolicy::applyDefaultRangePolicy(reading.unit, reading.value, reading.minValue, reading.maxValue,
                                               config.fanDefaultMaxRpm);
        applyTemperatureUnit(reading, config.temperatureUnit);
        // After the °F conversion: the scale is chosen for the values as displayed.
        reading.scale = SensorsPolicy::chooseScale(reading.unit, reading.value, reading.firmwareMin,
                                                   reading.firmwareMax, &latch);
        if (latch.has_value()) {
            m_scaleLatch.insert(latchKey, {reading.unit, *latch});
        }
    }
    return readings;
}
