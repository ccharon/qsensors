// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "app_config_store.h"
#include "settings_schema.h"

#include <QSettings>
#include <QDebug>
#include <algorithm>

namespace {
    const QString kPollingIntervalKey = QStringLiteral("runtime/polling_interval_sec");
    const QString kFanDefaultMaxRpmKey = QStringLiteral("runtime/fan_default_max_rpm");
    const QString kTemperatureUnitKey = QStringLiteral("runtime/temperature_unit");

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
    SettingsSchema::ensureUpToDate(settings);

    RuntimeConfig config;
    config.pollingIntervalSec = readBoundedInt(settings, kPollingIntervalKey,
                                               RuntimeConfigLimits::kDefaultPollingIntervalSec,
                                               RuntimeConfigLimits::kMinPollingIntervalSec,
                                               RuntimeConfigLimits::kMaxPollingIntervalSec);
    config.fanDefaultMaxRpm = readBoundedInt(settings, kFanDefaultMaxRpmKey,
                                             RuntimeConfigLimits::kDefaultFanDefaultMaxRpm,
                                             RuntimeConfigLimits::kMinFanDefaultMaxRpm,
                                             RuntimeConfigLimits::kMaxFanDefaultMaxRpm);

    const QVariant rawUnit = settings.value(kTemperatureUnitKey);
    if (rawUnit.isValid()) {
        const QString token = rawUnit.toString();
        if (const auto unit = temperatureUnitFromToken(token)) {
            config.temperatureUnit = *unit;
        } else {
            qWarning("qsensors: invalid setting %s=%s, using Celsius", qPrintable(kTemperatureUnitKey), qPrintable(token));
        }
    }
    return config;
}

bool AppConfigStore::saveRuntimeConfig(const RuntimeConfig &config) {
    QSettings settings;
    SettingsSchema::ensureUpToDate(settings);
    settings.setValue(kPollingIntervalKey, config.pollingIntervalSec);
    settings.setValue(kFanDefaultMaxRpmKey, config.fanDefaultMaxRpm);
    settings.setValue(kTemperatureUnitKey, temperatureUnitToToken(config.temperatureUnit));
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qWarning("qsensors: could not write settings to %s", qPrintable(settings.fileName()));
        return false;
    }
    return true;
}
