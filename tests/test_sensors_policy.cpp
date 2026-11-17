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
    void scale_firmware_limit_decides_the_range();
    void scale_without_limits_follows_the_first_value();
    void scale_latch_holds_within_the_hysteresis();
    void scale_from_a_limit_survives_a_missing_limit();
    void scale_latch_out_of_range_is_ignored();
    void signed_current_without_limits_mirrors_range();
    void signed_power_with_native_max_only_mirrors_range();
    void negative_voltage_keeps_zero_minimum();
    void alertState_matches_unit_rules();
    void alertState_ignores_default_ranges();
    void alertState_temperature_without_firmware_max_uses_fallback();
    void rangeFraction_maps_value_into_limits();
    void implausibleLimits_temperature_sentinels_dropped();
    void implausibleLimits_inverted_or_zero_limits_dropped();
    void implausibleLimits_plausible_limits_kept();
    void missingValue_never_alerts_and_has_no_fraction();
    void missingValue_keeps_latched_scale();
    void missingValue_yields_valid_range();
    void plausibleTemperatureLimit_rejects_placeholders();
    void pwm_and_humidity_default_to_percent_range();
    void energy_and_frequency_get_no_default_range();
    void new_quantities_scale_and_alert();
    void pwmDutyPercent_maps_full_scale();
    void alertState_tells_which_limit_was_crossed();
};

void SensorsPolicyTest::temperatures_defaultRange_applied_when_missing() {
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorUnit::Celsius, 42.0, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QCOMPARE(*min, 0.0);
    QCOMPARE(*max, 100.0);
}

void SensorsPolicyTest::fans_defaultMax_uses_runtime_config() {
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorUnit::Rpm, 1800.0, min, max, 7200);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QCOMPARE(*min, 0.0);
    QCOMPARE(*max, 7200.0);
}

void SensorsPolicyTest::voltages_defaults_are_filled() {
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorUnit::Volt, 1.2, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QCOMPARE(*min, 0.0);
    QCOMPARE(*max, 1.8);
}

void SensorsPolicyTest::currents_defaults_are_filled() {
    std::optional<double> min;
    std::optional<double> max = 10.0;

    SensorsPolicy::applyDefaultRangePolicy(SensorUnit::Ampere, 6.0, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QCOMPARE(*min, 2.0);
    QCOMPARE(*max, 10.0);
}

void SensorsPolicyTest::power_defaults_are_filled() {
    std::optional<double> min = 20.0;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorUnit::Watt, 35.0, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QCOMPARE(*min, 20.0);
    QCOMPARE(*max, 35.0);
}

void SensorsPolicyTest::power_defaults_both_missing() {
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorUnit::Watt, 80.0, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QCOMPARE(*min, 0.0);
    QCOMPARE(*max, 120.0);
}

void SensorsPolicyTest::other_defaults_are_filled() {
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorUnit::Unknown, 4.0, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QCOMPARE(*min, 2.0);
    QCOMPARE(*max, 6.0);
}

void SensorsPolicyTest::existing_bounds_not_overwritten() {
    std::optional<double> min = 5.0;
    std::optional<double> max = 42.0;

    SensorsPolicy::applyDefaultRangePolicy(SensorUnit::Volt, 12.0, min, max, 5000);

    QCOMPARE(*min, 5.0);
    QCOMPARE(*max, 42.0);
}

void SensorsPolicyTest::nan_input_yields_finite_range() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorUnit::Unknown, nan, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QVERIFY(std::isfinite(*min));
    QVERIFY(std::isfinite(*max));
}

void SensorsPolicyTest::inf_input_yields_finite_range() {
    const double inf = std::numeric_limits<double>::infinity();
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorUnit::Volt, inf, min, max, 5000);

    QVERIFY(min.has_value());
    QVERIFY(max.has_value());
    QVERIFY(std::isfinite(*min));
    QVERIFY(std::isfinite(*max));
}

