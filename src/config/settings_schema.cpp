// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "config/settings_schema.h"
#include "config/runtime_config.h"
#include "config/settings_keys.h"

#include <QSettings>
#include <QDebug>
#include <algorithm>

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
    return std::max(0, settings.value(SettingsKeys::kSchemaVersion, 0).toInt());
}

bool SettingsSchema::ensureUpToDate(QSettings &settings) {
    const int version = storedVersion(settings);
    const QVariant rawVersion = settings.value(SettingsKeys::kSchemaVersion);
    bool numeric = false;
    const int rawNumber = rawVersion.toInt(&numeric);
    if (rawVersion.isValid() && (!numeric || rawNumber < 0)) {
        qWarning("qsensors: unreadable settings schema version '%s', treating the settings as unversioned",
                 qPrintable(rawVersion.toString()));
    }
    if (version > kCurrentVersion) {
        qWarning("qsensors: settings schema v%d is newer than supported v%d; keeping it unchanged",
                 version, kCurrentVersion);
        return true;
    }

    if (version == kCurrentVersion) {
        return true;
    }

    // Settings without any key are a first start, not a migration.
    const bool existing = !settings.allKeys().isEmpty();
    if (version < 1) {
        migrateV0ToV1(settings);
    }
    if (version < 2) {
        migrateV1ToV2(settings);
    }

    settings.setValue(SettingsKeys::kSchemaVersion, kCurrentVersion);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qWarning("qsensors: could not write the settings schema version to %s", qPrintable(settings.fileName()));
        return false;
    }
    if (existing) {
        qInfo("qsensors: migrated settings in %s from schema v%d to v%d",
              qPrintable(settings.fileName()), version, kCurrentVersion);
    }
    return true;
}
