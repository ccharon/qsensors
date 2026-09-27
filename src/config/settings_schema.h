// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

class QSettings;

namespace SettingsSchema {
    constexpr int kCurrentVersion = 2;

    /** Schema version stored in @p settings; 0 when none was written yet. */
    [[nodiscard]] int storedVersion(const QSettings &settings);

    /**
     * Migrates older settings to kCurrentVersion. Settings from a newer qsensors
     * are left untouched and logged, so their version marker is never downgraded.
     */
    void ensureUpToDate(QSettings &settings);
}