namespace {
    using SensorsPolicy::ScaleLatch;
    constexpr SensorScale kMilli{SiPrefix::Milli, 0};
    constexpr SensorScale kBase3{SiPrefix::None, 3}; // "1.052"
    constexpr SensorScale kBase2{SiPrefix::None, 2}; // "12.10"
    constexpr SensorScale kBase1{SiPrefix::None, 1}; // "230.1"
    constexpr SensorScale kKilo3{SiPrefix::Kilo, 3}; // "1.234 k"

    SensorScale scale(const SensorUnit unit, const std::optional<double> value,
                      const std::optional<double> min = std::nullopt, const std::optional<double> max = std::nullopt,
                      std::optional<ScaleLatch> *latch = nullptr) {
        return SensorsPolicy::chooseScale(unit, value, min, max, latch);
    }
}

void SensorsPolicyTest::scale_firmware_limit_decides_the_range() {
    // Each row of the range table, decided by the limit and not by the value.
    QCOMPARE(scale(SensorUnit::Volt, 0.5, std::nullopt, 0.95), kMilli);
    QCOMPARE(scale(SensorUnit::Volt, 0.5, 0.2, 1.488), kBase3);
    QCOMPARE(scale(SensorUnit::Volt, 12.1, 10.8, 13.2), kBase2);
    QCOMPARE(scale(SensorUnit::Volt, 1.0, std::nullopt, 250.0), kBase1);
    QCOMPARE(scale(SensorUnit::Watt, 0.926, std::nullopt, 0.5), kMilli);
    QCOMPARE(scale(SensorUnit::Watt, 35.0, std::nullopt, 1500.0), kKilo3);
    QCOMPARE(scale(SensorUnit::Ampere, 0.5, 0.0, 10.0), kBase2);
    QCOMPARE(scale(SensorUnit::Celsius, 40.0, std::nullopt, 105.0), (SensorScale{SiPrefix::None, 1}));
    QCOMPARE(scale(SensorUnit::Rpm, 1200.0, 500.0, std::nullopt), (SensorScale{SiPrefix::None, 0}));
    // A nonzero minimum decides when the maximum is zero; a zero limit carries no scale.
    QCOMPARE(scale(SensorUnit::Ampere, 0.2, -0.5, 0.0), kMilli);
    QCOMPARE(scale(SensorUnit::Ampere, 12.0, 0.0, std::nullopt), kBase2);
}

void SensorsPolicyTest::scale_without_limits_follows_the_first_value() {
    QCOMPARE(scale(SensorUnit::Volt, 0.88), kMilli);
    QCOMPARE(scale(SensorUnit::Volt, 1.052), kBase3);
    QCOMPARE(scale(SensorUnit::Volt, 12.1), kBase2);
    QCOMPARE(scale(SensorUnit::Volt, 230.1), kBase1);
    QCOMPARE(scale(SensorUnit::Ampere, 0.003), kMilli);
    QCOMPARE(scale(SensorUnit::Ampere, -0.396), kMilli);
    QCOMPARE(scale(SensorUnit::Watt, 1234.0), kKilo3);
    QCOMPARE(scale(SensorUnit::Celsius, 0.5), (SensorScale{SiPrefix::None, 1}));
    // Rounding up to the next decade belongs to the next range.
    QCOMPARE(scale(SensorUnit::Volt, 0.9996), kBase3);
    QCOMPARE(scale(SensorUnit::Volt, 9.9996), kBase2);
}

