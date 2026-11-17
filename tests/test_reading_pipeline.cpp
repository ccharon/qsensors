// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/reading_pipeline.h"
#include "sensors/sensors_policy.h"

#include <QtTest/QtTest>
#include <limits>

// Verifies how ReadingPipeline turns raw source readings into display readings.
class ReadingPipelineTest final : public QObject {
    Q_OBJECT

private slots:
    void raw_values_pass_through_with_default_range();
    void fahrenheit_converts_value_and_limits();
    void fan_fallback_follows_config();
    void milli_scale_is_latched_per_sensor();
    void latch_of_a_removed_sensor_is_dropped();
    void firmware_limits_follow_milli_scale();
    void reprocessing_same_raw_readings_is_stable();
    void zero_voltage_limits_are_ignored();
    void inverted_limits_are_ignored();
    void fan_zero_max_keeps_real_min();
    void nvme_sentinel_temperature_limits_are_ignored();
    void nvme_sentinel_temperature_limits_are_ignored_in_fahrenheit();
    void zero_limit_does_not_decide_milli_scale();
    void freezer_temperature_limits_are_kept();
    void freezer_temperature_limits_are_kept_in_fahrenheit();
    void zero_temperature_max_is_kept();
    void zero_minimums_with_real_maximum_are_kept();
    void negative_battery_current_minimum_is_kept();
    void reprocessing_with_dropped_limits_is_stable();
    void missing_value_keeps_unit_and_limits();
    void missing_first_value_uses_base_unit();
    void non_finite_value_is_missing();
    void reprocessing_with_missing_values_is_stable();
    void scale_follows_the_range_table();
    void scale_from_a_limit_survives_a_missing_limit();
    void temperature_scale_in_both_units();
    void new_quantities_get_their_ranges_and_scales();
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

    SensorReading raw(const SensorCategory category, const QString &feature, const int featureNumber, const double value,
                      const SensorUnit unit, const std::optional<double> min, const std::optional<double> max) {
        SensorReading reading;
        reading.chip = QStringLiteral("chip");
        reading.category = category;
        reading.feature = feature;
        reading.featureNumber = featureNumber;
        reading.subfeatureNumber = 1;
        reading.value = value;
        reading.unit = unit;
        reading.firmwareMin = min;
        reading.firmwareMax = max;
        return reading;
    }

    SensorReading voltage(const double volts, const std::optional<double> min, const std::optional<double> max) {
        return raw(SensorCategory::Voltages, QStringLiteral("in1"), 5, volts, SensorUnit::Volt, min, max);
    }

    SensorReading current(const double amps, const std::optional<double> min, const std::optional<double> max) {
        return raw(SensorCategory::Currents, QStringLiteral("curr1"), 6, amps, SensorUnit::Ampere, min, max);
    }

    SensorReading fan(const double rpm, const std::optional<double> min, const std::optional<double> max) {
        return raw(SensorCategory::Fans, QStringLiteral("fan1"), 7, rpm, SensorUnit::Rpm, min, max);
    }

    // NVMe secondary sensors report 0 K and 65535 K as their limits.
    constexpr double kNvmeLowC = -273.15;
    constexpr double kNvmeHighC = 65261.85;

    SiPrefix prefixOf(const SensorReading &reading) {
        return reading.scale ? reading.scale->prefix : SiPrefix::None;
    }

    SensorReading processOne(const SensorReading &input, const TemperatureUnit unit = TemperatureUnit::Celsius) {
        RuntimeConfig config;
        config.temperatureUnit = unit;
        ReadingPipeline pipeline;
        return pipeline.process({input}, config)[0];
    }
}

