// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensor_reading.h"

#include <algorithm>
#include <cmath>
#include <optional>

/** Rules applied to raw readings: unit scaling, default ranges and alert state. */
namespace SensorsPolicy {
    /** Milli-unit readings at or above this no longer fit the six-cell LCD value ("9999.9"). */
    inline constexpr double kMilliUnitOverflow = 10000.0;
    /** Base-unit readings below this lose too much precision at two decimals ("0.04"). */
    inline constexpr double kBaseUnitUnderflow = 0.1;

    /**
     * Rescales Ampere/Watt to mA/mW for sub-1 sensors. Native limits decide when present;
     * otherwise @p latchedMilli carries the scale between polls with hysteresis
     * (milli below kBaseUnitUnderflow, base from kMilliUnitOverflow) to avoid flicker.
     */
    inline void applyCurrentPowerUnitScaling(
        SensorUnit &unit,
        double &value,
        std::optional<double> &min,
        std::optional<double> &max,
        std::optional<bool> *latchedMilli = nullptr
    ) {
        if (unit != SensorUnit::Ampere && unit != SensorUnit::Watt) {
            return;
        }

        constexpr double kMilliScale = 1000.0;
        bool useMilli = false;
        if (max.has_value() || min.has_value()) {
            const double reference = max.has_value() ? *max : *min;
            useMilli = std::isfinite(reference) && std::abs(reference) < 1.0;
        } else if (!std::isfinite(value)) {
            return;
        } else {
            const double magnitude = std::abs(value);
            if (latchedMilli && latchedMilli->has_value()) {
                useMilli = **latchedMilli ? magnitude * kMilliScale < kMilliUnitOverflow
                                          : magnitude < kBaseUnitUnderflow;
            } else {
                useMilli = magnitude < 1.0;
            }
            if (latchedMilli) {
                *latchedMilli = useMilli;
            }
        }

        if (!useMilli) {
            return;
        }
        value *= kMilliScale;
        if (min.has_value()) *min *= kMilliScale;
        if (max.has_value()) *max *= kMilliScale;
        unit = (unit == SensorUnit::Ampere) ? SensorUnit::Milliampere : SensorUnit::Milliwatt;
    }

    /** Applies qsensors default range policy when firmware does not expose limits. */
    inline void applyDefaultRangePolicy(
        const SensorCategory category,
        const double measuredValue,
        std::optional<double> &min,
        std::optional<double> &max,
        const int fanDefaultMaxRpm
    ) {
        if (!std::isfinite(measuredValue)) {
            if (!min.has_value()) min = 0.0;
            if (!max.has_value()) max = 1.0;
            return;
        }

        if (category == SensorCategory::Temperatures) {
            if (!min.has_value()) {
                min = 0.0;
            }
            if (!max.has_value()) {
                max = 100.0;
            }
        }

        if (category == SensorCategory::Fans) {
            if (!min.has_value()) {
                min = 0.0;
            }
            if (!max.has_value()) {
                max = static_cast<double>(fanDefaultMaxRpm);
            }
        }

        if (category == SensorCategory::Voltages ||
            category == SensorCategory::Currents ||
            category == SensorCategory::Power) {
            // Current and power are signed (battery charge/discharge); mirror the range below zero.
            const bool signedWithoutNativeMin = category != SensorCategory::Voltages
                                                && measuredValue < 0.0 && !min.has_value();
            if (!min.has_value() && !max.has_value()) {
                min = 0.0;
                max = std::max(1.0, std::abs(measuredValue) * 1.5);
            } else if (!min.has_value() && max.has_value()) {
                min = std::max(0.0, *max * 0.2);
            } else if (min.has_value() && !max.has_value()) {
                max = std::max(measuredValue, *min + std::max(1.0, std::abs(*min) * 0.5));
            }
            if (signedWithoutNativeMin) {
                min = -std::max({1.0, std::abs(measuredValue) * 1.5, std::abs(*max)});
            }
        }

        if (category == SensorCategory::Other) {
            if (!min.has_value() && !max.has_value()) {
                const double span = std::max(1.0, std::abs(measuredValue) * 0.5);
                min = measuredValue - span;
                max = measuredValue + span;
            } else if (!min.has_value() && max.has_value()) {
                min = *max - std::max(1.0, std::abs(*max) * 0.8);
            } else if (min.has_value() && !max.has_value()) {
                max = *min + std::max(1.0, std::abs(*min) * 0.8);
            }
        }

        if (min.has_value() && max.has_value() && *max <= *min) {
            *max = *min + std::max(1.0, std::abs(*min) * 0.5);
        }
    }

    /**
     * True when the reading violates its limits: fans below min, temperatures above
     * max, electrical values outside [min, max].
     */
    [[nodiscard]] inline bool isAlertState(const SensorReading &reading) {
        const bool belowMin = reading.minValue && reading.value < *reading.minValue;
        const bool aboveMax = reading.maxValue && reading.value > *reading.maxValue;
        switch (reading.unit) {
            case SensorUnit::Rpm:
                return belowMin;
            case SensorUnit::Celsius:
            case SensorUnit::Fahrenheit:
                return aboveMax;
            case SensorUnit::Volt:
            case SensorUnit::Ampere:
            case SensorUnit::Milliampere:
            case SensorUnit::Watt:
            case SensorUnit::Milliwatt:
                return belowMin || aboveMax;
            case SensorUnit::Unknown:
                break;
        }
        return false;
    }

    /** Position of the value within its limits (0..1); nullopt without limits or value. */
    [[nodiscard]] inline std::optional<double> rangeFraction(const SensorReading &reading) {
        if (!reading.hasRange() || !std::isfinite(reading.value)) {
            return std::nullopt;
        }
        const double min = reading.minValue.value_or(reading.value);
        double max = reading.maxValue.value_or(min + 1.0);
        if (!(max > min)) {
            max = min + 1.0;
        }
        return std::clamp((reading.value - min) / (max - min), 0.0, 1.0);
    }
}