void SensorsPolicyTest::scale_latch_holds_within_the_hysteresis() {
    std::optional<ScaleLatch> latch;
    QCOMPARE(scale(SensorUnit::Watt, 0.9827, std::nullopt, std::nullopt, &latch), kMilli);
    QCOMPARE(latch, std::optional(ScaleLatch{0, false}));
    // Crossing 1 W, up to just below ten times the range: still mW.
    QCOMPARE(scale(SensorUnit::Watt, 1.25, std::nullopt, std::nullopt, &latch), kMilli);
    QCOMPARE(scale(SensorUnit::Watt, 9.9, std::nullopt, std::nullopt, &latch), kMilli);
    // From ten times the upper bound the value's own range takes over.
    QCOMPARE(scale(SensorUnit::Watt, 10.0, std::nullopt, std::nullopt, &latch), kBase2);
    QCOMPARE(latch, std::optional(ScaleLatch{2, false}));
    // Down to a tenth of the range's lower bound (10 W) it stays.
    QCOMPARE(scale(SensorUnit::Watt, 1.0, std::nullopt, std::nullopt, &latch), kBase2);
    QCOMPARE(scale(SensorUnit::Watt, 0.99, std::nullopt, std::nullopt, &latch), kMilli);

    // A volt rail latched at "1.052 V" keeps it from 0.100 V up to 99.999 V.
    std::optional<ScaleLatch> rail;
    QCOMPARE(scale(SensorUnit::Volt, 1.2, std::nullopt, std::nullopt, &rail), kBase3);
    QCOMPARE(scale(SensorUnit::Volt, 0.1, std::nullopt, std::nullopt, &rail), kBase3);
    QCOMPARE(scale(SensorUnit::Volt, 99.0, std::nullopt, std::nullopt, &rail), kBase3);
    QCOMPARE(scale(SensorUnit::Volt, 100.0, std::nullopt, std::nullopt, &rail), kBase1);
    QCOMPARE(scale(SensorUnit::Volt, 0.099, std::nullopt, std::nullopt, &rail), kMilli);

    // The previous mA/mW latch: base unit kept down to 0.1, milli kept up to 9999.
    std::optional<ScaleLatch> amps = ScaleLatch{1, false};
    QCOMPARE(scale(SensorUnit::Ampere, 0.1, std::nullopt, std::nullopt, &amps), kBase3);
    QCOMPARE(scale(SensorUnit::Ampere, 0.04, std::nullopt, std::nullopt, &amps), kMilli);
    QCOMPARE(scale(SensorUnit::Ampere, -9.99, std::nullopt, std::nullopt, &amps), kMilli);
    QCOMPARE(scale(SensorUnit::Ampere, -12.0, std::nullopt, std::nullopt, &amps), kBase2);
}

void SensorsPolicyTest::scale_from_a_limit_survives_a_missing_limit() {
    // L1: the limit decides "A", a failed limit read must not switch to mA.
    std::optional<ScaleLatch> latch;
    QCOMPARE(scale(SensorUnit::Ampere, 0.05, std::nullopt, 2.0, &latch), kBase3);
    QCOMPARE(latch, std::optional(ScaleLatch{1, true}));
    QCOMPARE(scale(SensorUnit::Ampere, 0.05, std::nullopt, std::nullopt, &latch), kBase3);
    QCOMPARE(scale(SensorUnit::Ampere, 0.001, std::nullopt, std::nullopt, &latch), kBase3);
    QCOMPARE(scale(SensorUnit::Ampere, 0.05, std::nullopt, 2.0, &latch), kBase3);

    // The limit wins over a latch made by values.
    std::optional<ScaleLatch> fromValue = ScaleLatch{0, false};
    QCOMPARE(scale(SensorUnit::Ampere, 0.5, 0.0, 10.0, &fromValue), kBase2);
    QCOMPARE(fromValue, std::optional(ScaleLatch{2, true}));
}

void SensorsPolicyTest::scale_latch_out_of_range_is_ignored() {
    std::optional<ScaleLatch> broken = ScaleLatch{99, false};
    QCOMPARE(scale(SensorUnit::Volt, 1.052, std::nullopt, std::nullopt, &broken), kBase3);
    QCOMPARE(broken, std::optional(ScaleLatch{1, false}));
}

void SensorsPolicyTest::signed_current_without_limits_mirrors_range() {
    // Discharging battery: -396 mA must lie inside the synthetic range (no alert).
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorUnit::Ampere, -396.0, min, max, 5000);

    QCOMPARE(*min, -594.0);
    QCOMPARE(*max, 594.0);
}

void SensorsPolicyTest::signed_power_with_native_max_only_mirrors_range() {
    std::optional<double> min;
    std::optional<double> max = 60.0;

    SensorsPolicy::applyDefaultRangePolicy(SensorUnit::Watt, -4.7, min, max, 5000);

    QCOMPARE(*min, -60.0);
    QCOMPARE(*max, 60.0);
}

