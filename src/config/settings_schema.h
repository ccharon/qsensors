// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

class QSettings;

/** Versioned layout of the persisted settings and its migrations. */
namespace SettingsSchema {
    /** Schema version written by this build. */
    constexpr int kCurrentVersion = 2;

    /** Schema version stored in @p settings; 0 when none was written yet. */
    [[nodiscard]] int storedVersion(const QSettings &settings);

    /**
     * Migrates older settings to kCurrentVersion and logs the migration; new settings
     * just get the version marker. Settings from a newer qsensors are left untouched
     * and logged, so their version marker is never downgraded. Runs once at startup,
     * before any settings are read; the stores do not migrate.
     */
    void ensureUpToDate(QSettings &settings);
}
