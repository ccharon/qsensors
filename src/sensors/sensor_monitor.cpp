// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/sensor_monitor.h"

#include <QDeadlineTimer>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>
#include <QWaitCondition>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <utility>

namespace {
    std::atomic_bool g_workerAbandoned{false};

    /**
     * Hand-over of the first read between the worker and start(), which waits for it with
     * a deadline. Once start() has given up (abandoned), the worker emits the result instead.
     */
    struct StartupHandover {
        QMutex mutex;
        QWaitCondition done;
        bool finished = false;
        bool abandoned = false;
        QString error;
        QVector<SensorReading> first;
    };
}

/**
 * Worker-thread side of SensorMonitor: owns the source and reads it on request. A source
 * that failed is kept, so it can be retried; it dies with the poller, in the worker.
 */
class SensorPoller final : public QObject {
    Q_OBJECT

public:
    explicit SensorPoller(SensorMonitor::SourceFactory factory) : m_factory(std::move(factory)) {}

    /** Creates the source; returns its initialization error, empty on success. */
    QString initialize() {
        m_source = m_factory();
        return m_source && m_source->isInitialized() ? QString() : error();
    }

    /** Why the source is not usable; never empty. */
    [[nodiscard]] QString error() const {
        if (!m_source)
            return QStringLiteral("no sensor source available");
        const QString error = m_source->lastError();
        return error.isEmpty() ? QStringLiteral("sensor source stopped working") : error;
    }

    [[nodiscard]] QVector<SensorReading> read() const {
        return m_source ? m_source->readAll() : QVector<SensorReading>();
    }

    /** Creates the source and reads it once; hands the result to start() or, if it gave up, emits it. */
    void startup(const std::shared_ptr<StartupHandover> &handover) {
        QString error = initialize();
        QVector<SensorReading> first;
        if (error.isEmpty()) {
            first = read();
            // A source that fails in its first read is a failed start, retried the same way.
            if (!m_source->isInitialized()) {
                error = this->error();
                first.clear();
            }
        }
        QMutexLocker lock(&handover->mutex);
        if (handover->abandoned) {
            lock.unlock();
            emit lateStartFinished(error, first);
            return;
        }
        handover->error = std::move(error);
        handover->first = std::move(first);
        handover->finished = true;
        handover->done.wakeAll();
    }

    /** Reads the source and emits the result; a source that stopped working is reported first. */
    void poll() {
        const QVector<SensorReading> raw = read();
        if (m_source && !m_source->isInitialized())
            emit sourceFailed(error());
        emit rawReadingsRead(raw);
    }

    /** Retries a failed source (see SensorSource::recover()); on success reads it and emits the result. */
    void retry(const bool retryDue) {
        if (!m_source || !m_source->recover(retryDue)) {
            emit retryFailed(error());
            return;
        }
        const QVector<SensorReading> raw = m_source->readAll();
        if (!m_source->isInitialized()) {
            emit retryFailed(error());
            return;
        }
        emit recovered(raw);
    }

signals:
    void rawReadingsRead(const QVector<SensorReading> &raw);
    void sourceFailed(const QString &error);
    void retryFailed(const QString &error);
    void recovered(const QVector<SensorReading> &raw);
    void lateStartFinished(const QString &error, const QVector<SensorReading> &first);

private:
    SensorMonitor::SourceFactory m_factory;
    std::unique_ptr<SensorSource> m_source;
};

SensorMonitor::SensorMonitor(SourceFactory factory, QObject *parent)
    : QObject(parent), m_thread(new QThread(this)), m_poller(new SensorPoller(std::move(factory))),
      m_timer(new QTimer(this)), m_stallTimer(new QTimer(this)), m_retryTimer(new QTimer(this)) {
    m_thread->setObjectName(QStringLiteral("qsensors-poll"));
    m_stallTimer->setSingleShot(true);
    m_retryTimer->setSingleShot(true);
    // Never early, so the backoff is a lower bound (a coarse timer may fire 5 % early).
    m_retryTimer->setTimerType(Qt::PreciseTimer);
    m_poller->moveToThread(m_thread);
    // Deferred deletes run in the worker before it exits, so the source dies in its own thread.
    connect(m_thread, &QThread::finished, m_poller, &QObject::deleteLater);
    connect(m_poller, &SensorPoller::rawReadingsRead, this, &SensorMonitor::acceptRawReadings);
    connect(m_poller, &SensorPoller::sourceFailed, this, &SensorMonitor::handleSourceFailure);
    connect(m_poller, &SensorPoller::retryFailed, this, &SensorMonitor::handleRetryFailure);
    connect(m_poller, &SensorPoller::recovered, this, &SensorMonitor::handleRecovery);
    connect(m_poller, &SensorPoller::lateStartFinished, this, &SensorMonitor::finishLateStart);
    connect(m_timer, &QTimer::timeout, this, &SensorMonitor::requestPoll);
    connect(m_stallTimer, &QTimer::timeout, this, &SensorMonitor::markStalled);
    connect(m_retryTimer, &QTimer::timeout, this, &SensorMonitor::retryDue);
}

