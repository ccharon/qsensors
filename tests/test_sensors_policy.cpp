// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/sensors_policy.h"

#include <QtTest/QtTest>
#include <limits>

// Verifies default-range policy behavior for normalized sensor categories.
class SensorsPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void temperatures_defaultRange_applied_when_missing();
    void fans_defaultMax_uses_runtime_config();
    void voltages_defaults_are_filled();
    void currents_defaults_are_filled();
    void power_defaults_are_filled();
    void power_defaults_both_missing();
    void other_defaults_are_filled();
    void existing_bounds_not_overwritten();
    void nan_input_yields_finite_range();
    void inf_input_yields_finite_range();
    void currentPower_scaling_uses_native_max_when_present();
    void currentPower_scaling_falls_back_to_value_when_no_range();
    void currentPower_scaling_skipped_when_at_or_above_one();
    void currentPower_scaling_ignores_unrelated_units();
    void currentPower_latched_milli_survives_crossing_one();
    void currentPower_latched_base_survives_dropping_below_one();
    void currentPower_latched_milli_released_on_display_overflow();
    void currentPower_latched_base_switches_to_milli_when_too_coarse();
    void currentPower_native_limits_bypass_latch();
    void signed_current_without_limits_mirrors_range();
    void signed_power_with_native_max_only_mirrors_range();
    void negative_voltage_keeps_zero_minimum();
    void alertState_matches_unit_rules();
    void rangeFraction_maps_value_into_limits();
};

void SensorsPolicyTest::temperatures_defaultRange_applied_when_missing() {
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorCategory::Temperatures, 42.0, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QCOMPARE(*min, 0.0);
    QCOMPARE(*max, 100.0);
}

void SensorsPolicyTest::fans_defaultMax_uses_runtime_config() {
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorCategory::Fans, 1800.0, min, max, 7200);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QCOMPARE(*min, 0.0);
    QCOMPARE(*max, 7200.0);
}

void SensorsPolicyTest::voltages_defaults_are_filled() {
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorCategory::Voltages, 1.2, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QCOMPARE(*min, 0.0);
    QCOMPARE(*max, 1.8);
}

void SensorsPolicyTest::currents_defaults_are_filled() {
    std::optional<double> min;
    std::optional<double> max = 10.0;

    SensorsPolicy::applyDefaultRangePolicy(SensorCategory::Currents, 6.0, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QCOMPARE(*min, 2.0);
    QCOMPARE(*max, 10.0);
}

void SensorsPolicyTest::power_defaults_are_filled() {
    std::optional<double> min = 20.0;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorCategory::Power, 35.0, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QCOMPARE(*min, 20.0);
    QCOMPARE(*max, 35.0);
}

void SensorsPolicyTest::power_defaults_both_missing() {
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorCategory::Power, 80.0, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QCOMPARE(*min, 0.0);
    QCOMPARE(*max, 120.0);
}

void SensorsPolicyTest::other_defaults_are_filled() {
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorCategory::Other, 4.0, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QCOMPARE(*min, 2.0);
    QCOMPARE(*max, 6.0);
}

void SensorsPolicyTest::existing_bounds_not_overwritten() {
    std::optional<double> min = 5.0;
    std::optional<double> max = 42.0;

    SensorsPolicy::applyDefaultRangePolicy(SensorCategory::Voltages, 12.0, min, max, 5000);

    QCOMPARE(*min, 5.0);
    QCOMPARE(*max, 42.0);
}

void SensorsPolicyTest::nan_input_yields_finite_range() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorCategory::Other, nan, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QVERIFY(std::isfinite(*min));
    QVERIFY(std::isfinite(*max));
}

void SensorsPolicyTest::inf_input_yields_finite_range() {
    const double inf = std::numeric_limits<double>::infinity();
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorCategory::Voltages, inf, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QVERIFY(std::isfinite(*min));
    QVERIFY(std::isfinite(*max));
}

void SensorsPolicyTest::currentPower_scaling_uses_native_max_when_present() {
    SensorUnit unit = SensorUnit::Watt;
    double value = 0.926;
    std::optional<double> min;
    std::optional<double> max = 0.5;

    SensorsPolicy::applyCurrentPowerUnitScaling(unit, value, min, max);

    QCOMPARE(unit, SensorUnit::Milliwatt);
    QCOMPARE(value, 926.0);
    QCOMPARE(*max, 500.0);
}

