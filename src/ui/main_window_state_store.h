// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

struct MainWindowState {
    QByteArray geometry;
    QHash<QString, bool> chipExpanded;
    QString sensorFingerprint;
    bool hasGeometry = false;
};

/** Reads/writes persisted main-window state via QSettings. */
class MainWindowStateStore final {
public:
    [[nodiscard]] static MainWindowState load();

    static void save(
        const QByteArray &geometry,
        const QString &sensorFingerprint,
        const QHash<QString, bool> &chipExpanded
    );
};