void ReadingPipelineTest::raw_values_pass_through_with_default_range() {
    ReadingPipeline pipeline;
    const QVector<SensorReading> shown = pipeline.process({temperature(42.0)}, RuntimeConfig{});

    QCOMPARE(shown.size(), 1);
    QCOMPARE(shown[0].value.value(), 42.0);
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
    QCOMPARE(shown[0].value.value(), 212.0);
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
    QCOMPARE(prefixOf(pipeline.process({power(0.5)}, config)[0]), SiPrefix::Milli);
    const SensorReading latched = pipeline.process({power(1.5)}, config)[0];
    QCOMPARE(prefixOf(latched), SiPrefix::Milli);
    QCOMPARE(latched.value.value(), 1.5);

    // Another sensor at 1.5 W has no history and starts in W.
    QCOMPARE(prefixOf(pipeline.process({power(1.5, 4)}, config)[0]), SiPrefix::None);

    // 10 W or more goes back to W.
    QCOMPARE(prefixOf(pipeline.process({power(10.0)}, config)[0]), SiPrefix::None);
}

void ReadingPipelineTest::latch_of_a_removed_sensor_is_dropped() {
    ReadingPipeline pipeline;
    const RuntimeConfig config;
    QCOMPARE(prefixOf(pipeline.process({power(0.5), power(2.0, 4)}, config)[0]), SiPrefix::Milli);
    QCOMPARE(pipeline.latchedSensorCount(), 2);

    // An empty read (failed source) keeps both latches.
    QVERIFY(pipeline.process({}, config).isEmpty());
    QCOMPARE(pipeline.latchedSensorCount(), 2);

    // Feature 3 is gone: its latch goes, so it starts over when it returns.
    (void) pipeline.process({power(2.0, 4)}, config);
    QCOMPARE(pipeline.latchedSensorCount(), 1);
    QCOMPARE(prefixOf(pipeline.process({power(1.5), power(2.0, 4)}, config)[0]), SiPrefix::None);
}

void ReadingPipelineTest::firmware_limits_follow_milli_scale() {
    ReadingPipeline pipeline;
    SensorReading raw = power(0.3);
    raw.firmwareMax = 0.5;
    const SensorReading shown = pipeline.process({raw}, RuntimeConfig{})[0];
    QCOMPARE(prefixOf(shown), SiPrefix::Milli);
    // Values and limits stay in W; only the display scale changes.
    QCOMPARE(shown.firmwareMax, std::optional(0.5));
    QCOMPARE(shown.maxValue, std::optional(0.5));
    // Only a maximum: the bar graph gets a default minimum, the firmware limits do not.
    QCOMPARE(shown.minValue, std::optional(0.1));
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
        QCOMPARE(second[i].scale, first[i].scale);
        QCOMPARE(second[i].value, first[i].value);
        QCOMPARE(second[i].minValue, first[i].minValue);
        QCOMPARE(second[i].maxValue, first[i].maxValue);
        QCOMPARE(second[i].firmwareMax, first[i].firmwareMax);
    }
}

void ReadingPipelineTest::zero_voltage_limits_are_ignored() {
    // Unconfigured voltage input: min = max = 0 V.
    const SensorReading shown = processOne(voltage(1.2, 0.0, 0.0));
    QVERIFY(!shown.hasFirmwareLimits());
    QCOMPARE(SensorsPolicy::alertState(shown), AlertState::None);
    QCOMPARE(shown.minValue, std::optional(0.0));
    QCOMPARE(*shown.maxValue, 1.8);
    QCOMPARE(*SensorsPolicy::rangeFraction(shown), 1.2 / 1.8);

    // Only a zero maximum reported.
    const SensorReading maxOnly = processOne(voltage(12.1, std::nullopt, 0.0));
    QVERIFY(!maxOnly.hasFirmwareLimits());
    QCOMPARE(SensorsPolicy::alertState(maxOnly), AlertState::None);
}

void ReadingPipelineTest::inverted_limits_are_ignored() {
    const SensorReading shown = processOne(voltage(4.0, 5.0, 3.0));
    QVERIFY(!shown.hasFirmwareLimits());
    QCOMPARE(SensorsPolicy::alertState(shown), AlertState::None);
    QCOMPARE(shown.minValue, std::optional(0.0));
    QCOMPARE(shown.maxValue, std::optional(6.0));
}

