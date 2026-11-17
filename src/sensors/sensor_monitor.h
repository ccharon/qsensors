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
    /**
     * Upper bound of the time-based retry of a failed source; the first retry comes after one
     * polling interval and each failed one doubles the delay up to this.
     */
    static constexpr std::chrono::milliseconds kDefaultMaxRetryDelay{std::chrono::minutes(1)};

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
     * the source could not be initialized (see lastError()); it is retried then like a
     * source that failed later, and sourceRecovered() reports success.
     * When the first read takes longer than the startup timeout (a hanging driver),
     * returns true without readings and in the stalled state (stalledChanged(true));
     * once it returns, readingsChanged() and polling follow, or sourceFailed() when
     * the source could not be initialized (and is retried). Call once.
     */
    bool start(const RuntimeConfig &config);

    /** Error of the source: why it could not be initialized or why it stopped working; empty while it works. */
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

    /** Replaces the first retry delay (one polling interval) and kDefaultMaxRetryDelay; for tests. */
    void setRetryDelays(std::chrono::milliseconds first, std::chrono::milliseconds max);

    /** Delay of the time-based retry after one that failed after @p current: doubled, at most @p max. */
    [[nodiscard]] static std::chrono::milliseconds nextRetryDelay(std::chrono::milliseconds current,
                                                                  std::chrono::milliseconds max);

signals:
    /** New display readings, from a poll or a changed display setting. */
    void readingsChanged(const QVector<SensorReading> &readings);

    /** A read stalled (true) or a stalled read returned (false). */
    void stalledChanged(bool stalled);

    /**
     * The source stopped working after start() (see lastError()), or a failed source now
     * fails with a different error; readingsChanged() follows with no readings the first
     * time. The source is retried every polling interval (cheap: only when it sees a
     * reason such as a hardware change) and with a time-based backoff (see
     * kDefaultMaxRetryDelay) until sourceRecovered().
     */
    void sourceFailed(const QString &error);

    /** A failed source works again; lastError() is empty and readingsChanged() follows. */
    void sourceRecovered();

private:
    /** Requests a read (or, while failed, a retry) in the worker unless the previous one is still running. */
    void requestPoll();

    /** The time-based retry is due; it runs with the next request. */
    void retryDue();

    /** Result of the first read: starts polling on success, else keeps @p error; false on error. */
    bool finishStart(const QString &error, const QVector<SensorReading> &first);

    /** Result of a first read that start() stopped waiting for. */
    void finishLateStart(const QString &error, const QVector<SensorReading> &first);

    /** Stores raw readings from the worker and emits them prepared for display. */
    void acceptRawReadings(const QVector<SensorReading> &raw);

    /** Reports the outstanding read as stalled. */
    void markStalled();

    /** Switches to retrying a source that stopped working and reports @p error. */
    void handleSourceFailure(const QString &error);

    /** Enters the failed state with @p error: polling ticks retry, the time-based retry is scheduled. */
    void enterFailedState(const QString &error);

    /** A retry did not bring the source back; @p error is its current error. */
    void handleRetryFailure(const QString &error);

    /** A retry brought the source back; @p raw is its first read. */
    void handleRecovery(const QVector<SensorReading> &raw);

    /** A request to the worker returned: clears the pending state and ends a stall. */
    void finishRequest();

    QThread *m_thread;
    SensorPoller *m_poller;
    QTimer *m_timer;
    QTimer *m_stallTimer; // runs while a poll is pending
    QTimer *m_retryTimer; // single shot, while failed: the next time-based retry
    std::optional<std::chrono::milliseconds> m_stallTimeoutOverride;
    std::chrono::milliseconds m_startupTimeout = kDefaultStartupTimeout;
    std::chrono::milliseconds m_shutdownTimeout = kDefaultShutdownTimeout;
    std::optional<std::chrono::milliseconds> m_firstRetryDelayOverride;
    std::chrono::milliseconds m_maxRetryDelay = kDefaultMaxRetryDelay;
    std::chrono::milliseconds m_retryDelay{0};
    ReadingPipeline m_pipeline;
    RuntimeConfig m_config;
    QVector<SensorReading> m_rawReadings;
    QString m_lastError;
    bool m_running = false; // the polling timer runs (also while failed, for the retries)
    bool m_failed = false;
    bool m_retryDue = false;      // the time-based retry runs with the next request
    bool m_pendingRetryDue = false; // the request in the worker is a due retry
    bool m_pollPending = false;
    bool m_stalled = false;
};
