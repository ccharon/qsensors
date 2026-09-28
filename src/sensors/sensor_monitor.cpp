// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/sensor_monitor.h"

#include <QThread>
#include <QTimer>

#include <chrono>
#include <utility>

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

    /** Reads the source and emits the result. */
    void poll() {
        emit rawReadingsRead(read());
    }

signals:
    void rawReadingsRead(const QVector<SensorReading> &raw);

private:
    SensorMonitor::SourceFactory m_factory;
    std::unique_ptr<SensorSource> m_source;
};

SensorMonitor::SensorMonitor(SourceFactory factory, QObject *parent)
    : QObject(parent), m_thread(new QThread(this)), m_poller(new SensorPoller(std::move(factory))),
      m_timer(new QTimer(this)) {
    m_thread->setObjectName(QStringLiteral("qsensors-poll"));
    m_poller->moveToThread(m_thread);
    // Deferred deletes run in the worker before it exits, so the source dies in its own thread.
    connect(m_thread, &QThread::finished, m_poller, &QObject::deleteLater);
    connect(m_poller, &SensorPoller::rawReadingsRead, this, &SensorMonitor::acceptRawReadings);
    connect(m_timer, &QTimer::timeout, this, &SensorMonitor::requestPoll);
}

SensorMonitor::~SensorMonitor() {
    m_timer->stop();
    if (m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait();
    } else {
        delete m_poller; // worker never started; the poller still has no source
    }
}

bool SensorMonitor::start(const RuntimeConfig &config) {
    m_config = config;
    m_thread->start();

    // Blocking first read: the window is sized to its content before it is shown.
    QString error;
    QVector<SensorReading> first;
    SensorPoller *poller = m_poller;
    QMetaObject::invokeMethod(m_poller, [poller, &error, &first] {
        error = poller->initialize();
        if (error.isEmpty())
            first = poller->read();
    }, Qt::BlockingQueuedConnection);

    if (!error.isEmpty()) {
        m_lastError = error;
        return false;
    }
    m_running = true;
    m_timer->setInterval(std::chrono::seconds(m_config.pollingIntervalSec));
    m_timer->start();
    acceptRawReadings(first);
    return true;
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

void SensorMonitor::requestPoll() {
    // A read slower than the interval must not queue up further reads.
    if (m_pollPending)
        return;
    m_pollPending = true;
    QMetaObject::invokeMethod(m_poller, &SensorPoller::poll, Qt::QueuedConnection);
}

void SensorMonitor::acceptRawReadings(const QVector<SensorReading> &raw) {
    m_pollPending = false;
    m_rawReadings = raw;
    emit readingsChanged(m_pipeline.process(m_rawReadings, m_config));
}

#include "sensor_monitor.moc"
