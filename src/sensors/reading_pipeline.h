// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "config/runtime_config.h"
#include "sensors/sensor_reading.h"

#include <QHash>
#include <QString>
#include <QVector>

/**
 * Turns raw readings of a SensorSource into display readings: mA/mW scaling,
 * the bar graph range (default ranges where firmware has no limits), and the
 * temperature unit. The firmware limits are kept apart; they alone raise alerts.
 */
class ReadingPipeline {
public:
    /**
     * Display readings for @p raw. Keeps the mA/mW scale per sensor between calls;
     * processing the same raw readings again gives the same result.
     */
    [[nodiscard]] QVector<SensorReading> process(const QVector<SensorReading> &raw, const RuntimeConfig &config);

private:
    // Current unit scale (true = mA/mW) per "chip:feature" for sensors without native limits.
    QHash<QString, bool> m_milliScaleLatch;
};
