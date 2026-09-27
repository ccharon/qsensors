// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

// Persistence of window geometry, chip order and per-chip expand state.

/** Window state as loaded from QSettings. */
struct MainWindowState {
    QByteArray geometry;
    QHash<QString, bool> chipExpanded;
    QStringList chipOrder; // user-arranged order; empty means alphabetical
    QString sensorFingerprint; // chip set the expand state belongs to
};

/** Reads/writes persisted main-window state via QSettings. */
namespace MainWindowStateStore {
    /** Loads the stored window state; missing entries stay empty. */
    [[nodiscard]] MainWindowState load();

    /** Writes the window state to disk; returns false (and logs) if it could not be stored. */
    [[nodiscard]] bool save(const MainWindowState &state);
}
