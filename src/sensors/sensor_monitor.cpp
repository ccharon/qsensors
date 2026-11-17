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
#include <chrono>
#include <memory>
#include <utility>

namespace {
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

/** Worker-thread side of SensorMonitor: owns the source and reads it on request. */
class SensorPoller final : public QObject {
    Q_OBJECT

public:
    explicit SensorPoller(SensorMonitor::SourceFactory factory) : m_factory(std::move(factory)) {}

    /** Creates the source; returns its initialization error, empty on success. */
    QString initialize() {
        m_source = m_factory();
        if (!m_source) {
            return QStringLiteral("no sensor source available");
        }
        if (!m_source->isInitialized()) {
            const QString error = m_source->lastError();
            m_source.reset();
            return error;
        }
        return {};
    }

    [[nodiscard]] QVector<SensorReading> read() const {
        return m_source ? m_source->readAll() : QVector<SensorReading>();
    }

    /** Creates the source and reads it once; hands the result to start() or, if it gave up, emits it. */
    void startup(const std::shared_ptr<StartupHandover> &handover) {
        QString error = initialize();
        QVector<SensorReading> first;
        if (error.isEmpty())
            first = read();
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

    /** Reads the source and emits the result; a source that stopped working is reported first and dropped. */
    void poll() {
        const QVector<SensorReading> raw = read();
        if (m_source && !m_source->isInitialized()) {
            QString error = m_source->lastError();
            m_source.reset();
            emit sourceFailed(error.isEmpty() ? QStringLiteral("sensor source stopped working") : error);
        }
        emit rawReadingsRead(raw);
    }

signals:
    void rawReadingsRead(const QVector<SensorReading> &raw);
    void sourceFailed(const QString &error);
    void lateStartFinished(const QString &error, const QVector<SensorReading> &first);

private:
    SensorMonitor::SourceFactory m_factory;
    std::unique_ptr<SensorSource> m_source;
};

SensorMonitor::SensorMonitor(SourceFactory factory, QObject *parent)
    : QObject(parent), m_thread(new QThread(this)), m_poller(new SensorPoller(std::move(factory))),
      m_timer(new QTimer(this)), m_stallTimer(new QTimer(this)) {
    m_thread->setObjectName(QStringLiteral("qsensors-poll"));
    m_stallTimer->setSingleShot(true);
    m_poller->moveToThread(m_thread);
    // Deferred deletes run in the worker before it exits, so the source dies in its own thread.
    connect(m_thread, &QThread::finished, m_poller, &QObject::deleteLater);
    connect(m_poller, &SensorPoller::rawReadingsRead, this, &SensorMonitor::acceptRawReadings);
    connect(m_poller, &SensorPoller::sourceFailed, this, &SensorMonitor::handleSourceFailure);
    connect(m_poller, &SensorPoller::lateStartFinished, this, &SensorMonitor::finishLateStart);
    connect(m_timer, &QTimer::timeout, this, &SensorMonitor::requestPoll);
    connect(m_stallTimer, &QTimer::timeout, this, &SensorMonitor::markStalled);
}

SensorMonitor::~SensorMonitor() {
    m_timer->stop();
    m_stallTimer->stop();
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
    m_thread->setParent(nullptr);
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
        m_lastError = error;
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
    qWarning("qsensors: sensor source could not be initialized: %s", qPrintable(error));
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
    if (displayChanged)
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

void SensorMonitor::requestPoll() {
    // A read slower than the interval must not queue up further reads.
    if (m_pollPending)
        return;
    m_pollPending = true;
    m_stallTimer->start(m_stallTimeoutOverride.value_or(stallTimeoutFor(m_config.pollingIntervalSec)));
    QMetaObject::invokeMethod(m_poller, &SensorPoller::poll, Qt::QueuedConnection);
}

void SensorMonitor::markStalled() {
    m_stalled = true;
    qWarning("qsensors: sensor read has not returned for %lld ms",
             static_cast<long long>(m_stallTimer->intervalAsDuration().count()));
    emit stalledChanged(true);
}

void SensorMonitor::handleSourceFailure(const QString &error) {
    m_lastError = error;
    m_running = false;
    m_timer->stop();
    qWarning("qsensors: sensor source stopped working: %s", qPrintable(error));
    emit sourceFailed(error);
}

void SensorMonitor::acceptRawReadings(const QVector<SensorReading> &raw) {
    m_pollPending = false;
    m_stallTimer->stop();
    m_rawReadings = raw;
    if (m_stalled) {
        m_stalled = false;
        qInfo("qsensors: sensor reads resumed");
        emit stalledChanged(false);
    }
    emit readingsChanged(m_pipeline.process(m_rawReadings, m_config));
}

#include "sensor_monitor.moc"