void ReadingPipelineTest::fan_zero_max_keeps_real_min() {
    // A fan with a minimum but an unset maximum register: the minimum still alerts.
    const SensorReading stalled = processOne(fan(600.0, 1000.0, 0.0));
    QCOMPARE(stalled.firmwareMin, std::optional(1000.0));
    QCOMPARE(stalled.firmwareMax, std::nullopt);
    QVERIFY(SensorsPolicy::alertState(stalled) != AlertState::None);
    QCOMPARE(stalled.maxValue, std::optional(5000.0));

    const SensorReading spinning = processOne(fan(1800.0, 1000.0, 0.0));
    QCOMPARE(SensorsPolicy::alertState(spinning), AlertState::None);
    QCOMPARE(*SensorsPolicy::rangeFraction(spinning), 0.2);
}

void ReadingPipelineTest::nvme_sentinel_temperature_limits_are_ignored() {
    SensorReading raw = temperature(40.0);
    raw.firmwareMin = kNvmeLowC;
    raw.firmwareMax = kNvmeHighC;
    const SensorReading shown = processOne(raw);
    QVERIFY(!shown.hasFirmwareLimits());
    QCOMPARE(SensorsPolicy::alertState(shown), AlertState::None);
    QCOMPARE(shown.minValue, std::optional(0.0));
    QCOMPARE(shown.maxValue, std::optional(100.0));
    QCOMPARE(*SensorsPolicy::rangeFraction(shown), 0.4);

    // Without a usable maximum the 100 °C fallback applies.
    raw.value = 105.0;
    QVERIFY(SensorsPolicy::alertState(processOne(raw)) != AlertState::None);
}

void ReadingPipelineTest::nvme_sentinel_temperature_limits_are_ignored_in_fahrenheit() {
    SensorReading raw = temperature(40.0);
    raw.firmwareMin = kNvmeLowC;
    raw.firmwareMax = kNvmeHighC;
    const SensorReading shown = processOne(raw, TemperatureUnit::Fahrenheit);
    QCOMPARE(shown.unit, SensorUnit::Fahrenheit);
    QCOMPARE(shown.value.value(), 104.0);
    QVERIFY(!shown.hasFirmwareLimits());
    QCOMPARE(SensorsPolicy::alertState(shown), AlertState::None);
    QCOMPARE(shown.minValue, std::optional(32.0));
    QCOMPARE(shown.maxValue, std::optional(212.0));
    QCOMPARE(*SensorsPolicy::rangeFraction(shown), 0.4);

    raw.value = 105.0;
    QVERIFY(SensorsPolicy::alertState(processOne(raw, TemperatureUnit::Fahrenheit)) != AlertState::None);
}

void ReadingPipelineTest::zero_limit_does_not_decide_milli_scale() {
    // 12 A with a zero maximum stays in A instead of becoming 12000.0 mA.
    const SensorReading zeroMax = processOne(current(12.0, std::nullopt, 0.0));
    QCOMPARE(zeroMax.unit, SensorUnit::Ampere);
    QCOMPARE(zeroMax.value.value(), 12.0);
    QVERIFY(!zeroMax.hasFirmwareLimits());
    QCOMPARE(SensorsPolicy::alertState(zeroMax), AlertState::None);
    QCOMPARE(zeroMax.minValue, std::optional(0.0));
    QCOMPARE(zeroMax.maxValue, std::optional(18.0));

    SensorReading watts = power(12.0);
    watts.firmwareMax = 0.0;
    const SensorReading zeroPowerMax = processOne(watts);
    QCOMPARE(zeroPowerMax.unit, SensorUnit::Watt);
    QVERIFY(!zeroPowerMax.hasFirmwareLimits());
    QCOMPARE(SensorsPolicy::alertState(zeroPowerMax), AlertState::None);

    // A zero minimum is a usable limit, but carries no scale: the value decides.
    const SensorReading zeroMin = processOne(current(12.0, 0.0, std::nullopt));
    QCOMPARE(zeroMin.unit, SensorUnit::Ampere);
    QCOMPARE(zeroMin.value.value(), 12.0);
    QCOMPARE(zeroMin.firmwareMin, std::optional(0.0));
    QCOMPARE(SensorsPolicy::alertState(zeroMin), AlertState::None);
    const SensorReading smallZeroMin = processOne(current(0.25, 0.0, std::nullopt));
    QCOMPARE(prefixOf(smallZeroMin), SiPrefix::Milli);
    QCOMPARE(smallZeroMin.value.value(), 0.25);
}