void SensorsPolicyTest::currentPower_scaling_falls_back_to_value_when_no_range() {
    SensorUnit unit = SensorUnit::Ampere;
    double value = 0.003;
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyCurrentPowerUnitScaling(unit, value, min, max);

    QCOMPARE(unit, SensorUnit::Milliampere);
    QCOMPARE(value, 3.0);
    QVERIFY(!min.has_value());
    QVERIFY(!max.has_value());
}

void SensorsPolicyTest::currentPower_scaling_skipped_when_at_or_above_one() {
    SensorUnit unit = SensorUnit::Ampere;
    double value = 5.0;
    std::optional<double> min = 0.0;
    std::optional<double> max = 10.0;

    SensorsPolicy::applyCurrentPowerUnitScaling(unit, value, min, max);

    QCOMPARE(unit, SensorUnit::Ampere);
    QCOMPARE(value, 5.0);
    QCOMPARE(*max, 10.0);
}

void SensorsPolicyTest::currentPower_scaling_ignores_unrelated_units() {
    SensorUnit unit = SensorUnit::Volt;
    double value = 0.011;
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyCurrentPowerUnitScaling(unit, value, min, max);

    QCOMPARE(unit, SensorUnit::Volt);
    QCOMPARE(value, 0.011);
}

void SensorsPolicyTest::currentPower_latched_milli_survives_crossing_one() {
    std::optional<bool> latch;
    std::optional<double> min;
    std::optional<double> max;

    SensorUnit unit = SensorUnit::Watt;
    double value = 0.9827;
    SensorsPolicy::applyCurrentPowerUnitScaling(unit, value, min, max, &latch);
    QCOMPARE(unit, SensorUnit::Milliwatt);
    QCOMPARE(latch, std::optional<bool>(true));

    // Next poll crosses 1 W: unit must not flip back to W.
    unit = SensorUnit::Watt;
    value = 1.25;
    SensorsPolicy::applyCurrentPowerUnitScaling(unit, value, min, max, &latch);
    QCOMPARE(unit, SensorUnit::Milliwatt);
    QCOMPARE(value, 1250.0);
}

void SensorsPolicyTest::currentPower_latched_base_survives_dropping_below_one() {
    std::optional<bool> latch;
    std::optional<double> min;
    std::optional<double> max;

    SensorUnit unit = SensorUnit::Watt;
    double value = 1.25;
    SensorsPolicy::applyCurrentPowerUnitScaling(unit, value, min, max, &latch);
    QCOMPARE(unit, SensorUnit::Watt);
    QCOMPARE(latch, std::optional<bool>(false));

    unit = SensorUnit::Watt;
    value = 0.9;
    SensorsPolicy::applyCurrentPowerUnitScaling(unit, value, min, max, &latch);
    QCOMPARE(unit, SensorUnit::Watt);
    QCOMPARE(value, 0.9);
}

void SensorsPolicyTest::currentPower_latched_milli_released_on_display_overflow() {
    std::optional<bool> latch = true;
    std::optional<double> min;
    std::optional<double> max;

    // 12 A would render as 12000.0 mA, which no longer fits the LCD.
    SensorUnit unit = SensorUnit::Ampere;
    double value = 12.0;
    SensorsPolicy::applyCurrentPowerUnitScaling(unit, value, min, max, &latch);
    QCOMPARE(unit, SensorUnit::Ampere);
    QCOMPARE(value, 12.0);
    QCOMPARE(latch, std::optional<bool>(false));
}

void SensorsPolicyTest::currentPower_latched_base_switches_to_milli_when_too_coarse() {
    std::optional<bool> latch = false;
    std::optional<double> min;
    std::optional<double> max;

    // 0.04 A would render as "0.04"; milli keeps useful precision.
    SensorUnit unit = SensorUnit::Ampere;
    double value = 0.04;
    SensorsPolicy::applyCurrentPowerUnitScaling(unit, value, min, max, &latch);
    QCOMPARE(unit, SensorUnit::Milliampere);
    QCOMPARE(value, 40.0);
    QCOMPARE(latch, std::optional<bool>(true));
}

void SensorsPolicyTest::currentPower_native_limits_bypass_latch() {
    std::optional<bool> latch = true;
    std::optional<double> min = 0.0;
    std::optional<double> max = 10.0;

    SensorUnit unit = SensorUnit::Ampere;
    double value = 0.5;
    SensorsPolicy::applyCurrentPowerUnitScaling(unit, value, min, max, &latch);
    QCOMPARE(unit, SensorUnit::Ampere);
    QCOMPARE(value, 0.5);
}

