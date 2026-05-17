// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "settings_schema.h"

#include <QSettings>

namespace {
    void migrateV0ToV1(QSettings &settings) {
        // v1 establishes explicit schema versioning.
        // No key migration required yet.
        settings.sync();
    }

    void migrateV1ToV2(QSettings &settings) {
        // v2 introduces runtime/temperature_unit with explicit default.
        if (!settings.contains(QStringLiteral("runtime/temperature_unit"))) {
            settings.setValue(QStringLiteral("runtime/temperature_unit"), QStringLiteral("C"));
        }
        settings.sync();
    }
}

void SettingsSchema::ensureUpToDate(QSettings &settings) {
    const int version = settings.value(QStringLiteral("meta/schema_version"), 0).toInt();

    if (version < 1) {
        migrateV0ToV1(settings);
    }
    if (version < 2) {
        migrateV1ToV2(settings);
    }

    settings.setValue(QStringLiteral("meta/schema_version"), kCurrentVersion);
}
