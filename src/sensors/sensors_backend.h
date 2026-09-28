// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "config/runtime_config.h"
#include "sensors/sensor_reading.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

/** libsensors wrapper: initializes the library and reads all chips into SensorReadings. */
class SensorsBackend {
public:
    /** Initializes libsensors; on failure isInitialized() is false and lastError() explains why. */
    SensorsBackend();

    /** Releases libsensors if this instance initialized it. */
    ~SensorsBackend();

    SensorsBackend(const SensorsBackend &) = delete;

    SensorsBackend &operator=(const SensorsBackend &) = delete;

    /** True when libsensors init succeeded and reads are valid. */
    [[nodiscard]] bool isInitialized() const;

    /** Human-readable backend init error. */
    [[nodiscard]] QString lastError() const;

    /** All readable sensors; keeps per-sensor unit scale state between calls. */
    [[nodiscard]] QVector<SensorReading> readAll(int defaultFanMaxRpm, TemperatureUnit temperatureUnit);

private:
    bool m_initialized;
    QString m_lastError;
    // Current unit scale (true = mA/mW) per "chip:feature" for sensors without native limits.
    QHash<QString, bool> m_milliScaleLatch;
    // Problems already logged, so a failing sensor does not flood the log every poll.
    QSet<QString> m_reportedProblems;
};
