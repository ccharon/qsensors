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

#include <chrono>
#include <functional>
#include <memory>
#include <optional>

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

    /** A read counts as stalled after this many polling intervals, but never sooner than kMinStallTimeout. */
    static constexpr int kStallIntervalFactor = 3;
    /** Lower bound of the stall timeout; covers slow drivers such as drivetemp waking a drive. */
    static constexpr std::chrono::milliseconds kMinStallTimeout{std::chrono::seconds(10)};
    /** How long start() waits for the first read before it returns without readings (stalled). */
    static constexpr std::chrono::milliseconds kDefaultStartupTimeout = kMinStallTimeout;
        /** How long the destructor waits for a read in progress before giving up on the worker. */
    static constexpr std::chrono::milliseconds kDefaultShutdownTimeout{std::chrono::seconds(3)};

    /** Time after which a read requested at @p pollingIntervalSec counts as stalled. */
    [[nodiscard]] static std::chrono::milliseconds stallTimeoutFor(int pollingIntervalSec);

    /**
     * True once a destructor gave up on a worker (shutdown timeout). That worker may still
     * return at any time and run Qt and libsensors code, so the process must not tear down
     * Qt and its statics under it; main() ends with _Exit() then. Thread-safe.
     */
    [[nodiscard]] static bool workerAbandoned();

    /** Creates an idle monitor; nothing is read before start(). */
    explicit SensorMonitor(SourceFactory factory, QObject *parent = nullptr);

    /**
     * Stops polling and waits for the worker, which destroys the source. When a read
     * does not return within the shutdown timeout, the worker is abandoned (see .cpp).
     */
    ~SensorMonitor() override;

    /**
     * Creates the source and reads it once, blocking, so readingsChanged() is emitted
     * before this returns; then polls with the configured interval. Returns false when
     * the source could not be initialized (see lastError()); nothing is polled then.
     * When the first read takes longer than the startup timeout (a hanging driver),
     * returns true without readings and in the stalled state (stalledChanged(true));
     * once it returns, readingsChanged() and polling follow, or sourceFailed() when
     * the source could not be initialized. Call once.
     */
    bool start(const RuntimeConfig &config);

    /** Error of the source: why it could not be initialized or why it stopped working; empty otherwise. */
    [[nodiscard]] QString lastError() const;

    /**
     * Applies @p config: a new interval restarts the timer, a new temperature unit or
     * fan fallback re-emits the last readings right away without reading the source.
     */
    void setConfig(const RuntimeConfig &config);

    /** True while a read has been outstanding for longer than the stall timeout. */
    [[nodiscard]] bool isStalled() const;

    /** Replaces the stall timeout derived from the polling interval; for tests. */
    void setStallTimeout(std::chrono::milliseconds timeout);

    /** Replaces kDefaultStartupTimeout; for tests. */
    void setStartupTimeout(std::chrono::milliseconds timeout);

    /** Replaces kDefaultShutdownTimeout; for tests. */
    void setShutdownTimeout(std::chrono::milliseconds timeout);

signals:
    /** New display readings, from a poll or a changed display setting. */
    void readingsChanged(const QVector<SensorReading> &readings);

    /** A read stalled (true) or a stalled read returned (false). */
    void stalledChanged(bool stalled);

    /**
     * The source stopped working after start() (see lastError()); polling stops and
     * readingsChanged() follows with no readings.
     */
    void sourceFailed(const QString &error);

private:
    /** Requests a read in the worker unless the previous one is still running. */
    void requestPoll();

    /** Result of the first read: starts polling on success, else keeps @p error; false on error. */
    bool finishStart(const QString &error, const QVector<SensorReading> &first);

    /** Result of a first read that start() stopped waiting for. */
    void finishLateStart(const QString &error, const QVector<SensorReading> &first);

    /** Stores raw readings from the worker and emits them prepared for display. */
    void acceptRawReadings(const QVector<SensorReading> &raw);

    /** Reports the outstanding read as stalled. */
    void markStalled();

    /** Stops polling a source that stopped working and reports @p error. */
    void handleSourceFailure(const QString &error);

    QThread *m_thread;
    SensorPoller *m_poller;
    QTimer *m_timer;
    QTimer *m_stallTimer; // runs while a poll is pending
    std::optional<std::chrono::milliseconds> m_stallTimeoutOverride;
    std::chrono::milliseconds m_startupTimeout = kDefaultStartupTimeout;
    std::chrono::milliseconds m_shutdownTimeout = kDefaultShutdownTimeout;
    ReadingPipeline m_pipeline;
    RuntimeConfig m_config;
    QVector<SensorReading> m_rawReadings;
    QString m_lastError;
    bool m_running = false;
    bool m_pollPending = false;
    bool m_stalled = false;
};
