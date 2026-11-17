// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors/hwmon_watch.h"
#include "sensors/sensor_source.h"

#include <QSet>
#include <QString>
#include <QVector>

/**
 * libsensors wrapper: initializes the library and reads all chips into raw SensorReadings.
 * libsensors enumerates chips once, in sensors_init(); when the hwmon devices change, the
 * next readAll() re-initializes it. A failed re-init leaves the source uninitialized,
 * with lastError() explaining why.
 */
class LibsensorsSource final : public SensorSource {
public:
    /** Initializes libsensors; on failure isInitialized() is false and lastError() explains why. */
    LibsensorsSource();

    /** Releases libsensors if this instance initialized it. */
    ~LibsensorsSource() override;

    LibsensorsSource(const LibsensorsSource &) = delete;

    LibsensorsSource &operator=(const LibsensorsSource &) = delete;

    [[nodiscard]] bool isInitialized() const override;

    [[nodiscard]] QString lastError() const override;

    [[nodiscard]] QVector<SensorReading> readAll() override;

private:
    /** sensors_cleanup() + sensors_init(); false (and uninitialized) when the init fails. */
    bool reinitialize();

    HwmonWatch m_hwmonWatch;
    bool m_initialized;
    QString m_lastError;
    // Problems already logged, so a failing sensor does not flood the log every poll.
    QSet<QString> m_reportedProblems;
};
