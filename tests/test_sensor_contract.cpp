// SPDX-License-Identifier: GPL-2.0-or-later

#include "sensors_backend.h"
#include "lcd_segment_font.h"

#include <QtTest/QtTest>

// Verifies SensorUnit symbol contracts and that every rendered unit is drawable by the segment LCD.
class SensorContractTest final : public QObject {
    Q_OBJECT

private slots:
    void known_units_have_symbols_and_glyphs();
    void unknown_unit_has_no_symbol_and_no_glyph();
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

QTEST_APPLESS_MAIN(SensorContractTest)
#include "test_sensor_contract.moc"
