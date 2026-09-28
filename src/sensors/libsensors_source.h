// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors/sensor_source.h"

#include <QSet>
#include <QString>
#include <QVector>

/** libsensors wrapper: initializes the library and reads all chips into raw SensorReadings. */
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
    bool m_initialized;
    QString m_lastError;
    // Problems already logged, so a failing sensor does not flood the log every poll.
    QSet<QString> m_reportedProblems;
};
