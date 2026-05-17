// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

class QSettings;

namespace SettingsSchema {
    constexpr int kCurrentVersion = 1;

    /** Ensures persisted settings are migrated to the current schema version. */
    void ensureUpToDate(QSettings &settings);
}

