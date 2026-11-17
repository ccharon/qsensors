// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/hwmon_watch.h"

#include <QDir>
#include <QFileInfo>

#include <utility>

HwmonWatch::HwmonWatch(QString directory) : m_directory(std::move(directory)) {}

void HwmonWatch::reset() {
    m_snapshot = snapshot();
}

bool HwmonWatch::changed() {
    QStringList current = snapshot();
    if (current == m_snapshot)
        return false;
    m_snapshot = std::move(current);
    return true;
}

QStringList HwmonWatch::snapshot() const {
    // A missing directory gives an empty, stable snapshot.
    const QFileInfoList entries = QDir(m_directory).entryInfoList(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot,
                                                                  QDir::Name);
    QStringList result;
    result.reserve(entries.size());
    for (const QFileInfo &entry: entries)
        result.push_back(entry.fileName() + QLatin1Char('\n') + entry.symLinkTarget());
    return result;
}