void SensorsPolicyTest::negative_voltage_keeps_zero_minimum() {
    std::optional<double> min;
    std::optional<double> max;

    SensorsPolicy::applyDefaultRangePolicy(SensorUnit::Volt, -0.5, min, max, 5000);

    QCOMPARE(*min, 0.0);
    QCOMPARE(*max, 1.0);
}

void SensorsPolicyTest::alertState_matches_unit_rules() {
    SensorReading rpm{.value = 900.0, .unit = SensorUnit::Rpm, .firmwareMin = 1000.0};
    QVERIFY(SensorsPolicy::alertState(rpm) != AlertState::None);

    SensorReading temp{.value = 92.0, .unit = SensorUnit::Celsius, .firmwareMax = 85.0};
    QVERIFY(SensorsPolicy::alertState(temp) != AlertState::None);
    SensorReading tempF{.value = 200.0, .unit = SensorUnit::Fahrenheit, .firmwareMax = 185.0};
    QVERIFY(SensorsPolicy::alertState(tempF) != AlertState::None);

    SensorReading volt{.value = 1.35, .unit = SensorUnit::Volt, .firmwareMin = 1.0, .firmwareMax = 1.3};
    QVERIFY(SensorsPolicy::alertState(volt) != AlertState::None);

    SensorReading normal{.value = 42.0, .unit = SensorUnit::Watt, .firmwareMin = 1.0, .firmwareMax = 100.0};
    QCOMPARE(SensorsPolicy::alertState(normal), AlertState::None);

    SensorReading wattOver{.value = 120.0, .unit = SensorUnit::Watt, .firmwareMax = 100.0};
    QVERIFY(SensorsPolicy::alertState(wattOver) != AlertState::None);

    SensorReading ampOver{.value = 15.0, .unit = SensorUnit::Ampere, .firmwareMax = 10.0};
    QVERIFY(SensorsPolicy::alertState(ampOver) != AlertState::None);

    SensorReading ampNormal{.value = 5.0, .unit = SensorUnit::Ampere, .firmwareMin = 0.0, .firmwareMax = 10.0};
    QCOMPARE(SensorsPolicy::alertState(ampNormal), AlertState::None);

    SensorReading milliwattOver{.value = 0.55, .unit = SensorUnit::Watt, .firmwareMax = 0.5};
    QVERIFY(SensorsPolicy::alertState(milliwattOver) != AlertState::None);

    SensorReading milliampNormal{.value = 0.3, .unit = SensorUnit::Ampere, .firmwareMin = 0.0, .firmwareMax = 0.5};
    QCOMPARE(SensorsPolicy::alertState(milliampNormal), AlertState::None);
}

void SensorsPolicyTest::alertState_ignores_default_ranges() {
    // Bar graph ranges from the default policy are guesses, not limits.
    SensorReading idlePower{.value = 25.0, .unit = SensorUnit::Watt, .minValue = 40.0, .maxValue = 200.0,
                            .firmwareMax = 200.0};
    QCOMPARE(SensorsPolicy::alertState(idlePower), AlertState::None);
    SensorReading fan{.value = 0.0, .unit = SensorUnit::Rpm, .minValue = 0.0, .maxValue = 5000.0};
    QCOMPARE(SensorsPolicy::alertState(fan), AlertState::None);
}

void SensorsPolicyTest::alertState_temperature_without_firmware_max_uses_fallback() {
    SensorReading hot{.value = SensorsPolicy::kDefaultTemperatureMaxC + 2.0, .unit = SensorUnit::Celsius};
    QVERIFY(SensorsPolicy::alertState(hot) != AlertState::None);
    SensorReading warm{.value = SensorsPolicy::kDefaultTemperatureMaxC - 2.0, .unit = SensorUnit::Celsius};
    QCOMPARE(SensorsPolicy::alertState(warm), AlertState::None);
    SensorReading hotF{.value = 215.0, .unit = SensorUnit::Fahrenheit};
    QVERIFY(SensorsPolicy::alertState(hotF) != AlertState::None);
    // A firmware maximum above the fallback wins.
    SensorReading junction{.value = 105.0, .unit = SensorUnit::Celsius, .firmwareMax = 110.0};
    QCOMPARE(SensorsPolicy::alertState(junction), AlertState::None);
}

