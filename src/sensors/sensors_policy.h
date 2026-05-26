// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors_backend.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace SensorsPolicy {
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
            if (!min.has_value() && !max.has_value()) {
                min = 0.0;
                max = std::max(1.0, std::abs(measuredValue) * 1.5);
            } else if (!min.has_value() && max.has_value()) {
                min = std::max(0.0, *max * 0.2);
            } else if (min.has_value() && !max.has_value()) {
                max = std::max(measuredValue, *min + std::max(1.0, std::abs(*min) * 0.5));
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
