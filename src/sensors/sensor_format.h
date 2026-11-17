// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors/sensor_reading.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

/**
 * Unit table and text formatting of sensor values, shared by the rules, the LCD and the
 * tooltips. A value is shown with four significant digits in a fixed number of digit
 * cells; its SI prefix and decimals come from a SensorScale, one of the scale ranges the
 * unit table defines for the quantity.
 */
namespace SensorFormat {
    /** Digit cells of every displayed value, a minus sign included; the decimal point needs no cell. */
    inline constexpr int kValueCells = 5;
    /** Significant digits a scale range is laid out for, e.g. "1.052" or "12.10". */
    inline constexpr int kSignificantDigits = 4;

    /** Per quantity: symbol of the base unit, the SI prefixes the display may use, and the most decimals. */
    struct QuantityFormat {
        QString symbol; // base unit, e.g. "V"; empty for SensorUnit::Unknown
        SiPrefix smallestPrefix = SiPrefix::None;
        SiPrefix largestPrefix = SiPrefix::None;
        int maxDecimals = kSignificantDigits - 1; // cap below four significant digits, e.g. 1 for °C
    };

    /** Row of the unit table for @p unit. */
    [[nodiscard]] const QuantityFormat &quantityFormat(SensorUnit unit);

    /** Power of ten of @p prefix, e.g. 1e-3 for milli. */
    [[nodiscard]] double prefixFactor(SiPrefix prefix);

    /** Display symbol for @p unit with @p prefix, e.g. "°C" or "mA"; empty for Unknown. */
    [[nodiscard]] QString unitSymbol(SensorUnit unit, SiPrefix prefix = SiPrefix::None);

    /** Every symbol the unit table can show, each once; e.g. to size a unit field. */
    [[nodiscard]] QStringList allUnitSymbols();

    /** One display scale of a quantity and the values it is made for. */
    struct ScaleRange {
        SensorScale scale;
        double upperBound = 0.0; // base unit; magnitudes below it belong to this range
    };

    /**
     * Scale ranges of @p unit, smallest first, one per decade with kSignificantDigits:
     * "880 mV" (milli shows no decimals), "1.052 V", "12.10 V", "230.1 V", "1.234 kW".
     * The decimals never exceed QuantityFormat::maxDecimals ("38.3 °C", "1534 RPM"); the
     * largest prefix continues with whole numbers until the cells are full.
     */
    [[nodiscard]] const QVector<ScaleRange> &scaleRanges(SensorUnit unit);

    /** Index of the range without prefix whose decimals are highest; used when nothing else decides. */
    [[nodiscard]] int baseRangeIndex(SensorUnit unit);

    /**
     * Index of the range @p magnitude (base unit, >= 0) belongs to: the first one in which
     * the value, rounded to that range's decimals, stays below the range's upper bound.
     * So 0.9996 V is "1.000 V", not "1000 mV".
     */
    [[nodiscard]] int naturalRangeIndex(SensorUnit unit, double magnitude);

    /** Scale of @p reading: the one chosen by the pipeline, otherwise the natural one for its value. */
    [[nodiscard]] SensorScale scaleOf(const SensorReading &reading);

    /** What the LCD shows for a value: the digits and the unit symbol. */
    struct DisplayValue {
        QString digits; // right-aligned in kValueCells digit cells, e.g. "  1.052" or "  880"
        QString symbol; // e.g. "mV"

        bool operator==(const DisplayValue &) const = default;
    };

    /**
     * @p value (base unit) in @p scale. If the result does not fit kValueCells cells
     * (a minus sign, or rounding up to the next decade), fewer decimals are used, then the
     * next larger prefix; "OL" when nothing fits. A value that rounds to zero has no minus
     * sign. Without a value, every digit cell shows a dash in the layout of @p scale,
     * e.g. "----.-", and the symbol is kept.
     */
    [[nodiscard]] DisplayValue display(SensorUnit unit, const SensorScale &scale, std::optional<double> value);

    /** The reading's value as drawn on the LCD. */
    [[nodiscard]] DisplayValue display(const SensorReading &reading);

    /** @p value (base unit) as the reading's LCD would show it, trimmed, e.g. "9.888 V". */
    [[nodiscard]] QString valueWithUnit(const SensorReading &reading, double value);
}
