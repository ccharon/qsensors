// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <optional>

class QString;
class QStringView;

// User-adjustable runtime settings with their limits and defaults.

/** Unit for displayed temperatures; libsensors always reports Celsius. */
enum class TemperatureUnit {
    Celsius = 'C',
    Fahrenheit = 'F'
};

/** Parses the persisted token "C" or "F" (case and surrounding spaces ignored); nullopt otherwise. */
[[nodiscard]] std::optional<TemperatureUnit> temperatureUnitFromToken(QStringView token);

/** Persisted token for @p unit: "C" or "F". */
[[nodiscard]] QString temperatureUnitToToken(TemperatureUnit unit);

/** Bounds and defaults shared by settings loading and the settings panel. */
namespace RuntimeConfigLimits {
    constexpr int kDefaultPollingIntervalSec = 2;
    constexpr int kMinPollingIntervalSec = 1;
    constexpr int kMaxPollingIntervalSec = 10;

    constexpr int kDefaultFanDefaultMaxRpm = 5000;
    constexpr int kMinFanDefaultMaxRpm = 500;
    constexpr int kMaxFanDefaultMaxRpm = 9999;
}

/** Settings that change polling and display at runtime. */
struct RuntimeConfig {
    int pollingIntervalSec = RuntimeConfigLimits::kDefaultPollingIntervalSec;
    // Upper bar-graph bound for fans whose firmware reports no maximum.
    int fanDefaultMaxRpm = RuntimeConfigLimits::kDefaultFanDefaultMaxRpm;
    TemperatureUnit temperatureUnit = TemperatureUnit::Celsius;
};