void SensorsPolicyTest::rangeFraction_maps_value_into_limits() {
    SensorReading noRange{.value = 5.0, .unit = SensorUnit::Volt};
    QVERIFY(!SensorsPolicy::rangeFraction(noRange).has_value());

    SensorReading half{.value = 50.0, .unit = SensorUnit::Celsius, .minValue = 0.0, .maxValue = 100.0};
    QCOMPARE(*SensorsPolicy::rangeFraction(half), 0.5);

    SensorReading above{.value = 120.0, .unit = SensorUnit::Celsius, .minValue = 0.0, .maxValue = 100.0};
    QCOMPARE(*SensorsPolicy::rangeFraction(above), 1.0);

    SensorReading below{.value = -0.396, .unit = SensorUnit::Ampere, .minValue = 0.0, .maxValue = 1.0};
    QCOMPARE(*SensorsPolicy::rangeFraction(below), 0.0);

    // Degenerate range (max <= min) falls back to a one-unit span instead of dividing by zero.
    SensorReading degenerate{.value = 3.0, .unit = SensorUnit::Volt, .minValue = 3.0, .maxValue = 3.0};
    QCOMPARE(*SensorsPolicy::rangeFraction(degenerate), 0.0);
}

void SensorsPolicyTest::implausibleLimits_temperature_sentinels_dropped() {
    std::optional<double> min = -273.15;
    std::optional<double> max = 65261.85;
    SensorsPolicy::dropImplausibleFirmwareLimits(SensorUnit::Celsius, min, max);
    QVERIFY(!min.has_value());
    QVERIFY(!max.has_value());

    // Only the sentinel goes; a real limit next to it stays.
    min = -273.15;
    max = 84.85;
    SensorsPolicy::dropImplausibleFirmwareLimits(SensorUnit::Celsius, min, max);
    QVERIFY(!min.has_value());
    QCOMPARE(max, std::optional(84.85));

    min = std::nullopt;
    max = std::numeric_limits<double>::quiet_NaN();
    SensorsPolicy::dropImplausibleFirmwareLimits(SensorUnit::Celsius, min, max);
    QVERIFY(!max.has_value());
}

void SensorsPolicyTest::implausibleLimits_inverted_or_zero_limits_dropped() {
    std::optional<double> min = 0.0;
    std::optional<double> max = 0.0;
    SensorsPolicy::dropImplausibleFirmwareLimits(SensorUnit::Volt, min, max);
    QVERIFY(!min.has_value());
    QVERIFY(!max.has_value());

    min = 5.0;
    max = 3.0;
    SensorsPolicy::dropImplausibleFirmwareLimits(SensorUnit::Volt, min, max);
    QVERIFY(!min.has_value());
    QVERIFY(!max.has_value());

    min = 1000.0;
    max = 0.0;
    SensorsPolicy::dropImplausibleFirmwareLimits(SensorUnit::Rpm, min, max);
    QCOMPARE(min, std::optional(1000.0));
    QVERIFY(!max.has_value());

    for (const SensorUnit unit: {SensorUnit::Volt, SensorUnit::Rpm, SensorUnit::Ampere, SensorUnit::Watt}) {
        min = std::nullopt;
        max = 0.0;
        SensorsPolicy::dropImplausibleFirmwareLimits(unit, min, max);
        QVERIFY(!max.has_value());
    }
}

