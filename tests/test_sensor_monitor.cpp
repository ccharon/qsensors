// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/sensor_monitor.h"

#include <QMutex>
#include <QMutexLocker>
#include <QSignalSpy>
#include <QThread>
#include <QtTest/QtTest>

// Verifies that SensorMonitor keeps the source in a worker thread and emits display readings.
class SensorMonitorTest final : public QObject {
    Q_OBJECT

private slots:
    void start_reads_once_in_worker_and_emits_before_returning();
    void init_failure_reports_error_and_emits_nothing();
    void timer_polls_in_worker_thread();
    void display_setting_change_reprocesses_without_reading();
    void source_is_destroyed_in_its_own_thread();
};

namespace {
    /** What the fake source saw, shared with the test across threads. */
    struct SourceLog {
        QMutex mutex;
        QThread *createdIn = nullptr;
        QThread *destroyedIn = nullptr;
        QList<QThread *> readIn;

        int reads() {
            QMutexLocker lock(&mutex);
            return static_cast<int>(readIn.size());
        }
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
        [[nodiscard]] QString lastError() const override { return m_initialized ? QString() : QStringLiteral("no chips"); }

        [[nodiscard]] QVector<SensorReading> readAll() override {
            QMutexLocker lock(&m_log->mutex);
            m_log->readIn.append(QThread::currentThread());
            const double value = 40.0 + static_cast<double>(m_log->readIn.size());
            return {{.chip = QStringLiteral("chip"), .category = SensorCategory::Temperatures,
                     .feature = QStringLiteral("temp1"), .featureNumber = 0, .subfeatureNumber = 1,
                     .value = value, .unit = SensorUnit::Celsius}};
        }

    private:
        std::shared_ptr<SourceLog> m_log;
        bool m_initialized;
    };

    SensorMonitor::SourceFactory factory(const std::shared_ptr<SourceLog> &log, const bool initialized = true) {
        return [log, initialized] { return std::make_unique<FakeSource>(log, initialized); };
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
    QCOMPARE(readingsAt(spy, 0).at(0).value, 41.0);
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
    QCOMPARE(readingsAt(spy, 1).at(0).value, 42.0);
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
    QCOMPARE(readingsAt(spy, 1).at(0).value, 41.0 * 9.0 / 5.0 + 32.0);
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

QTEST_GUILESS_MAIN(SensorMonitorTest)
#include "test_sensor_monitor.moc"
