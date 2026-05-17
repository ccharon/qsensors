// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

class QString;
class QStringView;

enum class TemperatureUnit {
    Celsius = 'C',
    Fahrenheit = 'F'
};

[[nodiscard]] TemperatureUnit temperatureUnitFromToken(QStringView token);
[[nodiscard]] QString temperatureUnitToToken(TemperatureUnit unit);

namespace RuntimeConfigLimits {
    constexpr int kDefaultPollingIntervalSec = 2;
    constexpr int kMinPollingIntervalSec = 1;
    constexpr int kMaxPollingIntervalSec = 10;

    constexpr int kDefaultFanDefaultMaxRpm = 5000;
    constexpr int kMinFanDefaultMaxRpm = 500;
    constexpr int kMaxFanDefaultMaxRpm = 9999;
}

struct RuntimeConfig {
    int pollingIntervalSec = RuntimeConfigLimits::kDefaultPollingIntervalSec;
    int fanDefaultMaxRpm = RuntimeConfigLimits::kDefaultFanDefaultMaxRpm;
    TemperatureUnit temperatureUnit = TemperatureUnit::Celsius;
};
