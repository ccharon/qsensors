// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "app_config_store.h"
#include "settings_schema.h"

#include <QSettings>

RuntimeConfig AppConfigStore::loadRuntimeConfig() {
    RuntimeConfig config;
    QSettings settings;
    SettingsSchema::ensureUpToDate(settings);

    config.pollingIntervalSec = settings.value(QStringLiteral("runtime/polling_interval_sec"),RuntimeConfigLimits::kDefaultPollingIntervalSec).toInt();

    if (config.pollingIntervalSec < RuntimeConfigLimits::kMinPollingIntervalSec ||
        config.pollingIntervalSec > RuntimeConfigLimits::kMaxPollingIntervalSec) {
        config.pollingIntervalSec = RuntimeConfigLimits::kDefaultPollingIntervalSec;
    }

    config.fanDefaultMaxRpm = settings.value(QStringLiteral("runtime/fan_default_max_rpm"),RuntimeConfigLimits::kDefaultFanDefaultMaxRpm).toInt();

    if (config.fanDefaultMaxRpm < RuntimeConfigLimits::kMinFanDefaultMaxRpm ||
        config.fanDefaultMaxRpm > RuntimeConfigLimits::kMaxFanDefaultMaxRpm) {
        config.fanDefaultMaxRpm = RuntimeConfigLimits::kDefaultFanDefaultMaxRpm;
    }

    const QString temperatureUnitRaw = settings.value(QStringLiteral("runtime/temperature_unit"),QStringLiteral("C")).toString().trimmed().toUpper();
    config.temperatureUnit = temperatureUnitFromToken(QStringView(temperatureUnitRaw));

    return config;
}

void AppConfigStore::saveRuntimeConfig(const RuntimeConfig &config) {
    QSettings settings;

    SettingsSchema::ensureUpToDate(settings);
    settings.setValue(QStringLiteral("runtime/polling_interval_sec"), config.pollingIntervalSec);
    settings.setValue(QStringLiteral("runtime/fan_default_max_rpm"), config.fanDefaultMaxRpm);
    settings.setValue(QStringLiteral("runtime/temperature_unit"),temperatureUnitToToken(config.temperatureUnit));
}
