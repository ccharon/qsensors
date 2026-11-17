// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "config/runtime_config.h"
#include "sensors/sensor_reading.h"
#include "sensors/sensors_policy.h"

#include <QHash>
#include <QString>
#include <QVector>

/**
 * Turns raw readings of a SensorSource into display readings: drops placeholder
 * firmware limits, fills the bar graph range (default ranges where firmware has no
 * limits), converts the temperature unit and chooses the display scale (SI prefix and
 * decimals). Values and limits stay in the base unit. The firmware limits are kept apart; they
 * alone raise alerts. Readings without a value pass through with their limits.
 */
class ReadingPipeline {
public:
    /**
     * Display readings for @p raw. Keeps the display scale per sensor between calls;
     * processing the same raw readings again gives the same result.
     */
    [[nodiscard]] QVector<SensorReading> process(const QVector<SensorReading> &raw, const RuntimeConfig &config);

    /** Number of sensors with a kept display scale; sensors missing from a non-empty read are dropped. */
    [[nodiscard]] qsizetype latchedSensorCount() const;

private:
    // Scale range per "chip:feature", see SensorsPolicy::chooseScale(); kept with its unit,
    // so a °C/°F switch starts over.
    struct LatchedScale {
        SensorUnit unit = SensorUnit::Unknown;
        SensorsPolicy::ScaleLatch latch;
    };
    QHash<QString, LatchedScale> m_scaleLatch;
};
