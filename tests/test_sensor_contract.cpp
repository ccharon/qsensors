// SPDX-License-Identifier: GPL-2.0-or-later

#include "sensors_backend.h"
#include "lcd_glyph_atlas.h"

#include <QtTest/QtTest>

// Verifies SensorUnit symbol contracts and LCD glyph availability for rendered units.
class SensorContractTest final : public QObject {
    Q_OBJECT

private slots:
    void known_units_have_symbols_and_glyphs();
    void unknown_unit_has_no_symbol_and_no_glyph();
};

void SensorContractTest::known_units_have_symbols_and_glyphs() {
    const SensorUnit units[] = {
        SensorUnit::Celsius,
        SensorUnit::Volt,
        SensorUnit::Rpm,
        SensorUnit::Ampere,
        SensorUnit::Watt,
    };

    for (const SensorUnit unit: units) {
        const QString symbol = sensorUnitSymbol(unit);
        QVERIFY2(!symbol.isEmpty(), "Known unit must have symbol");

        // Only units rendered via LCD unit glyph must resolve to glyphs.
        if (unit == SensorUnit::Celsius || unit == SensorUnit::Volt || unit == SensorUnit::Rpm) {
            const auto glyph = LcdGlyphAtlas::GlyphId::bySymbol(QStringView(symbol));
            QVERIFY2(glyph.has_value(), "LCD-rendered unit must have glyph mapping");
        }
    }
}

void SensorContractTest::unknown_unit_has_no_symbol_and_no_glyph() {
    const QString symbol = sensorUnitSymbol(SensorUnit::Unknown);
    QVERIFY(symbol.isEmpty());
    const auto glyph = LcdGlyphAtlas::GlyphId::bySymbol(QStringView(symbol));
    QVERIFY(!glyph.has_value());
}

QTEST_APPLESS_MAIN(SensorContractTest)
#include "test_sensor_contract.moc"