void SensorsPolicyTest::implausibleLimits_plausible_limits_kept() {
    struct Case {
        SensorUnit unit;
        std::optional<double> min;
        std::optional<double> max;
    };
    const Case cases[] = {
        {SensorUnit::Celsius, -40.0, -10.0}, // freezer
        {SensorUnit::Celsius, std::nullopt, 0.0}, // freezer maximum
        {SensorUnit::Celsius, -55.0, 1800.0}, // thermocouple
        {SensorUnit::Rpm, 0.0, std::nullopt}, // semi-passive fan
        {SensorUnit::Volt, 0.0, 1.5},
        {SensorUnit::Ampere, -5.0, 3.0}, // battery
        {SensorUnit::Ampere, -5.0, 0.0}, // discharge only
        {SensorUnit::Ampere, 0.0, std::nullopt},
        {SensorUnit::Watt, std::nullopt, 0.5},
    };
    for (const Case &c: cases) {
        std::optional<double> min = c.min;
        std::optional<double> max = c.max;
        SensorsPolicy::dropImplausibleFirmwareLimits(c.unit, min, max);
        QCOMPARE(min, c.min);
        QCOMPARE(max, c.max);
    }
}

void SensorsPolicyTest::missingValue_never_alerts_and_has_no_fraction() {
    // Limits that a value of 0 would violate: a missing value must not read as 0.
    const auto missing = [](const SensorUnit unit, const std::optional<double> min, const std::optional<double> max) {
        SensorReading reading;
        reading.unit = unit;
        reading.minValue = reading.firmwareMin = min;
        reading.maxValue = reading.firmwareMax = max;
        return reading;
    };
    const SensorReading readings[] = {
        missing(SensorUnit::Rpm, 1000.0, 5000.0),
        missing(SensorUnit::Celsius, -40.0, -10.0),
        missing(SensorUnit::Volt, 11.0, 13.0),
        missing(SensorUnit::Ampere, 0.1, 0.5),
    };
    for (const SensorReading &reading: readings) {
        QVERIFY(!reading.hasValue());
        QCOMPARE(SensorsPolicy::alertState(reading), AlertState::None);
        QCOMPARE(SensorsPolicy::rangeFraction(reading), std::nullopt);
    }
}

void SensorsPolicyTest::missingValue_keeps_latched_scale() {
    for (const ScaleLatch latched: {ScaleLatch{0, false}, ScaleLatch{2, false}, ScaleLatch{1, true}}) {
        std::optional<ScaleLatch> latch = latched;
        QCOMPARE(scale(SensorUnit::Watt, std::nullopt, std::nullopt, std::nullopt, &latch),
                 SensorFormat::scaleRanges(SensorUnit::Watt).at(latched.rangeIndex).scale);
        QCOMPARE(latch, std::optional(latched));
    }

    // Nothing latched yet: the base scale, and the first real value still decides.
    std::optional<ScaleLatch> latch;
    QCOMPARE(scale(SensorUnit::Ampere, std::nullopt, std::nullopt, std::nullopt, &latch), kBase3);
    QCOMPARE(latch, std::nullopt);

    // Firmware limits decide even without a value.
    QCOMPARE(scale(SensorUnit::Ampere, std::nullopt, std::nullopt, 0.5, &latch), kMilli);
    QCOMPARE(latch, std::optional(ScaleLatch{0, true}));
}

void SensorsPolicyTest::missingValue_yields_valid_range() {
    for (const SensorUnit unit: {SensorUnit::Celsius, SensorUnit::Fahrenheit, SensorUnit::Rpm, SensorUnit::Volt,
                                 SensorUnit::Ampere, SensorUnit::Watt, SensorUnit::Percent,
                                 SensorUnit::RelativeHumidity, SensorUnit::Unknown}) {
        std::optional<double> min;
        std::optional<double> max;
        SensorsPolicy::applyDefaultRangePolicy(unit, std::nullopt, min, max, 5000);
        QVERIFY(min.has_value() && max.has_value());
        QVERIFY(*max > *min);

        // L5: a lone firmware minimum above the default maximum must not invert the range.
        std::optional<double> onlyMin = 5.0;
        std::optional<double> noMax;
        SensorsPolicy::applyDefaultRangePolicy(unit, std::nullopt, onlyMin, noMax, 5000);
        QCOMPARE(onlyMin, std::optional(5.0));
        QVERIFY(noMax.has_value() && *noMax > 5.0);
    }
}

