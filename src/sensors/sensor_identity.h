// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors_backend.h"

#include <QString>
#include <QStringList>
#include <QVector>

// Free functions for stable identification of sensors and chips.
// Kept separate from SensorsPanel so tests can use them without UI dependencies.

/** Stable fingerprint of the chip set; changes when chips appear or disappear. */
[[nodiscard]] inline QString chipFingerprint(const QVector<SensorReading> &readings) {
    QStringList chips;
    chips.reserve(readings.size());
    for (const SensorReading &r: readings)
        chips.push_back(r.chip);
    chips.removeDuplicates();
    chips.sort();
    return chips.join(QStringLiteral("\n"));
}

namespace SensorIdentity {
    /** Unique widget lookup key for a single sensor reading. */
    [[nodiscard]] inline QString sensorKey(const SensorReading &reading) {
        return reading.chip + QStringLiteral("|")
               + QString::number(static_cast<int>(reading.category)) + QStringLiteral("|")
               + QString::number(reading.featureNumber) + QStringLiteral("|")
               + QString::number(reading.subfeatureNumber) + QStringLiteral("|")
               + QString::number(static_cast<int>(reading.unit)) + QStringLiteral("|")
               + reading.feature;
    }
}