void ReadingPipelineTest::freezer_temperature_limits_are_kept() {
    SensorReading raw = temperature(-18.0);
    raw.firmwareMin = -40.0;
    raw.firmwareMax = -10.0;
    const SensorReading cold = processOne(raw);
    QCOMPARE(cold.firmwareMin, std::optional(-40.0));
    QCOMPARE(cold.firmwareMax, std::optional(-10.0));
    QCOMPARE(SensorsPolicy::alertState(cold), AlertState::None);
    QCOMPARE(cold.minValue, std::optional(-40.0));
    QCOMPARE(cold.maxValue, std::optional(-10.0));

    raw.value = -5.0;
    QVERIFY(SensorsPolicy::alertState(processOne(raw)) != AlertState::None);
}

void ReadingPipelineTest::freezer_temperature_limits_are_kept_in_fahrenheit() {
    SensorReading raw = temperature(-18.0);
    raw.firmwareMin = -40.0;
    raw.firmwareMax = -10.0;
    const SensorReading cold = processOne(raw, TemperatureUnit::Fahrenheit);
    QCOMPARE(cold.unit, SensorUnit::Fahrenheit);
    QCOMPARE(cold.firmwareMin, std::optional(-40.0));
    QCOMPARE(cold.firmwareMax, std::optional(14.0));
    QCOMPARE(SensorsPolicy::alertState(cold), AlertState::None);
    QCOMPARE(cold.minValue, std::optional(-40.0));
    QCOMPARE(cold.maxValue, std::optional(14.0));

    raw.value = -5.0;
    QVERIFY(SensorsPolicy::alertState(processOne(raw, TemperatureUnit::Fahrenheit)) != AlertState::None);
}

void ReadingPipelineTest::zero_temperature_max_is_kept() {
    // 0 °C is a real maximum for a freezer sensor.
    const SensorReading thawing = processOne(temperature(4.0, 0.0));
    QCOMPARE(thawing.firmwareMax, std::optional(0.0));
    QVERIFY(SensorsPolicy::alertState(thawing) != AlertState::None);

    const SensorReading thawingF = processOne(temperature(4.0, 0.0), TemperatureUnit::Fahrenheit);
    QCOMPARE(thawingF.firmwareMax, std::optional(32.0));
    QVERIFY(SensorsPolicy::alertState(thawingF) != AlertState::None);
    QCOMPARE(SensorsPolicy::alertState(processOne(temperature(-18.0, 0.0), TemperatureUnit::Fahrenheit)), AlertState::None);
}

void ReadingPipelineTest::zero_minimums_with_real_maximum_are_kept() {
    const SensorReading volts = processOne(voltage(1.2, 0.0, 1.5));
    QCOMPARE(volts.firmwareMin, std::optional(0.0));
    QCOMPARE(volts.firmwareMax, std::optional(1.5));
    QCOMPARE(SensorsPolicy::alertState(volts), AlertState::None);
    QCOMPARE(*SensorsPolicy::rangeFraction(volts), 0.8);
    QVERIFY(SensorsPolicy::alertState(processOne(voltage(1.6, 0.0, 1.5))) != AlertState::None);

    // A fan minimum of 0 RPM (semi-passive fan) stays a limit and never alerts.
    const SensorReading idleFan = processOne(fan(0.0, 0.0, std::nullopt));
    QCOMPARE(idleFan.firmwareMin, std::optional(0.0));
    QCOMPARE(SensorsPolicy::alertState(idleFan), AlertState::None);
    QCOMPARE(idleFan.maxValue, std::optional(5000.0));
}

