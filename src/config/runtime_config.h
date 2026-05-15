// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

namespace RuntimeConfigLimits {
    constexpr int kDefaultPollingIntervalSec = 2;
    constexpr int kMinPollingIntervalSec = 1;
    constexpr int kMaxPollingIntervalSec = 10;

    constexpr int kDefaultFanDefaultMaxRpm = 5000;
    constexpr int kMinFanDefaultMaxRpm = 1000;
    constexpr int kMaxFanDefaultMaxRpm = 50000;
}

struct RuntimeConfig {
    int pollingIntervalSec = RuntimeConfigLimits::kDefaultPollingIntervalSec;
    int fanDefaultMaxRpm = RuntimeConfigLimits::kDefaultFanDefaultMaxRpm;
};

