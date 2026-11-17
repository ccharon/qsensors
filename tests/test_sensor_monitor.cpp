// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/sensor_monitor.h"

#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QSignalSpy>
#include <QThread>
#include <QWaitCondition>
#include <QtTest/QtTest>

#include <chrono>

// Verifies that SensorMonitor keeps the source in a worker thread and emits display readings.
class SensorMonitorTest final : public QObject {
    Q_OBJECT

private slots:
    void start_reads_once_in_worker_and_emits_before_returning();
    void init_failure_reports_error_and_emits_nothing();
    void timer_polls_in_worker_thread();
    void display_setting_change_reprocesses_without_reading();
    void source_is_destroyed_in_its_own_thread();
    void stall_timeout_scales_with_interval();
    void stalled_read_is_reported_and_recovery_too();
    void normal_read_is_not_reported_as_stalled();
    void source_that_stops_working_is_reported_and_no_longer_polled();
    void slow_first_read_starts_stalled_and_fills_in_later();
    void slow_init_failure_is_reported_after_start();
    void shutdown_with_blocked_read_returns_within_limit();
};

using namespace std::chrono_literals;

namespace {
    /** What the fake source saw, shared with the test across threads. */
    struct SourceLog {
        QMutex mutex;
        QThread *createdIn = nullptr;
        QThread *destroyedIn = nullptr;
        QList<QThread *> readIn;
        QWaitCondition opened;
        bool closed = false; // reads block while closed
        int blockedReads = 0;
        int failAfterReads = -1; // the source turns uninitialized after this many reads (-1: never)

        int reads() {
            QMutexLocker lock(&mutex);
            return static_cast<int>(readIn.size());
        }

        int blocked() {
            QMutexLocker lock(&mutex);
            return blockedReads;
        }

        QThread *destroyed() {
            QMutexLocker lock(&mutex);
            return destroyedIn;
        }

        void close() {
            QMutexLocker lock(&mutex);
            closed = true;
        }

        void open() {
            QMutexLocker lock(&mutex);
            closed = false;
            opened.wakeAll();
        }

        void waitWhileClosed() {
            QMutexLocker lock(&mutex);
            while (closed)
                opened.wait(&mutex);
        }
    };

    /** Opens the log's gate when leaving the scope, so no worker stays blocked after a failed check. */
    struct GateGuard {
        std::shared_ptr<SourceLog> log;
        ~GateGuard() { log->open(); }
    };

    class FakeSource final : public SensorSource {
    public:
        FakeSource(std::shared_ptr<SourceLog> log, const bool initialized) : m_log(std::move(log)), m_initialized(initialized) {
            QMutexLocker lock(&m_log->mutex);
            m_log->createdIn = QThread::currentThread();
        }

        ~FakeSource() override {
            QMutexLocker lock(&m_log->mutex);
            m_log->destroyedIn = QThread::currentThread();
        }

        [[nodiscard]] bool isInitialized() const override { return m_initialized; }
        [[nodiscard]] QString lastError() const override {
            if (m_failed)
                return QStringLiteral("re-init failed");
            return m_initialized ? QString() : QStringLiteral("no chips");
        }

        [[nodiscard]] QVector<SensorReading> readAll() override {
            QMutexLocker lock(&m_log->mutex);
            m_log->readIn.append(QThread::currentThread());
            ++m_log->blockedReads;
            while (m_log->closed)
                m_log->opened.wait(&m_log->mutex);
            --m_log->blockedReads;
            // Like LibsensorsSource after a failed re-init: no readings, uninitialized.
            if (m_log->failAfterReads >= 0 && m_log->readIn.size() > m_log->failAfterReads) {
                m_initialized = false;
                m_failed = true;
                return {};
            }
            const double value = 40.0 + static_cast<double>(m_log->readIn.size());
            return {{.chip = QStringLiteral("chip"), .category = SensorCategory::Temperatures,
                     .feature = QStringLiteral("temp1"), .featureNumber = 0, .subfeatureNumber = 1,
                     .value = value, .unit = SensorUnit::Celsius}};
        }

    private:
        std::shared_ptr<SourceLog> m_log;
        bool m_initialized;
        bool m_failed = false;
    };

