// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors/sensor_reading.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <utility>

/** Stable identification of sensors and chips, independent of the UI. */
namespace SensorIdentity {
    /**
     * Names for the chips of one read, in the order of @p names. A unique name stays as it
     * is, so saved chip order and expand state keep working. Chips sharing a name (virtual
     * and ACPI chips all have address 0, e.g. two "acpi_fan-acpi-0") are ordered by
     * @p stableKey (a device path; asked for repeated names only): the first keeps the
     * name, the others get " #2", " #3", ... That is stable across polls, and across
     * restarts as long as the same devices are present.
     */
    template<typename StableKey>
    [[nodiscard]] QStringList uniqueChipNames(const QStringList &names, StableKey &&stableKey) {
        QHash<QString, QVector<qsizetype>> byName;
        for (qsizetype i = 0; i < names.size(); ++i)
            byName[names.at(i)].push_back(i);

        QStringList result = names;
        QSet<QString> used(names.cbegin(), names.cend());
        for (auto it = byName.cbegin(); it != byName.cend(); ++it) {
            if (it->size() < 2)
                continue;
            QVector<qsizetype> group = *it;
            QHash<qsizetype, QString> keys;
            for (const qsizetype i: group)
                keys.insert(i, stableKey(i));
            // Equal keys keep the listing order.
            std::stable_sort(group.begin(), group.end(),
                             [&keys](const qsizetype a, const qsizetype b) { return keys.value(a) < keys.value(b); });
            int suffix = 1;
            for (qsizetype g = 1; g < group.size(); ++g) {
                QString candidate;
                do {
                    candidate = it.key() + QStringLiteral(" #") + QString::number(++suffix);
                } while (used.contains(candidate)); // never take a name another chip already has
                used.insert(candidate);
                result[group.at(g)] = candidate;
            }
        }
        return result;
    }

    /**
     * Widget lookup key built from libsensors numbering only. Unit and label are
     * left out, so a scale or unit switch (mA/A, °C/°F) is a value update, not a rebuild.
     */
    [[nodiscard]] inline QString sensorKey(const SensorReading &reading) {
        return reading.chip + QLatin1Char('|')
               + QString::number(static_cast<int>(reading.category)) + QLatin1Char('|')
               + QString::number(reading.featureNumber) + QLatin1Char('|')
               + QString::number(reading.subfeatureNumber);
    }
}