void SensorsPolicyTest::plausibleTemperatureLimit_rejects_placeholders() {
    QVERIFY(SensorsPolicy::isPlausibleTemperatureLimitC(84.85));
    QVERIFY(SensorsPolicy::isPlausibleTemperatureLimitC(-40.0));
    QVERIFY(SensorsPolicy::isPlausibleTemperatureLimitC(SensorsPolicy::kTemperatureLimitMaxPlausibleC));
    QVERIFY(!SensorsPolicy::isPlausibleTemperatureLimitC(65261.85));
    QVERIFY(!SensorsPolicy::isPlausibleTemperatureLimitC(-273.15));
    QVERIFY(!SensorsPolicy::isPlausibleTemperatureLimitC(std::numeric_limits<double>::quiet_NaN()));
}

void SensorsPolicyTest::pwm_and_humidity_default_to_percent_range() {
    for (const SensorUnit unit: {SensorUnit::Percent, SensorUnit::RelativeHumidity}) {
        std::optional<double> min;
        std::optional<double> max;
        SensorsPolicy::applyDefaultRangePolicy(unit, 42.0, min, max, 5000);
        QCOMPARE(min, std::optional(0.0));
        QCOMPARE(max, std::optional(100.0));
        // Not the fan fallback, although PWM outputs are shown with the fans.
        QVERIFY(*max != 5000.0);
    }
}

void SensorsPolicyTest::energy_and_frequency_get_no_default_range() {
    for (const SensorUnit unit: {SensorUnit::Joule, SensorUnit::Hertz}) {
        for (const std::optional<double> value: {std::optional(123456.0), std::optional<double>()}) {
            std::optional<double> min;
            std::optional<double> max;
            SensorsPolicy::applyDefaultRangePolicy(unit, value, min, max, 5000);
            QCOMPARE(min, std::nullopt);
            QCOMPARE(max, std::nullopt);
            const SensorReading reading{.value = value, .unit = unit, .minValue = min, .maxValue = max};
            QCOMPARE(SensorsPolicy::rangeFraction(reading), std::nullopt);
        }
    }
}

void SensorsPolicyTest::new_quantities_scale_and_alert() {
    QCOMPARE(SensorsPolicy::chooseScale(SensorUnit::Percent, 42.4, std::nullopt, std::nullopt), (SensorScale{SiPrefix::None, 0}));
    QCOMPARE(SensorsPolicy::chooseScale(SensorUnit::RelativeHumidity, 55.6, std::nullopt, std::nullopt),
             (SensorScale{SiPrefix::None, 0}));
    QCOMPARE(SensorsPolicy::chooseScale(SensorUnit::Joule, 12.5, std::nullopt, std::nullopt), (SensorScale{SiPrefix::None, 2}));
    QCOMPARE(SensorsPolicy::chooseScale(SensorUnit::Joule, 4321.0, std::nullopt, std::nullopt), (SensorScale{SiPrefix::Kilo, 3}));
    QCOMPARE(SensorsPolicy::chooseScale(SensorUnit::Joule, 2.5e9, std::nullopt, std::nullopt), (SensorScale{SiPrefix::Giga, 3}));
    QCOMPARE(SensorsPolicy::chooseScale(SensorUnit::Hertz, 50.0, std::nullopt, std::nullopt), (SensorScale{SiPrefix::None, 2}));
    QCOMPARE(SensorsPolicy::chooseScale(SensorUnit::Hertz, 25000.0, std::nullopt, std::nullopt), (SensorScale{SiPrefix::Kilo, 2}));
    // A GPU clock: "1850 MHz", not GHz.
    QCOMPARE(SensorsPolicy::chooseScale(SensorUnit::Hertz, 1.85e9, std::nullopt, std::nullopt), (SensorScale{SiPrefix::Mega, 0}));

    // PWM and energy have no limits to violate; humidity and frequency follow firmware limits.
    QCOMPARE(SensorsPolicy::alertState({.value = 100.0, .unit = SensorUnit::Percent, .firmwareMax = 50.0}), AlertState::None);
    QCOMPARE(SensorsPolicy::alertState({.value = 1e9, .unit = SensorUnit::Joule, .firmwareMax = 1.0}), AlertState::None);
    QVERIFY(SensorsPolicy::alertState({.value = 95.0, .unit = SensorUnit::RelativeHumidity, .firmwareMax = 90.0}) != AlertState::None);
    QVERIFY(SensorsPolicy::alertState({.value = 10.0, .unit = SensorUnit::Hertz, .firmwareMin = 45.0}) != AlertState::None);

    // A zero maximum is an unset limit for the new quantities too.
    for (const SensorUnit unit: {SensorUnit::Percent, SensorUnit::RelativeHumidity, SensorUnit::Joule, SensorUnit::Hertz}) {
        std::optional<double> min;
        std::optional<double> max = 0.0;
        SensorsPolicy::dropImplausibleFirmwareLimits(unit, min, max);
        QCOMPARE(max, std::nullopt);
    }
}

