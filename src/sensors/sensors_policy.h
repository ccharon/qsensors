// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors_backend.h"

#include <optional>

namespace SensorsPolicy {
/** Applies qsensors default range policy when firmware does not expose limits. */
inline void applyDefaultRangePolicy(
    const SensorCategory category,
    std::optional<double> &min,
    std::optional<double> &max
) {
    if (category == SensorCategory::Temperatures) {
        if (!min.has_value() && !max.has_value()) {
            min = 0.0;
            max = 100.0;
        } else if (!min.has_value() && max.has_value()) {
            min = 0.0;
        }
    }

    if (category == SensorCategory::Fans) {
        if (!min.has_value()) {
            min = 0.0;
        }
        if (!max.has_value()) {
            max = 10000.0;
        }
    }
}
}
