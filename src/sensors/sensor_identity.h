// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensor_reading.h"

#include <QString>
#include <QStringList>
#include <QVector>

/** Stable identification of sensors and chips, independent of the UI. */
namespace SensorIdentity {
    /** Fingerprint of the chip set; changes when chips appear or disappear. */
    [[nodiscard]] inline QString chipFingerprint(const QVector<SensorReading> &readings) {
        QStringList chips;
        chips.reserve(readings.size());
        for (const SensorReading &r: readings)
            chips.push_back(r.chip);
        chips.removeDuplicates();
        chips.sort();
        return chips.join(QLatin1Char('\n'));
    }

    /**
     * Widget lookup key built from libsensors numbering only. Unit and label are
     * left out, so a unit switch (A/mA, °C/°F) is a value update, not a rebuild.
     */
    [[nodiscard]] inline QString sensorKey(const SensorReading &reading) {
        return reading.chip + QLatin1Char('|')
               + QString::number(static_cast<int>(reading.category)) + QLatin1Char('|')
               + QString::number(reading.featureNumber) + QLatin1Char('|')
               + QString::number(reading.subfeatureNumber);
    }
}
