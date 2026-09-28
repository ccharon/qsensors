// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/sensor_reading.h"
#include "ui/widgets/lcd_segment_font.h"
#include "sensors/sensor_format.h"

#include <QtTest/QtTest>

// Verifies SensorUnit symbols, value formatting and that every unit is drawable by the segment LCD.
class SensorContractTest final : public QObject {
    Q_OBJECT

private slots:
    void known_units_have_symbols_and_glyphs();
    void unknown_unit_has_no_symbol_and_no_glyph();
    void valueDigits_formats_by_unit();
    void valueWithUnit_appends_symbol();
};

void SensorContractTest::known_units_have_symbols_and_glyphs() {
    const SensorUnit units[] = {
        SensorUnit::Celsius,
        SensorUnit::Fahrenheit,
        SensorUnit::Volt,
        SensorUnit::Rpm,
        SensorUnit::Ampere,
        SensorUnit::Milliampere,
        SensorUnit::Watt,
        SensorUnit::Milliwatt,
    };

    // Every known unit is rendered on the segment LCD; all characters must resolve.
    for (const SensorUnit unit: units) {
        const QString symbol = sensorUnitSymbol(unit);
        QVERIFY2(!symbol.isEmpty(), "Known unit must have symbol");

        QVERIFY2(LcdSegmentFont::supports(symbol), qPrintable(symbol + QStringLiteral(" must have segment glyphs")));
    }
}

void SensorContractTest::unknown_unit_has_no_symbol_and_no_glyph() {
    const QString symbol = sensorUnitSymbol(SensorUnit::Unknown);
    QVERIFY(symbol.isEmpty());
    QVERIFY(LcdSegmentFont::layoutText(QString(), symbol, 30.0).isEmpty());
}

void SensorContractTest::valueDigits_formats_by_unit() {
    SensorReading rpm{.value = 1534.0, .unit = SensorUnit::Rpm};
    SensorReading celsius{.value = 38.25, .unit = SensorUnit::Celsius};
    SensorReading fahrenheit{.value = 100.75, .unit = SensorUnit::Fahrenheit};
    SensorReading volt{.value = 1.234, .unit = SensorUnit::Volt};
    SensorReading milliamp{.value = 301.0, .unit = SensorUnit::Milliampere};
    SensorReading milliwatt{.value = 3.61, .unit = SensorUnit::Milliwatt};
    SensorReading dischargingBattery{.value = -396.0, .unit = SensorUnit::Milliampere};

    QCOMPARE(SensorFormat::valueDigits(rpm.unit, rpm.value), QStringLiteral(" 1534"));
    QCOMPARE(SensorFormat::valueDigits(celsius.unit, celsius.value), QStringLiteral("  38.3"));
    QCOMPARE(SensorFormat::valueDigits(fahrenheit.unit, fahrenheit.value), QStringLiteral(" 100.8"));
    QCOMPARE(SensorFormat::valueDigits(volt.unit, volt.value), QStringLiteral("  1.23"));
    QCOMPARE(SensorFormat::valueDigits(milliamp.unit, milliamp.value), QStringLiteral(" 301.0"));
    QCOMPARE(SensorFormat::valueDigits(milliwatt.unit, milliwatt.value), QStringLiteral("   3.6"));
    QCOMPARE(SensorFormat::valueDigits(dischargingBattery.unit, dischargingBattery.value), QStringLiteral("-396.0"));
}

void SensorContractTest::valueWithUnit_appends_symbol() {
    QCOMPARE(SensorFormat::valueWithUnit(SensorUnit::Volt, 9.888), QStringLiteral("9.89 V"));
    QCOMPARE(SensorFormat::valueWithUnit(SensorUnit::Rpm, 1534.0), QStringLiteral("1534 RPM"));
    QCOMPARE(SensorFormat::valueWithUnit(SensorUnit::Unknown, 1.5), QStringLiteral("1.50"));
}

QTEST_APPLESS_MAIN(SensorContractTest)
#include "test_sensor_contract.moc"
