// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/sensor_reading.h"
#include "ui/widgets/lcd_segment_font.h"
#include "sensors/sensor_format.h"

#include <QtTest/QtTest>

// Verifies the unit table, the scale ranges, value formatting and that every unit is drawable by the LCD.
class SensorContractTest final : public QObject {
    Q_OBJECT

private slots:
    void known_units_have_symbols_and_glyphs();
    void unknown_unit_has_no_symbol_and_no_glyph();
    void scale_ranges_give_four_significant_digits();
    void natural_range_follows_the_rounded_value();
    void display_formats_each_range_of_the_table();
    void display_fits_negative_and_rounded_values_into_the_cells();
    void display_never_shows_negative_zero();
    void display_overflow_shows_OL();
    void display_missing_value_shows_dashes_in_number_layout();
    void reading_without_scale_uses_the_natural_one();
    void valueWithUnit_matches_the_lcd();
};

namespace {
    constexpr SensorUnit kKnownUnits[] = {
        SensorUnit::Celsius, SensorUnit::Fahrenheit, SensorUnit::Volt, SensorUnit::Rpm, SensorUnit::Ampere,
        SensorUnit::Watt, SensorUnit::Percent, SensorUnit::RelativeHumidity, SensorUnit::Joule, SensorUnit::Hertz,
    };
    constexpr SensorUnit kAllUnits[] = {
        SensorUnit::Celsius, SensorUnit::Fahrenheit, SensorUnit::Volt, SensorUnit::Rpm, SensorUnit::Ampere,
        SensorUnit::Watt, SensorUnit::Percent, SensorUnit::RelativeHumidity, SensorUnit::Joule, SensorUnit::Hertz,
        SensorUnit::Unknown,
    };

    QStringList symbolsOf(const SensorUnit unit) {
        QStringList symbols;
        for (const SensorFormat::ScaleRange &range: SensorFormat::scaleRanges(unit)) {
            const QString symbol = SensorFormat::unitSymbol(unit, range.scale.prefix);
            if (!symbols.contains(symbol))
                symbols << symbol;
        }
        return symbols;
    }

    // Digits and symbol of the natural scale for @p value, as a fresh sensor shows it.
    QString shown(const SensorUnit unit, const double value) {
        const SensorScale scale =
                SensorFormat::scaleRanges(unit).at(SensorFormat::naturalRangeIndex(unit, std::abs(value))).scale;
        const SensorFormat::DisplayValue display = SensorFormat::display(unit, scale, value);
        return display.digits + QLatin1Char('|') + display.symbol;
    }

