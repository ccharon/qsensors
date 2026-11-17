// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors/sensor_reading.h"

#include <QString>
#include <optional>

/** Unit table and text formatting of sensor values, shared by the rules, the LCD and tooltips. */
namespace SensorFormat {
    /** Per quantity: symbol of the base unit, the SI prefixes the display may use and the decimals. */
    struct QuantityFormat {
        QString symbol; // base unit, e.g. "V"; empty for SensorUnit::Unknown
        SiPrefix smallestPrefix = SiPrefix::None;
        SiPrefix largestPrefix = SiPrefix::None;
        int decimals = 0; // without prefix
        int prefixedDecimals = 0; // with a milli prefix
        int width = 6; // characters of the right-aligned value, decimal point included
    };

    /** Row of the unit table for @p unit. */
    [[nodiscard]] const QuantityFormat &quantityFormat(SensorUnit unit);

    /** Power of ten of @p prefix, e.g. 1e-3 for milli. */
    [[nodiscard]] double prefixFactor(SiPrefix prefix);

    /** Display symbol for @p unit with @p prefix, e.g. "°C" or "mA"; empty for Unknown. */
    [[nodiscard]] QString unitSymbol(SensorUnit unit, SiPrefix prefix = SiPrefix::None);

    /** Scale of @p unit with @p prefix and the decimals the table gives it. */
    [[nodiscard]] SensorScale scaleFor(SensorUnit unit, SiPrefix prefix);

    /** Scale of @p reading: the one chosen by the pipeline, otherwise the table's base scale. */
    [[nodiscard]] SensorScale scaleOf(const SensorReading &reading);

    /**
     * Right-aligned digits of @p value (base unit) in @p scale, e.g. "  38.3" or " 1534".
     * Without a value, every digit cell shows a dash, e.g. "----.-".
     */
    [[nodiscard]] QString valueDigits(SensorUnit unit, const SensorScale &scale, std::optional<double> value);

    /** Digits of the reading's value as drawn on the LCD. */
    [[nodiscard]] QString valueDigits(const SensorReading &reading);

    /** Unit symbol of the reading as drawn on the LCD, e.g. "mA". */
    [[nodiscard]] QString unitSymbol(const SensorReading &reading);

    /** @p value (base unit) in the reading's scale with unit symbol, trimmed, e.g. "9.89 V". */
    [[nodiscard]] QString valueWithUnit(const SensorReading &reading, double value);
}