void ReadingPipelineTest::negative_battery_current_minimum_is_kept() {
    const SensorReading discharging = processOne(current(-2.0, -5.0, 3.0));
    QCOMPARE(discharging.unit, SensorUnit::Ampere);
    QCOMPARE(discharging.firmwareMin, std::optional(-5.0));
    QCOMPARE(discharging.firmwareMax, std::optional(3.0));
    QCOMPARE(SensorsPolicy::alertState(discharging), AlertState::None);
    QCOMPARE(*SensorsPolicy::rangeFraction(discharging), 3.0 / 8.0);
    QVERIFY(SensorsPolicy::alertState(processOne(current(-6.0, -5.0, 3.0))) != AlertState::None);

    // Discharge only: a zero maximum below which the battery must stay.
    const SensorReading dischargeOnly = processOne(current(-2.0, -5.0, 0.0));
    QCOMPARE(dischargeOnly.firmwareMin, std::optional(-5.0));
    QCOMPARE(dischargeOnly.firmwareMax, std::optional(0.0));
    QCOMPARE(dischargeOnly.unit, SensorUnit::Ampere);
    QCOMPARE(SensorsPolicy::alertState(dischargeOnly), AlertState::None);
}

void ReadingPipelineTest::reprocessing_with_dropped_limits_is_stable() {
    ReadingPipeline pipeline;
    const RuntimeConfig config;
    SensorReading nvme = temperature(40.0);
    nvme.firmwareMin = kNvmeLowC;
    nvme.firmwareMax = kNvmeHighC;
    const QVector<SensorReading> input{voltage(1.2, 0.0, 0.0), current(12.0, std::nullopt, 0.0), nvme};

    const QVector<SensorReading> first = pipeline.process(input, config);
    const QVector<SensorReading> second = pipeline.process(input, config);
    for (int i = 0; i < first.size(); ++i) {
        QCOMPARE(second[i].unit, first[i].unit);
        QCOMPARE(second[i].scale, first[i].scale);
        QCOMPARE(second[i].value, first[i].value);
        QCOMPARE(second[i].minValue, first[i].minValue);
        QCOMPARE(second[i].maxValue, first[i].maxValue);
        QCOMPARE(second[i].firmwareMin, first[i].firmwareMin);
        QCOMPARE(second[i].firmwareMax, first[i].firmwareMax);
    }
    // The raw readings keep what the firmware reported.
    QCOMPARE(input[2].firmwareMax, std::optional(kNvmeHighC));
}

void ReadingPipelineTest::missing_value_keeps_unit_and_limits() {
    ReadingPipeline pipeline;
    const RuntimeConfig config;
    SensorReading unreadable = power(0.3);
    unreadable.value.reset();

    QCOMPARE(prefixOf(pipeline.process({power(0.3)}, config)[0]), SiPrefix::Milli);
    const SensorReading missing = pipeline.process({unreadable}, config)[0];
    QCOMPARE(prefixOf(missing), SiPrefix::Milli);
    QCOMPARE(missing.value, std::nullopt);
    QCOMPARE(SensorsPolicy::alertState(missing), AlertState::None);
    QCOMPARE(SensorsPolicy::rangeFraction(missing), std::nullopt);
    // 1.5 W would switch a fresh sensor to W; the latch from before the gap still holds.
    const SensorReading back = pipeline.process({power(1.5)}, config)[0];
    QCOMPARE(prefixOf(back), SiPrefix::Milli);
    QCOMPARE(back.value.value(), 1.5);

    // Firmware limits are reported, cleaned and converted without a value.
    SensorReading hot = temperature(0.0, 80.0);
    hot.value.reset();
    hot.firmwareMin = -273.15;
    RuntimeConfig fahrenheit;
    fahrenheit.temperatureUnit = TemperatureUnit::Fahrenheit;
    const SensorReading shown = pipeline.process({hot}, fahrenheit)[0];
    QCOMPARE(shown.unit, SensorUnit::Fahrenheit);
    QCOMPARE(shown.value, std::nullopt);
    QCOMPARE(shown.firmwareMin, std::nullopt);
    QCOMPARE(shown.firmwareMax.value(), 176.0);
    QCOMPARE(SensorsPolicy::alertState(shown), AlertState::None);
}

