// SPDX-License-Identifier: GPL-2.0-or-later

#include "lcd_display_widget.h"

#include <QtTest/QtTest>

// Verifies LCD value formatting and alert-state decisions independent of painting.
class LcdLogicTest final : public QObject {
    Q_OBJECT

private slots:
    void valueDigits_formats_by_unit();
    void alertState_matches_unit_rules();
};

void LcdLogicTest::valueDigits_formats_by_unit() {
    SensorReading rpm{.value = 1534.0, .unit = SensorUnit::Rpm};
    SensorReading celsius{.value = 38.25, .unit = SensorUnit::Celsius};
    SensorReading fahrenheit{.value = 100.75, .unit = SensorUnit::Fahrenheit};
    SensorReading volt{.value = 1.234, .unit = SensorUnit::Volt};

    QCOMPARE(LcdDisplayWidget::valueDigitsFor(rpm), QStringLiteral(" 1534"));
    QCOMPARE(LcdDisplayWidget::valueDigitsFor(celsius), QStringLiteral("  38.3"));
    QCOMPARE(LcdDisplayWidget::valueDigitsFor(fahrenheit), QStringLiteral(" 100.8"));
    QCOMPARE(LcdDisplayWidget::valueDigitsFor(volt), QStringLiteral("  1.23"));
}

void LcdLogicTest::alertState_matches_unit_rules() {
    SensorReading rpm{.value = 900.0, .unit = SensorUnit::Rpm, .hasRange = true, .hasMin = true, .minValue = 1000.0};
    QVERIFY(LcdDisplayWidget::isAlertState(rpm));

    SensorReading temp{.value = 92.0, .unit = SensorUnit::Celsius, .hasRange = true, .hasMax = true, .maxValue = 85.0};
    QVERIFY(LcdDisplayWidget::isAlertState(temp));
    SensorReading tempF{.value = 200.0, .unit = SensorUnit::Fahrenheit, .hasRange = true, .hasMax = true, .maxValue = 185.0};
    QVERIFY(LcdDisplayWidget::isAlertState(tempF));

    SensorReading volt{.value = 1.35, .unit = SensorUnit::Volt, .hasRange = true, .hasMin = true, .hasMax = true, .minValue = 1.0, .maxValue = 1.3};
    QVERIFY(LcdDisplayWidget::isAlertState(volt));

    SensorReading normal{.value = 42.0, .unit = SensorUnit::Watt, .hasRange = true, .hasMin = true, .hasMax = true, .minValue = 1.0, .maxValue = 100.0};
    QVERIFY(!LcdDisplayWidget::isAlertState(normal));
}

QTEST_APPLESS_MAIN(LcdLogicTest)
#include "test_lcd_logic.moc"