SensorMonitor::~SensorMonitor() {
    m_timer->stop();
    m_stallTimer->stop();
    m_retryTimer->stop();
    if (!m_thread->isRunning()) {
        delete m_poller; // worker never started; the poller still has no source
        return;
    }
    m_thread->quit();
    if (m_thread->wait(QDeadlineTimer(m_shutdownTimeout)))
        return;

    qWarning("qsensors: sensor read did not return within %lld ms; not waiting for it",
             static_cast<long long>(m_shutdownTimeout.count()));
    // Destroying a running QThread aborts, and the poller and source may only die in the
    // worker. So the QThread object is leaked: if the read returns, the worker exits and
    // deletes the poller and source there as usual; if not, the process exit ends it.
    // A worker returning during Qt teardown would run against a half-destroyed Qt, so
    // main() skips the teardown once a worker was abandoned (see workerAbandoned()).
    m_thread->setParent(nullptr);
    g_workerAbandoned = true;
}

bool SensorMonitor::workerAbandoned() {
    return g_workerAbandoned;
}

std::chrono::milliseconds SensorMonitor::stallTimeoutFor(const int pollingIntervalSec) {
    return std::max(kMinStallTimeout, std::chrono::milliseconds(std::chrono::seconds(pollingIntervalSec)) * kStallIntervalFactor);
}

bool SensorMonitor::start(const RuntimeConfig &config) {
    m_config = config;
    m_thread->start();

    // Blocking first read: the window is sized to its content before it is shown. A
    // hanging driver must not keep the window from appearing, so the wait is bounded.
    const auto handover = std::make_shared<StartupHandover>();
    SensorPoller *poller = m_poller;
    QMetaObject::invokeMethod(m_poller, [poller, handover] { poller->startup(handover); }, Qt::QueuedConnection);

    QMutexLocker lock(&handover->mutex);
    const QDeadlineTimer deadline(m_startupTimeout);
    while (!handover->finished) {
        if (!handover->done.wait(&handover->mutex, deadline))
            break;
    }
    if (!handover->finished) {
        handover->abandoned = true; // the worker emits lateStartFinished() instead
        lock.unlock();
        m_stalled = true;
        qWarning("qsensors: first sensor read has not returned within %lld ms; starting without readings",
                 static_cast<long long>(m_startupTimeout.count()));
        emit stalledChanged(true);
        return true;
    }
    const QString error = handover->error;
    const QVector<SensorReading> first = handover->first;
    lock.unlock();
    return finishStart(error, first);
}

bool SensorMonitor::finishStart(const QString &error, const QVector<SensorReading> &first) {
    if (!error.isEmpty()) {
        qWarning("qsensors: sensor source could not be initialized: %s; retrying", qPrintable(error));
        enterFailedState(error);
        return false;
    }
    m_running = true;
    m_timer->setInterval(std::chrono::seconds(m_config.pollingIntervalSec));
    m_timer->start();
    acceptRawReadings(first); // also ends a stalled state
    return true;
}

void SensorMonitor::finishLateStart(const QString &error, const QVector<SensorReading> &first) {
    if (finishStart(error, first))
        return;
    emit sourceFailed(error);
    m_stalled = false;
    emit stalledChanged(false);
}

QString SensorMonitor::lastError() const {
    return m_lastError;
}

void SensorMonitor::setConfig(const RuntimeConfig &config) {
    const bool intervalChanged = config.pollingIntervalSec != m_config.pollingIntervalSec;
    const bool displayChanged = config.temperatureUnit != m_config.temperatureUnit
                                || config.fanDefaultMaxRpm != m_config.fanDefaultMaxRpm;
    m_config = config;
    if (!m_running)
        return;
    if (intervalChanged)
        m_timer->setInterval(std::chrono::seconds(m_config.pollingIntervalSec));
    // A failed source has no readings to show in another unit.
    if (displayChanged && !m_failed)
        emit readingsChanged(m_pipeline.process(m_rawReadings, m_config));
}