void SensorsPolicyTest::signed_current_without_limits_mirrors_range() {
    // Discharging battery: -396 mA must lie inside the synthetic range (no alert).
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorCategory::Currents, -396.0, min, max, 5000);

    QCOMPARE(*min, -594.0);
    QCOMPARE(*max, 594.0);
}

void SensorsPolicyTest::signed_power_with_native_max_only_mirrors_range() {
    std::optional<double> min;
    std::optional<double> max = 60.0;

    SensorsPolicy::applyDefaultRangePolicy(SensorCategory::Power, -4.7, min, max, 5000);

    QCOMPARE(*min, -60.0);
    QCOMPARE(*max, 60.0);
}

void SensorsPolicyTest::negative_voltage_keeps_zero_minimum() {
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorCategory::Voltages, -0.5, min, max, 5000);

    QCOMPARE(*min, 0.0);
    QCOMPARE(*max, 1.0);
}

void SensorsPolicyTest::alertState_matches_unit_rules() {
    SensorReading rpm{.value = 900.0, .unit = SensorUnit::Rpm, .minValue = 1000.0};
    QVERIFY(SensorsPolicy::isAlertState(rpm));

    SensorReading temp{.value = 92.0, .unit = SensorUnit::Celsius, .maxValue = 85.0};
    QVERIFY(SensorsPolicy::isAlertState(temp));
    SensorReading tempF{.value = 200.0, .unit = SensorUnit::Fahrenheit, .maxValue = 185.0};
    QVERIFY(SensorsPolicy::isAlertState(tempF));

    SensorReading volt{.value = 1.35, .unit = SensorUnit::Volt, .minValue = 1.0, .maxValue = 1.3};
    QVERIFY(SensorsPolicy::isAlertState(volt));

    SensorReading normal{.value = 42.0, .unit = SensorUnit::Watt, .minValue = 1.0, .maxValue = 100.0};
    QVERIFY(!SensorsPolicy::isAlertState(normal));

    SensorReading wattOver{.value = 120.0, .unit = SensorUnit::Watt, .maxValue = 100.0};
    QVERIFY(SensorsPolicy::isAlertState(wattOver));

    SensorReading ampOver{.value = 15.0, .unit = SensorUnit::Ampere, .maxValue = 10.0};
    QVERIFY(SensorsPolicy::isAlertState(ampOver));

    SensorReading ampNormal{.value = 5.0, .unit = SensorUnit::Ampere, .minValue = 0.0, .maxValue = 10.0};
    QVERIFY(!SensorsPolicy::isAlertState(ampNormal));

    SensorReading milliwattOver{.value = 550.0, .unit = SensorUnit::Milliwatt, .maxValue = 500.0};
    QVERIFY(SensorsPolicy::isAlertState(milliwattOver));

    SensorReading milliampNormal{.value = 300.0, .unit = SensorUnit::Milliampere, .minValue = 0.0, .maxValue = 500.0};
    QVERIFY(!SensorsPolicy::isAlertState(milliampNormal));
}

void SensorsPolicyTest::rangeFraction_maps_value_into_limits() {
    SensorReading noRange{.value = 5.0, .unit = SensorUnit::Volt};
    QVERIFY(!SensorsPolicy::rangeFraction(noRange).has_value());

    SensorReading half{.value = 50.0, .unit = SensorUnit::Celsius, .minValue = 0.0, .maxValue = 100.0};
    QCOMPARE(*SensorsPolicy::rangeFraction(half), 0.5);

    SensorReading above{.value = 120.0, .unit = SensorUnit::Celsius, .minValue = 0.0, .maxValue = 100.0};
    QCOMPARE(*SensorsPolicy::rangeFraction(above), 1.0);

    SensorReading below{.value = -396.0, .unit = SensorUnit::Milliampere, .minValue = 0.0, .maxValue = 1000.0};
    QCOMPARE(*SensorsPolicy::rangeFraction(below), 0.0);

    // Degenerate range (max <= min) falls back to a one-unit span instead of dividing by zero.
    SensorReading degenerate{.value = 3.0, .unit = SensorUnit::Volt, .minValue = 3.0, .maxValue = 3.0};
    QCOMPARE(*SensorsPolicy::rangeFraction(degenerate), 0.0);
}

QTEST_APPLESS_MAIN(SensorsPolicyTest)
#include "test_sensors_policy.moc"
