// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "main_window_state_store.h"
#include "settings_keys.h"
#include "settings_schema.h"

#include <QSettings>
#include <QStringList>
#include <QUrl>
#include <QDebug>

namespace {
    // Chip names may contain '/' which QSettings interprets as a group separator.
    // Percent-encoding is idempotent for normal names (isa-0000, pci-0000).
    QString encodeChipKey(const QString &chip) {
        return QString::fromLatin1(QUrl::toPercentEncoding(chip));
    }
    QString decodeChipKey(const QString &key) {
        return QUrl::fromPercentEncoding(key.toLatin1());
    }
}

MainWindowState MainWindowStateStore::load() {
    MainWindowState state;
    QSettings settings;
    SettingsSchema::ensureUpToDate(settings);

    state.geometry = settings.value(SettingsKeys::kWindowGeometry).toByteArray();

    settings.beginGroup(SettingsKeys::kChipExpandedGroup);
    const QStringList keys = settings.childKeys();
    for (const QString &key: keys) {
        state.chipExpanded.insert(decodeChipKey(key), settings.value(key, true).toBool());
    }
    settings.endGroup();

    state.chipOrder = settings.value(SettingsKeys::kChipOrder).toStringList();
    state.sensorFingerprint = settings.value(SettingsKeys::kSensorFingerprint).toString();
    return state;
}

bool MainWindowStateStore::save(const MainWindowState &state) {
    QSettings settings;
    SettingsSchema::ensureUpToDate(settings);
    settings.setValue(SettingsKeys::kWindowGeometry, state.geometry);
    settings.setValue(SettingsKeys::kSensorFingerprint, state.sensorFingerprint);
    settings.setValue(SettingsKeys::kChipOrder, state.chipOrder);
    settings.beginGroup(SettingsKeys::kChipExpandedGroup);
    settings.remove(QString());
    for (auto it = state.chipExpanded.constBegin(); it != state.chipExpanded.constEnd(); ++it) {
        settings.setValue(encodeChipKey(it.key()), it.value());
    }
    settings.endGroup();
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qWarning("qsensors: could not write window state to %s", qPrintable(settings.fileName()));
        return false;
    }
    return true;
}