    int cells(const QString &digits) {
        return static_cast<int>(digits.size() - digits.count(QLatin1Char('.')));
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
    QCOMPARE(symbolsOf(SensorUnit::Volt), QStringList({QStringLiteral("mV"), QStringLiteral("V")}));
    QCOMPARE(symbolsOf(SensorUnit::Ampere),
             QStringList({QStringLiteral("mA"), QStringLiteral("A"), QStringLiteral("kA")}));
    QCOMPARE(symbolsOf(SensorUnit::Watt),
             QStringList({QStringLiteral("mW"), QStringLiteral("W"), QStringLiteral("kW")}));
    QCOMPARE(symbolsOf(SensorUnit::Celsius), QStringList{QStringLiteral("°C")});
    QCOMPARE(symbolsOf(SensorUnit::Fahrenheit), QStringList{QStringLiteral("°F")});
    QCOMPARE(symbolsOf(SensorUnit::Rpm), QStringList{QStringLiteral("RPM")});
    QCOMPARE(symbolsOf(SensorUnit::Percent), QStringList{QStringLiteral("%")});
    QCOMPARE(symbolsOf(SensorUnit::RelativeHumidity), QStringList{QStringLiteral("%RH")});
    QCOMPARE(symbolsOf(SensorUnit::Joule), QStringList({QStringLiteral("J"), QStringLiteral("kJ"), QStringLiteral("MJ"),
                                                        QStringLiteral("GJ")}));
    QCOMPARE(symbolsOf(SensorUnit::Hertz),
             QStringList({QStringLiteral("Hz"), QStringLiteral("kHz"), QStringLiteral("MHz")}));
}

void SensorContractTest::unknown_unit_has_no_symbol_and_no_glyph() {
    const QString symbol = SensorFormat::unitSymbol(SensorUnit::Unknown);
    QVERIFY(symbol.isEmpty());
    QVERIFY(LcdSegmentFont::layoutText(QString(), symbol, 30.0).isEmpty());
}

void SensorContractTest::scale_ranges_give_four_significant_digits() {
    const auto scales = [](const SensorUnit unit) {
        QList<QPair<int, int>> list;
        for (const SensorFormat::ScaleRange &range: SensorFormat::scaleRanges(unit))
            list.append({static_cast<int>(range.scale.prefix), range.scale.decimals});
        return list;
    };
    // mV without decimals, then 1.052 / 12.10 / 230.1 V, then whole volts.
    QCOMPARE(scales(SensorUnit::Volt), (QList<QPair<int, int>>{{-1, 0}, {0, 3}, {0, 2}, {0, 1}, {0, 0}, {0, 0}}));
    // mA, A as for volts, then kA from 1000 A.
    QCOMPARE(scales(SensorUnit::Ampere),
             (QList<QPair<int, int>>{{-1, 0}, {0, 3}, {0, 2}, {0, 1}, {1, 3}, {1, 2}, {1, 1}, {1, 0}, {1, 0}}));
    // Temperatures keep one decimal until four digits need the cells.
    QCOMPARE(scales(SensorUnit::Celsius), (QList<QPair<int, int>>{{0, 1}, {0, 1}, {0, 1}, {0, 0}, {0, 0}}));
    for (const auto &[prefix, decimals]: scales(SensorUnit::Rpm))
        QCOMPARE(decimals, 0);

    // Upper bounds grow by decades and the last range is open.
    const QVector<SensorFormat::ScaleRange> &volt = SensorFormat::scaleRanges(SensorUnit::Volt);
    QCOMPARE(volt.at(0).upperBound, 1.0);
    QCOMPARE(volt.at(1).upperBound, 10.0);
    QCOMPARE(volt.at(3).upperBound, 1000.0);
    QVERIFY(std::isinf(volt.last().upperBound));
    QCOMPARE(SensorFormat::scaleRanges(SensorUnit::Watt).at(4).upperBound, 10000.0);

    QCOMPARE(SensorFormat::baseRangeIndex(SensorUnit::Volt), 1);
    QCOMPARE(SensorFormat::baseRangeIndex(SensorUnit::Celsius), 0);
}

void SensorContractTest::natural_range_follows_the_rounded_value() {
    QCOMPARE(SensorFormat::naturalRangeIndex(SensorUnit::Volt, 0.0), 0);
    QCOMPARE(SensorFormat::naturalRangeIndex(SensorUnit::Volt, 0.9994), 0);
    // 0.9996 V would be "1000 mV": it belongs to the volt range.
    QCOMPARE(SensorFormat::naturalRangeIndex(SensorUnit::Volt, 0.9996), 1);
    QCOMPARE(SensorFormat::naturalRangeIndex(SensorUnit::Volt, 9.9994), 1);
    QCOMPARE(SensorFormat::naturalRangeIndex(SensorUnit::Volt, 9.9996), 2);
    QCOMPARE(SensorFormat::naturalRangeIndex(SensorUnit::Volt, 99.996), 3);
    QCOMPARE(SensorFormat::naturalRangeIndex(SensorUnit::Watt, 999.94), 3);
    QCOMPARE(SensorFormat::naturalRangeIndex(SensorUnit::Watt, 999.96), 4);
    QCOMPARE(SensorFormat::naturalRangeIndex(SensorUnit::Volt, 1e9), 5);
}

void SensorContractTest::display_formats_each_range_of_the_table() {
    QCOMPARE(shown(SensorUnit::Volt, 0.88), QStringLiteral("  880|mV"));
    QCOMPARE(shown(SensorUnit::Volt, 1.052), QStringLiteral(" 1.052|V"));
    QCOMPARE(shown(SensorUnit::Volt, 12.1), QStringLiteral(" 12.10|V"));
    QCOMPARE(shown(SensorUnit::Volt, 230.1), QStringLiteral(" 230.1|V"));
    QCOMPARE(shown(SensorUnit::Ampere, 0.396), QStringLiteral("  396|mA"));
    QCOMPARE(shown(SensorUnit::Watt, 0.00361), QStringLiteral("    4|mW"));
    QCOMPARE(shown(SensorUnit::Watt, 1234.0), QStringLiteral(" 1.234|kW"));
    QCOMPARE(shown(SensorUnit::Ampere, 1234.0), QStringLiteral(" 1.234|kA"));
    QCOMPARE(shown(SensorUnit::Watt, 45678.0), QStringLiteral(" 45.68|kW"));
    QCOMPARE(shown(SensorUnit::Celsius, 38.26), QStringLiteral("  38.3|°C"));
    QCOMPARE(shown(SensorUnit::Celsius, 5.04), QStringLiteral("   5.0|°C"));
    QCOMPARE(shown(SensorUnit::Fahrenheit, 100.76), QStringLiteral(" 100.8|°F"));
    // A thermocouple above 1000 °C drops the decimal instead of overflowing.
    QCOMPARE(shown(SensorUnit::Celsius, 1234.6), QStringLiteral(" 1235|°C"));
    QCOMPARE(shown(SensorUnit::Rpm, 1534.0), QStringLiteral(" 1534|RPM"));
    QCOMPARE(shown(SensorUnit::Rpm, 12000.0), QStringLiteral("12000|RPM"));
    QCOMPARE(shown(SensorUnit::Unknown, 1.5), QStringLiteral(" 1.500|"));
    QCOMPARE(shown(SensorUnit::Percent, 42.4), QStringLiteral("   42|%"));
    QCOMPARE(shown(SensorUnit::Percent, 100.0), QStringLiteral("  100|%"));
    QCOMPARE(shown(SensorUnit::RelativeHumidity, 55.6), QStringLiteral("   56|%RH"));
    QCOMPARE(shown(SensorUnit::Joule, 12.5), QStringLiteral(" 12.50|J"));
    QCOMPARE(shown(SensorUnit::Joule, 4321.0), QStringLiteral(" 4.321|kJ"));
    QCOMPARE(shown(SensorUnit::Joule, 123456789.0), QStringLiteral(" 123.5|MJ"));
    QCOMPARE(shown(SensorUnit::Hertz, 50.0), QStringLiteral(" 50.00|Hz"));
    QCOMPARE(shown(SensorUnit::Hertz, 25000.0), QStringLiteral(" 25.00|kHz"));
    QCOMPARE(shown(SensorUnit::Hertz, 1.85e9), QStringLiteral(" 1850|MHz"));

    // Every digit string has exactly kValueCells cells.
    for (const SensorUnit unit: kAllUnits) {
        for (const double value: {0.0, 0.001, 0.5, 1.0, 9.99, 42.0, 999.9, 4321.0, -0.396, -12.5}) {
            const QString line = shown(unit, value);
            QCOMPARE(cells(line.section(QLatin1Char('|'), 0, 0)), SensorFormat::kValueCells);
        }
    }
}

void SensorContractTest::display_fits_negative_and_rounded_values_into_the_cells() {
    const SensorScale milli{SiPrefix::Milli, 0};
    const SensorScale volts3{SiPrefix::None, 3};
    // A latched mA scale: a battery current of -2 A fits with its sign.
    QCOMPARE(SensorFormat::display(SensorUnit::Ampere, milli, -2.0).digits, QStringLiteral("-2000"));
    QCOMPARE(SensorFormat::display(SensorUnit::Ampere, milli, -2.0).symbol, QStringLiteral("mA"));
    // -10 A would need six cells in mA: the next prefix takes over.
    const SensorFormat::DisplayValue tenAmps = SensorFormat::display(SensorUnit::Ampere, milli, -10.0);
    QCOMPARE(tenAmps.digits, QStringLiteral("-10.00"));
    QCOMPARE(tenAmps.symbol, QStringLiteral("A"));
    // 99.99996 A rounds to 100000 mA, and to 100.000 A.
    QCOMPARE(SensorFormat::display(SensorUnit::Ampere, milli, 99.99996).digits, QStringLiteral("100.00"));
    // A latched "1.052 V" scale keeps volts but gives up decimals for a sign.
    QCOMPARE(SensorFormat::display(SensorUnit::Volt, volts3, -12.345).digits, QStringLiteral("-12.35"));
    QCOMPARE(SensorFormat::display(SensorUnit::Volt, volts3, 12.345).digits, QStringLiteral("12.345"));
    QCOMPARE(SensorFormat::display(SensorUnit::Volt, volts3, -9.9996).digits, QStringLiteral("-10.00"));
    QCOMPARE(SensorFormat::display(SensorUnit::Celsius, {SiPrefix::None, 1}, -100.04).digits, QStringLiteral("-100.0"));
    QCOMPARE(SensorFormat::display(SensorUnit::Celsius, {SiPrefix::None, 1}, -1000.4).digits, QStringLiteral("-1000"));
}

void SensorContractTest::display_never_shows_negative_zero() {
    QCOMPARE(SensorFormat::display(SensorUnit::Celsius, {SiPrefix::None, 1}, -0.04).digits, QStringLiteral("   0.0"));
    QCOMPARE(SensorFormat::display(SensorUnit::Ampere, {SiPrefix::Milli, 0}, -0.0004).digits, QStringLiteral("    0"));
    QCOMPARE(SensorFormat::display(SensorUnit::Volt, {SiPrefix::None, 3}, -0.0).digits, QStringLiteral(" 0.000"));
    QCOMPARE(SensorFormat::display(SensorUnit::Volt, {SiPrefix::None, 3}, -0.0006).digits, QStringLiteral("-0.001"));
}

void SensorContractTest::display_overflow_shows_OL() {
    const SensorFormat::DisplayValue overflow = SensorFormat::display(SensorUnit::Rpm, {SiPrefix::None, 0}, 123456.0);
    QCOMPARE(overflow.digits, QStringLiteral("   OL"));
    QCOMPARE(overflow.symbol, QStringLiteral("RPM"));
    QVERIFY(LcdSegmentFont::supports(overflow.digits));
}

void SensorContractTest::display_missing_value_shows_dashes_in_number_layout() {
    QCOMPARE(SensorFormat::display(SensorUnit::Rpm, {SiPrefix::None, 0}, std::nullopt).digits, QStringLiteral("-----"));
    QCOMPARE(SensorFormat::display(SensorUnit::Celsius, {SiPrefix::None, 1}, std::nullopt).digits,
             QStringLiteral("----.-"));
    QCOMPARE(SensorFormat::display(SensorUnit::Volt, {SiPrefix::None, 3}, std::nullopt).digits,
             QStringLiteral("--.---"));
    const SensorFormat::DisplayValue milliwatts = SensorFormat::display(SensorUnit::Watt, {SiPrefix::Milli, 0}, std::nullopt);
    QCOMPARE(milliwatts.digits, QStringLiteral("-----"));
    // The unit stays.
    QCOMPARE(milliwatts.symbol, QStringLiteral("mW"));

    // Same cells as a number in every scale, so the digits keep their place when the value goes or returns.
    for (const SensorUnit unit: kAllUnits) {
        for (const SensorFormat::ScaleRange &range: SensorFormat::scaleRanges(unit)) {
            const SensorFormat::DisplayValue placeholder = SensorFormat::display(unit, range.scale, std::nullopt);
            const SensorFormat::DisplayValue number =
                    SensorFormat::display(unit, range.scale, 1.0 * SensorFormat::prefixFactor(range.scale.prefix));
            QVERIFY(LcdSegmentFont::supports(placeholder.digits));
            QCOMPARE(placeholder.digits.size(), number.digits.size());
            QCOMPARE(placeholder.symbol, number.symbol);
        }
    }
}

void SensorContractTest::reading_without_scale_uses_the_natural_one() {
    const SensorReading volts{.value = 1.234, .unit = SensorUnit::Volt};
    QCOMPARE(SensorFormat::display(volts), (SensorFormat::DisplayValue{QStringLiteral(" 1.234"), QStringLiteral("V")}));
    const SensorReading small{.value = 0.25, .unit = SensorUnit::Ampere};
    QCOMPARE(SensorFormat::display(small), (SensorFormat::DisplayValue{QStringLiteral("  250"), QStringLiteral("mA")}));
    // Without a value: the base scale.
    const SensorReading missing{.unit = SensorUnit::Watt};
    QCOMPARE(SensorFormat::display(missing), (SensorFormat::DisplayValue{QStringLiteral("--.---"), QStringLiteral("W")}));
    // A scale from the pipeline wins over the value.
    SensorReading latched{.value = 1.5, .unit = SensorUnit::Watt, .scale = SensorScale{SiPrefix::Milli, 0}};
    QCOMPARE(SensorFormat::display(latched), (SensorFormat::DisplayValue{QStringLiteral(" 1500"), QStringLiteral("mW")}));
}

void SensorContractTest::valueWithUnit_matches_the_lcd() {
    const SensorReading volts{.value = 12.0, .unit = SensorUnit::Volt};
    const SensorReading rpm{.unit = SensorUnit::Rpm};
    const SensorReading unknown{.unit = SensorUnit::Unknown};
    const SensorReading milliamps{.unit = SensorUnit::Ampere, .scale = SensorScale{SiPrefix::Milli, 0}};
    // Limits use the reading's scale, so they read like the card.
    QCOMPARE(SensorFormat::valueWithUnit(volts, 9.888), QStringLiteral("9.89 V"));
    QCOMPARE(SensorFormat::valueWithUnit(rpm, 1534.0), QStringLiteral("1534 RPM"));
    QCOMPARE(SensorFormat::valueWithUnit(unknown, 1.5), QStringLiteral("1.500"));
    QCOMPARE(SensorFormat::valueWithUnit(milliamps, 0.5), QStringLiteral("500 mA"));
    QCOMPARE(SensorFormat::valueWithUnit(milliamps, -20.0), QStringLiteral("-20.00 A"));
}

QTEST_APPLESS_MAIN(SensorContractTest)
#include "test_sensor_contract.moc"
