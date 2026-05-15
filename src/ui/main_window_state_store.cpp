// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "main_window_state_store.h"

#include <QSettings>
#include <QStringList>

MainWindowState MainWindowStateStore::load() {
    MainWindowState state;
    QSettings settings;

    state.geometry = settings.value(QStringLiteral("ui/geometry")).toByteArray();
    state.hasGeometry = !state.geometry.isEmpty();

    settings.beginGroup(QStringLiteral("ui/chips"));
    const QStringList keys = settings.childKeys();
    for (const QString &key: keys) {
        state.chipExpanded.insert(key, settings.value(key, true).toBool());
    }
    settings.endGroup();

    state.sensorFingerprint = settings.value(QStringLiteral("sensors/fingerprint")).toString();
    return state;
}

void MainWindowStateStore::save(
    const QByteArray &geometry,
    const QString &sensorFingerprint,
    const QHash<QString, bool> &chipExpanded
) {
    QSettings settings;
    settings.setValue(QStringLiteral("ui/geometry"), geometry);
    settings.setValue(QStringLiteral("sensors/fingerprint"), sensorFingerprint);
    settings.beginGroup(QStringLiteral("ui/chips"));
    settings.remove(QString());
    for (auto it = chipExpanded.constBegin(); it != chipExpanded.constEnd(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
}