    /** The factory blocks while the log's gate is closed, like a slow sensors_init. */
    SensorMonitor::SourceFactory factory(const std::shared_ptr<SourceLog> &log, const bool initialized = true) {
        return [log, initialized] {
            log->waitWhileClosed();
            return std::make_unique<FakeSource>(log, initialized);
        };
    }

    QVector<SensorReading> readingsAt(const QSignalSpy &spy, const qsizetype index) {
        return spy.at(index).at(0).value<QVector<SensorReading>>();
    }
}

void SensorMonitorTest::start_reads_once_in_worker_and_emits_before_returning() {
    const auto log = std::make_shared<SourceLog>();
    SensorMonitor monitor(factory(log));
    QSignalSpy spy(&monitor, &SensorMonitor::readingsChanged);

    QVERIFY(monitor.start(RuntimeConfig{}));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(readingsAt(spy, 0).at(0).value.value(), 41.0);
    QCOMPARE(readingsAt(spy, 0).at(0).maxValue, std::optional(100.0)); // prepared by the pipeline
    QVERIFY(log->createdIn != QThread::currentThread());
    QCOMPARE(log->reads(), 1);
    QCOMPARE(log->readIn.at(0), log->createdIn);
    QVERIFY(monitor.lastError().isEmpty());
}

void SensorMonitorTest::init_failure_reports_error_and_emits_nothing() {
    const auto log = std::make_shared<SourceLog>();
    SensorMonitor monitor(factory(log, false));
    QSignalSpy spy(&monitor, &SensorMonitor::readingsChanged);

    QVERIFY(!monitor.start(RuntimeConfig{}));
    QCOMPARE(monitor.lastError(), QStringLiteral("no chips"));
    RuntimeConfig fahrenheit;
    fahrenheit.temperatureUnit = TemperatureUnit::Fahrenheit;
    monitor.setConfig(fahrenheit);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(log->reads(), 0);
}

void SensorMonitorTest::timer_polls_in_worker_thread() {
    const auto log = std::make_shared<SourceLog>();
    SensorMonitor monitor(factory(log));
    QSignalSpy spy(&monitor, &SensorMonitor::readingsChanged);
    RuntimeConfig config;
    config.pollingIntervalSec = RuntimeConfigLimits::kMinPollingIntervalSec;

    QVERIFY(monitor.start(config));
    QVERIFY(spy.wait(3000));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(readingsAt(spy, 1).at(0).value.value(), 42.0);
    QCOMPARE(log->readIn.at(1), log->createdIn);
}

void SensorMonitorTest::display_setting_change_reprocesses_without_reading() {
    const auto log = std::make_shared<SourceLog>();
    SensorMonitor monitor(factory(log));
    QSignalSpy spy(&monitor, &SensorMonitor::readingsChanged);
    RuntimeConfig config;
    QVERIFY(monitor.start(config));

    config.temperatureUnit = TemperatureUnit::Fahrenheit;
    monitor.setConfig(config);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(readingsAt(spy, 1).at(0).unit, SensorUnit::Fahrenheit);
    QCOMPARE(readingsAt(spy, 1).at(0).value.value(), 41.0 * 9.0 / 5.0 + 32.0);
    QCOMPARE(log->reads(), 1);

    // Only the interval changed: nothing to re-emit.
    config.pollingIntervalSec = RuntimeConfigLimits::kMaxPollingIntervalSec;
    monitor.setConfig(config);
    QCOMPARE(spy.count(), 2);
}

void SensorMonitorTest::source_is_destroyed_in_its_own_thread() {
    const auto log = std::make_shared<SourceLog>();
    {
        SensorMonitor monitor(factory(log));
        QVERIFY(monitor.start(RuntimeConfig{}));
    }
    QVERIFY(log->createdIn != nullptr);
    QCOMPARE(log->destroyedIn, log->createdIn);
}

