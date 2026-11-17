// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "config/runtime_config.h"
#include "sensors/sensor_reading.h"

#include <QHash>
#include <QString>
#include <QVector>

/**
 * Turns raw readings of a SensorSource into display readings: drops placeholder
 * firmware limits, the display scale (SI prefix and decimals), the bar graph range (default ranges where firmware
 * has no limits), and the temperature unit. The firmware limits are kept apart; they
 * alone raise alerts. Readings without a value pass through with their limits.
 */
class ReadingPipeline {
public:
    /**
     * Display readings for @p raw. Keeps the display prefix per sensor between calls;
     * processing the same raw readings again gives the same result.
     */
    [[nodiscard]] QVector<SensorReading> process(const QVector<SensorReading> &raw, const RuntimeConfig &config);

private:
    // Current display prefix per "chip:feature" for sensors without native limits.
    QHash<QString, SiPrefix> m_prefixLatch;
};
