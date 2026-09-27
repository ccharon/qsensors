// SPDX-License-Identifier: GPL-2.0-or-later

#include "lcd_display_widget.h"
#include "sensor_value_widget.h"

#include <QtTest/QtTest>

// Verifies LCD value formatting and alert-state decisions independent of painting.
class LcdLogicTest final : public QObject {
    Q_OBJECT

private slots:
    void valueDigits_formats_by_unit();
    void alertState_matches_unit_rules();
    void rangeFraction_maps_value_into_limits();
    void barGraph_lights_proportional_segments();
    void detailsToolTip_lists_chip_and_limits_in_display_units();
};

void LcdLogicTest::valueDigits_formats_by_unit() {
    SensorReading rpm{.value = 1534.0, .unit = SensorUnit::Rpm};
    SensorReading celsius{.value = 38.25, .unit = SensorUnit::Celsius};
    SensorReading fahrenheit{.value = 100.75, .unit = SensorUnit::Fahrenheit};
    SensorReading volt{.value = 1.234, .unit = SensorUnit::Volt};
    SensorReading milliamp{.value = 301.0, .unit = SensorUnit::Milliampere};
    SensorReading milliwatt{.value = 3.61, .unit = SensorUnit::Milliwatt};
    SensorReading dischargingBattery{.value = -396.0, .unit = SensorUnit::Milliampere};

    QCOMPARE(LcdDisplayWidget::valueDigitsFor(rpm), QStringLiteral(" 1534"));
    QCOMPARE(LcdDisplayWidget::valueDigitsFor(celsius), QStringLiteral("  38.3"));
    QCOMPARE(LcdDisplayWidget::valueDigitsFor(fahrenheit), QStringLiteral(" 100.8"));
    QCOMPARE(LcdDisplayWidget::valueDigitsFor(volt), QStringLiteral("  1.23"));
    QCOMPARE(LcdDisplayWidget::valueDigitsFor(milliamp), QStringLiteral(" 301.0"));
    QCOMPARE(LcdDisplayWidget::valueDigitsFor(milliwatt), QStringLiteral("   3.6"));
    QCOMPARE(LcdDisplayWidget::valueDigitsFor(dischargingBattery), QStringLiteral("-396.0"));
}

void LcdLogicTest::alertState_matches_unit_rules() {
    SensorReading rpm{.value = 900.0, .unit = SensorUnit::Rpm, .minValue = 1000.0};
    QVERIFY(LcdDisplayWidget::isAlertState(rpm));

    SensorReading temp{.value = 92.0, .unit = SensorUnit::Celsius, .maxValue = 85.0};
    QVERIFY(LcdDisplayWidget::isAlertState(temp));
    SensorReading tempF{.value = 200.0, .unit = SensorUnit::Fahrenheit, .maxValue = 185.0};
    QVERIFY(LcdDisplayWidget::isAlertState(tempF));

    SensorReading volt{.value = 1.35, .unit = SensorUnit::Volt, .minValue = 1.0, .maxValue = 1.3};
    QVERIFY(LcdDisplayWidget::isAlertState(volt));

    SensorReading normal{.value = 42.0, .unit = SensorUnit::Watt, .minValue = 1.0, .maxValue = 100.0};
    QVERIFY(!LcdDisplayWidget::isAlertState(normal));

    SensorReading wattOver{.value = 120.0, .unit = SensorUnit::Watt, .maxValue = 100.0};
    QVERIFY(LcdDisplayWidget::isAlertState(wattOver));

    SensorReading ampOver{.value = 15.0, .unit = SensorUnit::Ampere, .maxValue = 10.0};
    QVERIFY(LcdDisplayWidget::isAlertState(ampOver));

    SensorReading ampNormal{.value = 5.0, .unit = SensorUnit::Ampere, .minValue = 0.0, .maxValue = 10.0};
    QVERIFY(!LcdDisplayWidget::isAlertState(ampNormal));

    SensorReading milliwattOver{.value = 550.0, .unit = SensorUnit::Milliwatt, .maxValue = 500.0};
    QVERIFY(LcdDisplayWidget::isAlertState(milliwattOver));

    SensorReading milliampNormal{.value = 300.0, .unit = SensorUnit::Milliampere, .minValue = 0.0, .maxValue = 500.0};
    QVERIFY(!LcdDisplayWidget::isAlertState(milliampNormal));
}

