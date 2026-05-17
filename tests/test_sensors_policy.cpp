// SPDX-License-Identifier: GPL-2.0-or-later

#include "sensors_policy.h"

#include <QtTest/QtTest>

// Verifies default-range policy behavior for normalized sensor categories.
class SensorsPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void temperatures_defaultRange_applied_when_missing();
    void fans_defaultMax_uses_runtime_config();
    void voltages_defaults_are_filled();
    void currents_defaults_are_filled();
    void power_defaults_are_filled();
    void other_defaults_are_filled();
    void existing_bounds_not_overwritten();
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

QTEST_APPLESS_MAIN(SensorsPolicyTest)
#include "test_sensors_policy.moc"