void SensorsPolicyTest::pwmDutyPercent_maps_full_scale() {
    QCOMPARE(SensorsPolicy::pwmDutyPercent(0.0), 0.0);
    QCOMPARE(SensorsPolicy::pwmDutyPercent(255.0), 100.0);
    QCOMPARE(SensorsPolicy::pwmDutyPercent(127.5), 50.0);
    QVERIFY(qFuzzyCompare(SensorsPolicy::pwmDutyPercent(69.0), 69.0 / 2.55));
    // Out of range values (e.g. a sensors.conf compute statement) are clamped.
    QCOMPARE(SensorsPolicy::pwmDutyPercent(300.0), 100.0);
    QCOMPARE(SensorsPolicy::pwmDutyPercent(-1.0), 0.0);
}

void SensorsPolicyTest::alertState_tells_which_limit_was_crossed() {
    const auto state = [](const SensorUnit unit, const double value, const std::optional<double> min,
                          const std::optional<double> max) {
        return SensorsPolicy::alertState({.value = value, .unit = unit, .firmwareMin = min, .firmwareMax = max});
    };
    QCOMPARE(state(SensorUnit::Volt, 13.0, 11.0, 12.6), AlertState::High);
    QCOMPARE(state(SensorUnit::Volt, 10.9, 11.0, 12.6), AlertState::Low);
    QCOMPARE(state(SensorUnit::Volt, 12.0, 11.0, 12.6), AlertState::None);
    QCOMPARE(state(SensorUnit::Ampere, -2.5, -2.0, 2.0), AlertState::Low);
    QCOMPARE(state(SensorUnit::Watt, 0.55, std::nullopt, 0.5), AlertState::High);
    QCOMPARE(state(SensorUnit::RelativeHumidity, 5.0, 10.0, 90.0), AlertState::Low);
    QCOMPARE(state(SensorUnit::Hertz, 60.0, 45.0, 55.0), AlertState::High);
    // Fans only alert low, temperatures only high.
    QCOMPARE(state(SensorUnit::Rpm, 120.0, 300.0, std::nullopt), AlertState::Low);
    QCOMPARE(state(SensorUnit::Rpm, 9000.0, 300.0, 5000.0), AlertState::None);
    QCOMPARE(state(SensorUnit::Celsius, 92.3, 0.0, 85.0), AlertState::High);
    QCOMPARE(state(SensorUnit::Celsius, -45.0, -40.0, -10.0), AlertState::None);
    QCOMPARE(state(SensorUnit::Celsius, 101.0, std::nullopt, std::nullopt), AlertState::High);
    QCOMPARE(state(SensorUnit::Fahrenheit, 213.0, std::nullopt, std::nullopt), AlertState::High);
    QCOMPARE(state(SensorUnit::Percent, 100.0, std::nullopt, 50.0), AlertState::None);
    QCOMPARE(SensorsPolicy::alertState({.value = 10.9, .unit = SensorUnit::Volt, .firmwareMin = 11.0}), AlertState::Low);
    QCOMPARE(SensorsPolicy::alertState({.unit = SensorUnit::Volt, .firmwareMin = 11.0}), AlertState::None);
}

QTEST_APPLESS_MAIN(SensorsPolicyTest)
#include "test_sensors_policy.moc"
