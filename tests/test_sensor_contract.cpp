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
    void valueDigits_missing_value_shows_dashes_in_number_layout();
};

namespace {
    constexpr SensorUnit kKnownUnits[] = {
        SensorUnit::Celsius, SensorUnit::Fahrenheit, SensorUnit::Volt,
        SensorUnit::Rpm, SensorUnit::Ampere, SensorUnit::Watt,
    };

    QString digits(const SensorUnit unit, const SiPrefix prefix, const std::optional<double> value) {
        return SensorFormat::valueDigits(unit, SensorFormat::scaleFor(unit, prefix), value);
    }

    QStringList symbolsOf(const SensorUnit unit) {
        const SensorFormat::QuantityFormat &format = SensorFormat::quantityFormat(unit);
        QStringList symbols;
        for (int p = static_cast<int>(format.smallestPrefix); p <= static_cast<int>(format.largestPrefix); ++p)
            symbols << SensorFormat::unitSymbol(unit, static_cast<SiPrefix>(p));
        return symbols;
    }
}

void SensorContractTest::known_units_have_symbols_and_glyphs() {
    // Every symbol the unit table allows is rendered on the segment LCD; all characters must resolve.
    for (const SensorUnit unit: kKnownUnits) {
        for (const QString &symbol: symbolsOf(unit)) {
            QVERIFY2(!symbol.isEmpty(), "Known unit must have symbol");
            QVERIFY2(LcdSegmentFont::supports(symbol), qPrintable(symbol + QStringLiteral(" must have segment glyphs")));
        }
    }
    QCOMPARE(SensorFormat::unitSymbol(SensorUnit::Ampere, SiPrefix::Milli), QStringLiteral("mA"));
    QCOMPARE(SensorFormat::unitSymbol(SensorUnit::Watt, SiPrefix::Milli), QStringLiteral("mW"));
    QCOMPARE(SensorFormat::unitSymbol(SensorUnit::Celsius), QStringLiteral("°C"));
}

void SensorContractTest::unknown_unit_has_no_symbol_and_no_glyph() {
    const QString symbol = SensorFormat::unitSymbol(SensorUnit::Unknown);
    QVERIFY(symbol.isEmpty());
    QVERIFY(LcdSegmentFont::layoutText(QString(), symbol, 30.0).isEmpty());
}

void SensorContractTest::valueDigits_formats_by_unit() {
    QCOMPARE(digits(SensorUnit::Rpm, SiPrefix::None, 1534.0), QStringLiteral(" 1534"));
    QCOMPARE(digits(SensorUnit::Celsius, SiPrefix::None, 38.25), QStringLiteral("  38.3"));
    QCOMPARE(digits(SensorUnit::Fahrenheit, SiPrefix::None, 100.75), QStringLiteral(" 100.8"));
    QCOMPARE(digits(SensorUnit::Volt, SiPrefix::None, 1.234), QStringLiteral("  1.23"));
    // Values stay in the base unit; the prefix only scales the digits.
    QCOMPARE(digits(SensorUnit::Ampere, SiPrefix::Milli, 0.301), QStringLiteral(" 301.0"));
    QCOMPARE(digits(SensorUnit::Watt, SiPrefix::Milli, 0.00361), QStringLiteral("   3.6"));
    QCOMPARE(digits(SensorUnit::Ampere, SiPrefix::Milli, -0.396), QStringLiteral("-396.0"));

    // A reading without a scale from the pipeline uses the base unit.
    const SensorReading volts{.value = 1.234, .unit = SensorUnit::Volt};
    QCOMPARE(SensorFormat::valueDigits(volts), QStringLiteral("  1.23"));
    QCOMPARE(SensorFormat::unitSymbol(volts), QStringLiteral("V"));
}

void SensorContractTest::valueWithUnit_appends_symbol() {
    const SensorReading volts{.unit = SensorUnit::Volt};
    const SensorReading rpm{.unit = SensorUnit::Rpm};
    const SensorReading unknown{.unit = SensorUnit::Unknown};
    const SensorReading milliamps{.unit = SensorUnit::Ampere, .scale = SensorFormat::scaleFor(SensorUnit::Ampere, SiPrefix::Milli)};
    QCOMPARE(SensorFormat::valueWithUnit(volts, 9.888), QStringLiteral("9.89 V"));
    QCOMPARE(SensorFormat::valueWithUnit(rpm, 1534.0), QStringLiteral("1534 RPM"));
    QCOMPARE(SensorFormat::valueWithUnit(unknown, 1.5), QStringLiteral("1.50"));
    QCOMPARE(SensorFormat::valueWithUnit(milliamps, 0.5), QStringLiteral("500.0 mA"));
}

void SensorContractTest::valueDigits_missing_value_shows_dashes_in_number_layout() {
    QCOMPARE(digits(SensorUnit::Rpm, SiPrefix::None, std::nullopt), QStringLiteral("-----"));
    QCOMPARE(digits(SensorUnit::Celsius, SiPrefix::None, std::nullopt), QStringLiteral("----.-"));
    QCOMPARE(digits(SensorUnit::Volt, SiPrefix::None, std::nullopt), QStringLiteral("---.--"));
    QCOMPARE(digits(SensorUnit::Watt, SiPrefix::Milli, std::nullopt), QStringLiteral("----.-"));

    // Same cells as a number, so the digits keep their size when the value goes or returns.
    for (const SensorUnit unit: kKnownUnits) {
        for (const SiPrefix prefix: {SiPrefix::Milli, SiPrefix::None}) {
            const QString placeholder = digits(unit, prefix, std::nullopt);
            const QString number = digits(unit, prefix, 1.0 * SensorFormat::prefixFactor(prefix));
            QVERIFY(LcdSegmentFont::supports(placeholder));
            QCOMPARE(placeholder.size(), number.size());
            const QString symbol = SensorFormat::unitSymbol(unit, prefix);
            QCOMPARE(LcdSegmentFont::layoutWidth(LcdSegmentFont::layoutText(placeholder, symbol, 28.0)),
                     LcdSegmentFont::layoutWidth(LcdSegmentFont::layoutText(number, symbol, 28.0)));
            QCOMPARE(LcdSegmentFont::fitHeight(placeholder, symbol, 28.0, 120.0),
                     LcdSegmentFont::fitHeight(number, symbol, 28.0, 120.0));
        }
    }
}

QTEST_APPLESS_MAIN(SensorContractTest)
#include "test_sensor_contract.moc"