void SensorMonitorTest::stall_timeout_scales_with_interval() {
    QCOMPARE(SensorMonitor::stallTimeoutFor(RuntimeConfigLimits::kMinPollingIntervalSec), SensorMonitor::kMinStallTimeout);
    QCOMPARE(SensorMonitor::stallTimeoutFor(RuntimeConfigLimits::kMaxPollingIntervalSec),
             std::chrono::milliseconds(std::chrono::seconds(RuntimeConfigLimits::kMaxPollingIntervalSec))
                 * SensorMonitor::kStallIntervalFactor);
    // No interval lets a read stall before it had a few intervals' time.
    for (int sec = RuntimeConfigLimits::kMinPollingIntervalSec; sec <= RuntimeConfigLimits::kMaxPollingIntervalSec; ++sec)
        QVERIFY(SensorMonitor::stallTimeoutFor(sec) >= std::chrono::seconds(sec) * 2);
}

void SensorMonitorTest::stalled_read_is_reported_and_recovery_too() {
    const auto log = std::make_shared<SourceLog>();
    SensorMonitor monitor(factory(log));
    const GateGuard guard{log};
    monitor.setStallTimeout(100ms);
    QSignalSpy readings(&monitor, &SensorMonitor::readingsChanged);
    QSignalSpy stalled(&monitor, &SensorMonitor::stalledChanged);
    RuntimeConfig config;
    config.pollingIntervalSec = RuntimeConfigLimits::kMinPollingIntervalSec;
    QVERIFY(monitor.start(config));
    QVERIFY(!monitor.isStalled());

    log->close();
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("has not returned for 100 ms")));
    QVERIFY(stalled.wait(3000));
    QCOMPARE(stalled.count(), 1);
    QCOMPARE(stalled.at(0).at(0).toBool(), true);
    QVERIFY(monitor.isStalled());
    QCOMPARE(readings.count(), 1);

    // Further timer ticks while stalled request no further reads.
    QTest::qWait(1200);
    QCOMPARE(log->reads(), 2);

    log->open();
    QVERIFY(stalled.wait(3000));
    QCOMPARE(stalled.count(), 2);
    QCOMPARE(stalled.at(1).at(0).toBool(), false);
    QVERIFY(!monitor.isStalled());
    QCOMPARE(readings.count(), 2);
    QCOMPARE(readingsAt(readings, 1).at(0).value.value(), 42.0);
}

void SensorMonitorTest::normal_read_is_not_reported_as_stalled() {
    const auto log = std::make_shared<SourceLog>();
    SensorMonitor monitor(factory(log));
    monitor.setStallTimeout(300ms);
    QSignalSpy readings(&monitor, &SensorMonitor::readingsChanged);
    QSignalSpy stalled(&monitor, &SensorMonitor::stalledChanged);
    RuntimeConfig config;
    config.pollingIntervalSec = RuntimeConfigLimits::kMinPollingIntervalSec;
    QVERIFY(monitor.start(config));

    QVERIFY(readings.wait(3000));
    // Past the stall timeout of the poll that just returned, before the next poll.
    QTest::qWait(500);
    QCOMPARE(stalled.count(), 0);
    QVERIFY(!monitor.isStalled());
}

void SensorMonitorTest::source_that_stops_working_is_reported_and_no_longer_polled() {
    const auto log = std::make_shared<SourceLog>();
    log->failAfterReads = 1;
    SensorMonitor monitor(factory(log));
    QSignalSpy readings(&monitor, &SensorMonitor::readingsChanged);
    QSignalSpy failed(&monitor, &SensorMonitor::sourceFailed);
    RuntimeConfig config;
    config.pollingIntervalSec = RuntimeConfigLimits::kMinPollingIntervalSec;
    QVERIFY(monitor.start(config));
    QCOMPARE(readings.count(), 1);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("stopped working: re-init failed")));
    QVERIFY(failed.wait(10000));
    QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("re-init failed"));
    QCOMPARE(monitor.lastError(), QStringLiteral("re-init failed"));
    // The cards of the lost source go away: an empty update follows the failure.
    QTRY_COMPARE_WITH_TIMEOUT(readings.count(), 2, 10000);
    QVERIFY(readingsAt(readings, 1).isEmpty());
    // The source was dropped in its own thread, and nothing polls it any more.
    QCOMPARE(log->destroyed(), log->createdIn);
    QVERIFY(!readings.wait(2500));
    QCOMPARE(log->reads(), 2);
}

