// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/reading_pipeline.h"

#include <QtTest/QtTest>

// Verifies how ReadingPipeline turns raw source readings into display readings.
class ReadingPipelineTest final : public QObject {
    Q_OBJECT

private slots:
    void raw_values_pass_through_with_default_range();
    void fahrenheit_converts_value_and_limits();
    void fan_fallback_follows_config();
    void milli_scale_is_latched_per_sensor();
    void firmware_limits_follow_milli_scale();
    void reprocessing_same_raw_readings_is_stable();
};

namespace {
    SensorReading temperature(const double value, const std::optional<double> max = std::nullopt) {
        return {.chip = QStringLiteral("chip"), .category = SensorCategory::Temperatures, .feature = QStringLiteral("temp1"),
                .featureNumber = 0, .subfeatureNumber = 1, .value = value, .unit = SensorUnit::Celsius, .firmwareMax = max};
    }

    SensorReading power(const double watts, const int featureNumber = 3) {
        return {.chip = QStringLiteral("chip"), .category = SensorCategory::Power, .feature = QStringLiteral("power1"),
                .featureNumber = featureNumber, .subfeatureNumber = 1, .value = watts, .unit = SensorUnit::Watt};
    }
}

void ReadingPipelineTest::raw_values_pass_through_with_default_range() {
    ReadingPipeline pipeline;
    const QVector<SensorReading> shown = pipeline.process({temperature(42.0)}, RuntimeConfig{});

    QCOMPARE(shown.size(), 1);
    QCOMPARE(shown[0].value, 42.0);
    QCOMPARE(shown[0].unit, SensorUnit::Celsius);
    QCOMPARE(shown[0].minValue, std::optional(0.0));
    QCOMPARE(shown[0].maxValue, std::optional(100.0));
    // The default range fills the bar graph only; the firmware reported no limits.
    QVERIFY(!shown[0].hasFirmwareLimits());
}

void ReadingPipelineTest::fahrenheit_converts_value_and_limits() {
    ReadingPipeline pipeline;
    RuntimeConfig config;
    config.temperatureUnit = TemperatureUnit::Fahrenheit;
    const QVector<SensorReading> shown = pipeline.process({temperature(100.0, 80.0)}, config);

    QCOMPARE(shown[0].unit, SensorUnit::Fahrenheit);
    QCOMPARE(shown[0].value, 212.0);
    QCOMPARE(shown[0].minValue, std::optional(32.0));
    QCOMPARE(shown[0].maxValue, std::optional(176.0));
    QCOMPARE(shown[0].firmwareMin, std::nullopt);
    QCOMPARE(shown[0].firmwareMax, std::optional(176.0));
}

void ReadingPipelineTest::fan_fallback_follows_config() {
    ReadingPipeline pipeline;
    RuntimeConfig config;
    config.fanDefaultMaxRpm = 7200;
    const SensorReading fan{.chip = QStringLiteral("chip"), .category = SensorCategory::Fans, .feature = QStringLiteral("fan1"),
                            .featureNumber = 1, .subfeatureNumber = 1, .value = 1800.0, .unit = SensorUnit::Rpm};

    QCOMPARE(pipeline.process({fan}, config)[0].maxValue, std::optional(7200.0));
    config.fanDefaultMaxRpm = 3000;
    QCOMPARE(pipeline.process({fan}, config)[0].maxValue, std::optional(3000.0));
}

void ReadingPipelineTest::milli_scale_is_latched_per_sensor() {
    ReadingPipeline pipeline;
    const RuntimeConfig config;

    // First sample below 1 W switches to mW; 1.5 W stays in mW (hysteresis up to 10 W).
    QCOMPARE(pipeline.process({power(0.5)}, config)[0].unit, SensorUnit::Milliwatt);
    const SensorReading latched = pipeline.process({power(1.5)}, config)[0];
    QCOMPARE(latched.unit, SensorUnit::Milliwatt);
    QCOMPARE(latched.value, 1500.0);

    // Another sensor at 1.5 W has no history and starts in W.
    QCOMPARE(pipeline.process({power(1.5, 4)}, config)[0].unit, SensorUnit::Watt);

    // 10 W or more goes back to W.
    QCOMPARE(pipeline.process({power(10.0)}, config)[0].unit, SensorUnit::Watt);
}

void ReadingPipelineTest::firmware_limits_follow_milli_scale() {
    ReadingPipeline pipeline;
    SensorReading raw = power(0.3);
    raw.firmwareMax = 0.5;
    const SensorReading shown = pipeline.process({raw}, RuntimeConfig{})[0];
    QCOMPARE(shown.unit, SensorUnit::Milliwatt);
    QCOMPARE(shown.firmwareMax, std::optional(500.0));
    QCOMPARE(shown.maxValue, std::optional(500.0));
    // Only a maximum: the bar graph gets a default minimum, the firmware limits do not.
    QCOMPARE(shown.minValue, std::optional(100.0));
    QCOMPARE(shown.firmwareMin, std::nullopt);
}

void ReadingPipelineTest::reprocessing_same_raw_readings_is_stable() {
    ReadingPipeline pipeline;
    const RuntimeConfig config;
    const QVector<SensorReading> raw{power(0.05), power(5.0, 4), temperature(55.0)};

    const QVector<SensorReading> first = pipeline.process(raw, config);
    const QVector<SensorReading> second = pipeline.process(raw, config);
    QCOMPARE(second.size(), first.size());
    for (int i = 0; i < first.size(); ++i) {
        QCOMPARE(second[i].unit, first[i].unit);
        QCOMPARE(second[i].value, first[i].value);
        QCOMPARE(second[i].minValue, first[i].minValue);
        QCOMPARE(second[i].maxValue, first[i].maxValue);
        QCOMPARE(second[i].firmwareMax, first[i].firmwareMax);
    }
}

QTEST_APPLESS_MAIN(ReadingPipelineTest)
#include "test_reading_pipeline.moc"