void LcdLogicTest::rangeFraction_maps_value_into_limits() {
    SensorReading noRange{.value = 5.0, .unit = SensorUnit::Volt};
    QVERIFY(!LcdDisplayWidget::rangeFraction(noRange).has_value());

    SensorReading half{.value = 50.0, .unit = SensorUnit::Celsius, .minValue = 0.0, .maxValue = 100.0};
    QCOMPARE(*LcdDisplayWidget::rangeFraction(half), 0.5);

    SensorReading above{.value = 120.0, .unit = SensorUnit::Celsius, .minValue = 0.0, .maxValue = 100.0};
    QCOMPARE(*LcdDisplayWidget::rangeFraction(above), 1.0);

    SensorReading below{.value = -396.0, .unit = SensorUnit::Milliampere, .minValue = 0.0, .maxValue = 1000.0};
    QCOMPARE(*LcdDisplayWidget::rangeFraction(below), 0.0);

    // Degenerate range (max <= min) falls back to a one-unit span instead of dividing by zero.
    SensorReading degenerate{.value = 3.0, .unit = SensorUnit::Volt, .minValue = 3.0, .maxValue = 3.0};
    QCOMPARE(*LcdDisplayWidget::rangeFraction(degenerate), 0.0);
}

void LcdLogicTest::barGraph_lights_proportional_segments() {
    SensorReading quarter{.value = 25.0, .unit = SensorUnit::Celsius, .minValue = 0.0, .maxValue = 100.0};
    QCOMPARE(LcdDisplayWidget::litBarSegments(quarter, 20), 5);
    QCOMPARE(LcdDisplayWidget::litBarSegments(quarter, 0), 0);

    SensorReading noRange{.value = 25.0, .unit = SensorUnit::Celsius};
    QCOMPARE(LcdDisplayWidget::litBarSegments(noRange, 20), 0);

    const auto segments = LcdDisplayWidget::barGraphSegments(QRectF(4, 30, 142, 4));
    QVERIFY(segments.size() >= 20);
    for (qsizetype i = 1; i < segments.size(); ++i) {
        QVERIFY(segments.at(i).boundingRect().left() > segments.at(i - 1).boundingRect().left());
    }
    QVERIFY(segments.last().boundingRect().right() <= 4 + 142 + 1e-6);
}

void LcdLogicTest::detailsToolTip_lists_chip_and_limits_in_display_units() {
    SensorReading battery{.chip = QStringLiteral("macsmc_battery-isa-0000"), .feature = QStringLiteral("in0"),
                          .value = 11.9, .unit = SensorUnit::Volt, .minValue = 9.888, .maxValue = 13.325};
    const QString tip = SensorValueWidget::detailsToolTip(battery);
    QVERIFY(tip.contains(QStringLiteral("<b>in0</b>")));
    QVERIFY(tip.contains(QStringLiteral("macsmc_battery-isa-0000")));
    QVERIFY(tip.contains(QStringLiteral("9.89 V")));
    QVERIFY(tip.contains(QStringLiteral("13.32 V")) || tip.contains(QStringLiteral("13.33 V")));
    QVERIFY(!tip.contains(QStringLiteral("No limits")));

    // Names are escaped: sensors.conf labels are free text.
    SensorReading odd{.chip = QStringLiteral("c"), .feature = QStringLiteral("<CPU & SoC>"),
                      .value = 1.0, .unit = SensorUnit::Watt};
    const QString oddTip = SensorValueWidget::detailsToolTip(odd);
    QVERIFY(oddTip.contains(QStringLiteral("&lt;CPU &amp; SoC&gt;")));
    QVERIFY(oddTip.contains(QStringLiteral("No limits available")));
}

QTEST_APPLESS_MAIN(LcdLogicTest)
#include "test_lcd_logic.moc"
