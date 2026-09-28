// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "config/runtime_config.h"
#include "sensors/reading_pipeline.h"
#include "sensors/sensor_reading.h"
#include "sensors/sensor_source.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>

class QThread;
class QTimer;
class SensorPoller;

/**
 * Polls a SensorSource in a worker thread, so slow drivers never block the UI, and
 * emits display readings in the thread of the monitor. The source is created, read
 * and destroyed in the worker thread only.
 */
class SensorMonitor final : public QObject {
    Q_OBJECT

public:
    /** Creates the source; called once, in the worker thread. */
    using SourceFactory = std::function<std::unique_ptr<SensorSource>()>;

    /** Creates an idle monitor; nothing is read before start(). */
    explicit SensorMonitor(SourceFactory factory, QObject *parent = nullptr);

    /** Stops polling and waits for the worker, which destroys the source. */
    ~SensorMonitor() override;

    /**
     * Creates the source and reads it once, blocking, so readingsChanged() is emitted
     * before this returns; then polls with the configured interval. Returns false when
     * the source could not be initialized (see lastError()); nothing is polled then.
     * Call once.
     */
    bool start(const RuntimeConfig &config);

    /** Initialization error of the source; empty after a successful start(). */
    [[nodiscard]] QString lastError() const;

    /**
     * Applies @p config: a new interval restarts the timer, a new temperature unit or
     * fan fallback re-emits the last readings right away without reading the source.
     */
    void setConfig(const RuntimeConfig &config);

signals:
    /** New display readings, from a poll or a changed display setting. */
    void readingsChanged(const QVector<SensorReading> &readings);

private:
    /** Requests a read in the worker unless the previous one is still running. */
    void requestPoll();

    /** Stores raw readings from the worker and emits them prepared for display. */
    void acceptRawReadings(const QVector<SensorReading> &raw);

    QThread *m_thread;
    SensorPoller *m_poller;
    QTimer *m_timer;
    ReadingPipeline m_pipeline;
    RuntimeConfig m_config;
    QVector<SensorReading> m_rawReadings;
    QString m_lastError;
    bool m_running = false;
    bool m_pollPending = false;
};
