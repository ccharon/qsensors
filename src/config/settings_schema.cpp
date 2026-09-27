// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "settings_schema.h"
#include "runtime_config.h"
#include "settings_keys.h"

#include <QSettings>
#include <QDebug>

namespace {
    void migrateV0ToV1(QSettings &) {
        // v1 only introduces the version marker; no keys change.
    }

    void migrateV1ToV2(QSettings &settings) {
        // v2 introduces runtime/temperature_unit with explicit default.
        if (!settings.contains(SettingsKeys::kTemperatureUnit)) {
            settings.setValue(SettingsKeys::kTemperatureUnit, temperatureUnitToToken(TemperatureUnit::Celsius));
        }
    }
}

int SettingsSchema::storedVersion(const QSettings &settings) {
    return settings.value(SettingsKeys::kSchemaVersion, 0).toInt();
}

void SettingsSchema::ensureUpToDate(QSettings &settings) {
    const int version = storedVersion(settings);
    if (version > kCurrentVersion) {
        qWarning("qsensors: settings schema v%d is newer than supported v%d; keeping it unchanged",
                 version, kCurrentVersion);
        return;
    }

    if (version < 1) {
        migrateV0ToV1(settings);
    }
    if (version < 2) {
        migrateV1ToV2(settings);
    }

    settings.setValue(SettingsKeys::kSchemaVersion, kCurrentVersion);
}