bool SensorMonitor::isStalled() const {
    return m_stalled;
}

void SensorMonitor::setStallTimeout(const std::chrono::milliseconds timeout) {
    m_stallTimeoutOverride = timeout;
}

void SensorMonitor::setStartupTimeout(const std::chrono::milliseconds timeout) {
    m_startupTimeout = timeout;
}

void SensorMonitor::setShutdownTimeout(const std::chrono::milliseconds timeout) {
    m_shutdownTimeout = timeout;
}

void SensorMonitor::setRetryDelays(const std::chrono::milliseconds first, const std::chrono::milliseconds max) {
    m_firstRetryDelayOverride = first;
    m_maxRetryDelay = max;
}

std::chrono::milliseconds SensorMonitor::nextRetryDelay(const std::chrono::milliseconds current,
                                                        const std::chrono::milliseconds max) {
    return std::min(current * 2, max);
}

void SensorMonitor::requestPoll() {
    // A read slower than the interval must not queue up further reads.
    if (m_pollPending)
        return;
    m_pollPending = true;
    m_stallTimer->start(m_stallTimeoutOverride.value_or(stallTimeoutFor(m_config.pollingIntervalSec)));
    if (!m_failed) {
        QMetaObject::invokeMethod(m_poller, &SensorPoller::poll, Qt::QueuedConnection);
        return;
    }
    // While failed, every tick asks the source for a cheap retry; a due one forces the init.
    m_pendingRetryDue = std::exchange(m_retryDue, false);
    SensorPoller *poller = m_poller;
    const bool due = m_pendingRetryDue;
    QMetaObject::invokeMethod(m_poller, [poller, due] { poller->retry(due); }, Qt::QueuedConnection);
}

void SensorMonitor::retryDue() {
    m_retryDue = true;
    requestPoll(); // a request in progress picks it up when it returns
}

void SensorMonitor::markStalled() {
    m_stalled = true;
    qWarning("qsensors: sensor read has not returned for %lld ms",
             static_cast<long long>(m_stallTimer->intervalAsDuration().count()));
    emit stalledChanged(true);
}

void SensorMonitor::handleSourceFailure(const QString &error) {
    qWarning("qsensors: sensor source stopped working: %s; retrying", qPrintable(error));
    enterFailedState(error);
    emit sourceFailed(error);
}

void SensorMonitor::enterFailedState(const QString &error) {
    m_failed = true;
    m_lastError = error;
    m_retryDue = false;
    m_retryDelay = m_firstRetryDelayOverride.value_or(std::chrono::seconds(m_config.pollingIntervalSec));
    m_retryTimer->start(m_retryDelay);
    // The polling timer goes on (or starts, after a failed start) and drives the cheap retries.
    m_running = true;
    m_timer->setInterval(std::chrono::seconds(m_config.pollingIntervalSec));
    if (!m_timer->isActive())
        m_timer->start();
}

void SensorMonitor::handleRetryFailure(const QString &error) {
    const bool wasDue = m_pendingRetryDue;
    finishRequest();
    // The same failure is logged once; only a different one is worth a line and the status.
    if (error != m_lastError) {
        m_lastError = error;
        qWarning("qsensors: sensor source still not working, now: %s; retrying", qPrintable(error));
        emit sourceFailed(error);
    }
    if (wasDue) {
        m_retryDelay = nextRetryDelay(m_retryDelay, m_maxRetryDelay);
        m_retryTimer->start(m_retryDelay);
    }
    if (m_retryDue)
        requestPoll();
}

void SensorMonitor::handleRecovery(const QVector<SensorReading> &raw) {
    m_failed = false;
    m_retryDue = false;
    m_retryTimer->stop();
    m_lastError.clear();
    qInfo("qsensors: sensor source works again");
    emit sourceRecovered();
    acceptRawReadings(raw);
}

void SensorMonitor::finishRequest() {
    m_pollPending = false;
    m_pendingRetryDue = false;
    m_stallTimer->stop();
    if (m_stalled) {
        m_stalled = false;
        qInfo("qsensors: sensor reads resumed");
        emit stalledChanged(false);
    }
}

void SensorMonitor::acceptRawReadings(const QVector<SensorReading> &raw) {
    finishRequest();
    m_rawReadings = raw;
    emit readingsChanged(m_pipeline.process(m_rawReadings, m_config));
}

#include "sensor_monitor.moc"
