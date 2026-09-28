// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "config/app_config_store.h"
#include "config/settings_keys.h"

#include <QSettings>
#include <QDebug>
#include <algorithm>

namespace {
    int readBoundedInt(const QSettings &settings, const QString &key, const int fallback, const int min, const int max) {
        const QVariant raw = settings.value(key);
        if (!raw.isValid())
            return fallback;
        bool ok = false;
        const int value = raw.toInt(&ok);
        if (!ok) {
            qWarning("qsensors: invalid setting %s=%s, using default %d",
                     qPrintable(key), qPrintable(raw.toString()), fallback);
            return fallback;
        }
        if (value < min || value > max)
            qWarning("qsensors: setting %s=%d out of range [%d, %d], clamping", qPrintable(key), value, min, max);
        return std::clamp(value, min, max);
    }
}

RuntimeConfig AppConfigStore::loadRuntimeConfig() {
    QSettings settings;
    if (settings.status() != QSettings::NoError)
        qWarning("qsensors: settings file %s is unreadable, using defaults", qPrintable(settings.fileName()));

    RuntimeConfig config;
    config.pollingIntervalSec = readBoundedInt(settings, SettingsKeys::kPollingIntervalSec,
                                               RuntimeConfigLimits::kDefaultPollingIntervalSec,
                                               RuntimeConfigLimits::kMinPollingIntervalSec,
                                               RuntimeConfigLimits::kMaxPollingIntervalSec);
    config.fanDefaultMaxRpm = readBoundedInt(settings, SettingsKeys::kFanDefaultMaxRpm,
                                             RuntimeConfigLimits::kDefaultFanDefaultMaxRpm,
                                             RuntimeConfigLimits::kMinFanDefaultMaxRpm,
                                             RuntimeConfigLimits::kMaxFanDefaultMaxRpm);

    const QVariant rawUnit = settings.value(SettingsKeys::kTemperatureUnit);
    if (rawUnit.isValid()) {
        const QString token = rawUnit.toString();
        if (const auto unit = temperatureUnitFromToken(token)) {
            config.temperatureUnit = *unit;
        } else {
            qWarning("qsensors: invalid setting %s=%s, using Celsius", qPrintable(SettingsKeys::kTemperatureUnit), qPrintable(token));
        }
    }
    return config;
}

bool AppConfigStore::saveRuntimeConfig(const RuntimeConfig &config) {
    QSettings settings;
    settings.setValue(SettingsKeys::kPollingIntervalSec, config.pollingIntervalSec);
    settings.setValue(SettingsKeys::kFanDefaultMaxRpm, config.fanDefaultMaxRpm);
    settings.setValue(SettingsKeys::kTemperatureUnit, temperatureUnitToToken(config.temperatureUnit));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qWarning("qsensors: could not write settings to %s", qPrintable(settings.fileName()));
        return false;
    }
    return true;
}