void SensorMonitorTest::slow_first_read_starts_stalled_and_fills_in_later() {
    const auto log = std::make_shared<SourceLog>();
    SensorMonitor monitor(factory(log));
    const GateGuard guard{log};
    monitor.setStartupTimeout(100ms);
    QSignalSpy readings(&monitor, &SensorMonitor::readingsChanged);
    QSignalSpy stalled(&monitor, &SensorMonitor::stalledChanged);
    RuntimeConfig config;
    config.pollingIntervalSec = RuntimeConfigLimits::kMinPollingIntervalSec;

    // A hanging driver: start() must return so the window can be shown, empty and stalled.
    log->close();
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("first sensor read has not returned within 100 ms")));
    QElapsedTimer elapsed;
    elapsed.start();
    QVERIFY(monitor.start(config));
    QVERIFY2(elapsed.elapsed() < 5000, qPrintable(QString::number(elapsed.elapsed())));
    QCOMPARE(readings.count(), 0);
    QVERIFY(monitor.isStalled());
    QCOMPARE(stalled.count(), 1);
    QCOMPARE(stalled.at(0).at(0).toBool(), true);
    QVERIFY(monitor.lastError().isEmpty());

    // The read returns: the normal path shows it, ends the stall and starts polling.
    log->open();
    QVERIFY(readings.wait(10000));
    QCOMPARE(readingsAt(readings, 0).at(0).value.value(), 41.0);
    QVERIFY(!monitor.isStalled());
    QCOMPARE(stalled.count(), 2);
    QCOMPARE(stalled.at(1).at(0).toBool(), false);
    QTRY_COMPARE_WITH_TIMEOUT(readings.count(), 2, 10000);
    QCOMPARE(log->reads(), 2);
}

void SensorMonitorTest::slow_init_failure_is_reported_after_start() {
    const auto log = std::make_shared<SourceLog>();
    SensorMonitor monitor(factory(log, false));
    const GateGuard guard{log};
    monitor.setStartupTimeout(100ms);
    QSignalSpy readings(&monitor, &SensorMonitor::readingsChanged);
    QSignalSpy stalled(&monitor, &SensorMonitor::stalledChanged);
    QSignalSpy failed(&monitor, &SensorMonitor::sourceFailed);

    log->close();
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("first sensor read has not returned")));
    QVERIFY(monitor.start(RuntimeConfig{}));
    QVERIFY(monitor.isStalled());

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("could not be initialized: no chips")));
    log->open();
    QVERIFY(failed.wait(10000));
    QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("no chips"));
    QCOMPARE(monitor.lastError(), QStringLiteral("no chips"));
    QVERIFY(!monitor.isStalled());
    QCOMPARE(stalled.count(), 2);
    QCOMPARE(readings.count(), 0);
    QCOMPARE(log->reads(), 0);
}

void SensorMonitorTest::shutdown_with_blocked_read_returns_within_limit() {
    const auto log = std::make_shared<SourceLog>();
    const GateGuard guard{log};
    auto monitor = std::make_unique<SensorMonitor>(factory(log));
    monitor->setShutdownTimeout(200ms);
    RuntimeConfig config;
    config.pollingIntervalSec = RuntimeConfigLimits::kMinPollingIntervalSec;
    QVERIFY(monitor->start(config));

    log->close();
    QTRY_COMPARE_WITH_TIMEOUT(log->blocked(), 1, 3000);
    QElapsedTimer elapsed;
    elapsed.start();
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("did not return within 200 ms")));
    monitor.reset();
    QVERIFY2(elapsed.elapsed() < 1500, qPrintable(QString::number(elapsed.elapsed())));
    QCOMPARE(log->destroyed(), nullptr);

    // Once the read returns, the abandoned worker still destroys the source in its own thread.
    log->open();
    QTRY_VERIFY_WITH_TIMEOUT(log->destroyed() != nullptr, 3000);
    QCOMPARE(log->destroyed(), log->createdIn);
}

QTEST_GUILESS_MAIN(SensorMonitorTest)
#include "test_sensor_monitor.moc"
