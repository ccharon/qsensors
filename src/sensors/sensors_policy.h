// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors_backend.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace SensorsPolicy {
    /** Milli-unit readings at or above this no longer fit the six-cell LCD value ("9999.9"). */
    inline constexpr double kMilliUnitOverflow = 10000.0;
    /** Base-unit readings below this lose too much precision at two decimals ("0.04"). */
    inline constexpr double kBaseUnitUnderflow = 0.1;

    /**
     * Rescales Ampere/Watt readings to milli-units when the sensor's own scale is sub-1.
     *
     * With native limits the decision uses the native max (then min), which is stable by
     * itself. Without limits the live value decides with wide hysteresis: the current scale
     * is kept in @p latchedMilli (per sensor, by the caller). Milli is left only when the
     * value would overflow the display (>= 10 A/W), base only when two decimals get too
     * coarse (< 0.1 A/W). Values hovering around 1.0 therefore never flip between e.g.
     * "982.7 mW" and "1.25 W", and a startup load spike does not decide for good.
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
            // Currents and power are signed (e.g. battery charging vs. discharging): a
            // negative reading is a direction, not a fault. Without a native minimum the
            // synthetic range is mirrored below zero instead of flagging an alert.
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
}
