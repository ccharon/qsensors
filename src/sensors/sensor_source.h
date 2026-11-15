// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors/sensor_reading.h"

#include <QString>
#include <QVector>

/**
 * Hardware access behind the readings. A source reports raw values only: native
 * units (°C, V, RPM, A, W) and the firmware limits in firmwareMin/firmwareMax;
 * ReadingPipeline prepares them for display and fills the bar graph range.
 */
class SensorSource {
public:
    virtual ~SensorSource() = default;

    /** True when the source is usable and readAll() returns valid readings. */
    [[nodiscard]] virtual bool isInitialized() const = 0;

    /** Human-readable initialization error. */
    [[nodiscard]] virtual QString lastError() const = 0;

    /** All readable sensors with raw values and native limits. */
    [[nodiscard]] virtual QVector<SensorReading> readAll() = 0;
};