void ReadingPipelineTest::missing_first_value_uses_base_unit() {
    ReadingPipeline pipeline;
    const RuntimeConfig config;
    SensorReading unreadable = power(0.3);
    unreadable.value.reset();

    QCOMPARE(prefixOf(pipeline.process({unreadable}, config)[0]), SiPrefix::None);
    QCOMPARE(prefixOf(pipeline.process({unreadable}, config)[0]), SiPrefix::None);
    // Nothing was latched while the value was missing; the first value decides.
    QCOMPARE(prefixOf(pipeline.process({power(0.3)}, config)[0]), SiPrefix::Milli);
}

void ReadingPipelineTest::non_finite_value_is_missing() {
    ReadingPipeline pipeline;
    for (const double value: {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        const SensorReading shown = pipeline.process({temperature(value, 80.0)}, RuntimeConfig{})[0];
        QCOMPARE(shown.value, std::nullopt);
        QCOMPARE(shown.firmwareMax, std::optional(80.0));
        QCOMPARE(SensorsPolicy::alertState(shown), AlertState::None);
    }
}

void ReadingPipelineTest::reprocessing_with_missing_values_is_stable() {
    ReadingPipeline pipeline;
    const RuntimeConfig config;
    SensorReading unreadablePower = power(0.05);
    unreadablePower.value.reset();
    SensorReading unreadableFan = fan(0.0, 500.0, std::nullopt);
    unreadableFan.value.reset();
    const QVector<SensorReading> input{unreadablePower, unreadableFan, voltage(12.0, std::nullopt, std::nullopt)};

    const QVector<SensorReading> first = pipeline.process(input, config);
    const QVector<SensorReading> second = pipeline.process(input, config);
    for (int i = 0; i < first.size(); ++i) {
        QCOMPARE(second[i].unit, first[i].unit);
        QCOMPARE(second[i].scale, first[i].scale);
        QCOMPARE(second[i].value, first[i].value);
        QCOMPARE(second[i].minValue, first[i].minValue);
        QCOMPARE(second[i].maxValue, first[i].maxValue);
        QCOMPARE(second[i].firmwareMin, first[i].firmwareMin);
        QCOMPARE(second[i].firmwareMax, first[i].firmwareMax);
    }
    QCOMPARE(first[1].firmwareMin, std::optional(500.0));
}

void ReadingPipelineTest::scale_follows_the_range_table() {
    const auto scaleOf = [](const SensorReading &raw) { return processOne(raw).scale; };
    QCOMPARE(scaleOf(voltage(0.88, std::nullopt, std::nullopt)), std::optional(SensorScale{SiPrefix::Milli, 0}));
    QCOMPARE(scaleOf(voltage(1.052, std::nullopt, std::nullopt)), std::optional(SensorScale{SiPrefix::None, 3}));
    QCOMPARE(scaleOf(voltage(12.1, std::nullopt, std::nullopt)), std::optional(SensorScale{SiPrefix::None, 2}));
    QCOMPARE(scaleOf(voltage(230.1, std::nullopt, std::nullopt)), std::optional(SensorScale{SiPrefix::None, 1}));
    QCOMPARE(scaleOf(power(1234.0)), std::optional(SensorScale{SiPrefix::Kilo, 3}));
    QCOMPARE(scaleOf(fan(1534.0, std::nullopt, std::nullopt)), std::optional(SensorScale{SiPrefix::None, 0}));
    // The firmware range decides, not the value: a 1.2 V limit gives volts with three decimals.
    QCOMPARE(scaleOf(voltage(0.88, 0.5, 1.2)), std::optional(SensorScale{SiPrefix::None, 3}));
    // Values and limits stay in the base unit.
    const SensorReading shown = processOne(voltage(0.88, 0.5, 0.95));
    QCOMPARE(shown.scale, std::optional(SensorScale{SiPrefix::Milli, 0}));
    QCOMPARE(shown.value, std::optional(0.88));
    QCOMPARE(shown.firmwareMax, std::optional(0.95));
}

void ReadingPipelineTest::scale_from_a_limit_survives_a_missing_limit() {
    ReadingPipeline pipeline;
    const RuntimeConfig config;
    const std::optional<SensorScale> amps = SensorScale{SiPrefix::None, 3};
    QCOMPARE(pipeline.process({current(0.05, std::nullopt, 2.0)}, config)[0].scale, amps);
    // One poll fails to read curr1_max: the card stays in A instead of switching to "50 mA".
    QCOMPARE(pipeline.process({current(0.05, std::nullopt, std::nullopt)}, config)[0].scale, amps);
    QCOMPARE(pipeline.process({current(0.05, std::nullopt, 2.0)}, config)[0].scale, amps);
}

void ReadingPipelineTest::temperature_scale_in_both_units() {
    ReadingPipeline pipeline;
    RuntimeConfig config;
    const std::optional<SensorScale> oneDecimal = SensorScale{SiPrefix::None, 1};
    QCOMPARE(pipeline.process({temperature(40.0, 80.0)}, config)[0].scale, oneDecimal);
    config.temperatureUnit = TemperatureUnit::Fahrenheit;
    const SensorReading fahrenheit = pipeline.process({temperature(40.0)}, config)[0];
    QCOMPARE(fahrenheit.unit, SensorUnit::Fahrenheit);
    QCOMPARE(fahrenheit.scale, oneDecimal);
    config.temperatureUnit = TemperatureUnit::Celsius;
    QCOMPARE(pipeline.process({temperature(40.0)}, config)[0].scale, oneDecimal);
}

void ReadingPipelineTest::new_quantities_get_their_ranges_and_scales() {
    RuntimeConfig config;
    config.fanDefaultMaxRpm = 7200;
    ReadingPipeline pipeline;
    const QVector<SensorReading> shown = pipeline.process(
            {raw(SensorCategory::Fans, QStringLiteral("pwm1"), 8, 27.06, SensorUnit::Percent, std::nullopt, std::nullopt),
             raw(SensorCategory::Other, QStringLiteral("humidity1"), 9, 48.2, SensorUnit::RelativeHumidity,
                 std::nullopt, std::nullopt),
             raw(SensorCategory::Power, QStringLiteral("energy1"), 10, 3.6e6, SensorUnit::Joule, std::nullopt, std::nullopt),
             raw(SensorCategory::Other, QStringLiteral("freq1"), 11, 1.85e9, SensorUnit::Hertz, std::nullopt, std::nullopt)},
            config);
    // A PWM output among the fans uses 0..100 %, not the fan fallback maximum.
    QCOMPARE(shown[0].minValue, std::optional(0.0));
    QCOMPARE(shown[0].maxValue, std::optional(100.0));
    QCOMPARE(shown[0].scale, std::optional(SensorScale{SiPrefix::None, 0}));
    QCOMPARE(shown[1].maxValue, std::optional(100.0));
    // Counters and clocks have no range: no bar graph.
    QVERIFY(!shown[2].hasRange());
    QCOMPARE(shown[2].scale, std::optional(SensorScale{SiPrefix::Mega, 3}));
    QVERIFY(!shown[3].hasRange());
    QCOMPARE(shown[3].scale, std::optional(SensorScale{SiPrefix::Mega, 0}));
}

QTEST_APPLESS_MAIN(ReadingPipelineTest)
#include "test_reading_pipeline.moc"
