// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QString>
#include <QStringList>

/**
 * Notices when hwmon devices appear, disappear or are replaced, by comparing a
 * snapshot of the entries in the hwmon class directory (name and symlink target,
 * so a reused hwmonN index of another device counts as a change). Cheap enough to
 * check on every poll: one directory listing, no device I/O.
 */
class HwmonWatch {
public:
    static constexpr const char *kDefaultDirectory = "/sys/class/hwmon";

    /** Watches @p directory; call reset() to take the first snapshot. */
    explicit HwmonWatch(QString directory = QString::fromLatin1(kDefaultDirectory));

    /** Takes a new snapshot; the next changed() compares against it. */
    void reset();

    /** True when the entries differ from the last snapshot; the new state becomes the snapshot. */
    [[nodiscard]] bool changed();

private:
    [[nodiscard]] QStringList snapshot() const;

    QString m_directory;
    QStringList m_snapshot;
};
