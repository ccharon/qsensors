// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "app_config_store.h"

#include <QSettings>

RuntimeConfig AppConfigStore::loadRuntimeConfig() {
    RuntimeConfig config;
    QSettings settings;

    // Keep stable keys for backward compatibility with existing user settings.
    config.pollingIntervalSec = settings.value(
        QStringLiteral("ui/polling_interval_sec"),
        RuntimeConfigLimits::kDefaultPollingIntervalSec
    ).toInt();
    if (config.pollingIntervalSec < RuntimeConfigLimits::kMinPollingIntervalSec ||
        config.pollingIntervalSec > RuntimeConfigLimits::kMaxPollingIntervalSec) {
        config.pollingIntervalSec = RuntimeConfigLimits::kDefaultPollingIntervalSec;
    }

    config.fanDefaultMaxRpm = settings.value(
        QStringLiteral("ui/fan_default_max_rpm"),
        RuntimeConfigLimits::kDefaultFanDefaultMaxRpm
    ).toInt();
    if (config.fanDefaultMaxRpm < RuntimeConfigLimits::kMinFanDefaultMaxRpm ||
        config.fanDefaultMaxRpm > RuntimeConfigLimits::kMaxFanDefaultMaxRpm) {
        config.fanDefaultMaxRpm = RuntimeConfigLimits::kDefaultFanDefaultMaxRpm;
    }

    return config;
}

void AppConfigStore::saveRuntimeConfig(const RuntimeConfig &config) {
    QSettings settings;
    // Store only runtime-tunable values; window/chip UI state lives in MainWindowStateStore.
    settings.setValue(QStringLiteral("ui/polling_interval_sec"), config.pollingIntervalSec);
    settings.setValue(QStringLiteral("ui/fan_default_max_rpm"), config.fanDefaultMaxRpm);
}
