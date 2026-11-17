// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/sensor_format.h"

#include <cmath>

const SensorFormat::QuantityFormat &SensorFormat::quantityFormat(const SensorUnit unit) {
    // One row per quantity; the LCD and the tooltips draw exactly what this table allows.
    static const QuantityFormat kCelsius{QStringLiteral("°C"), SiPrefix::None, SiPrefix::None, 1, 1, 6};
    static const QuantityFormat kFahrenheit{QStringLiteral("°F"), SiPrefix::None, SiPrefix::None, 1, 1, 6};
    static const QuantityFormat kVolt{QStringLiteral("V"), SiPrefix::None, SiPrefix::None, 2, 2, 6};
    static const QuantityFormat kRpm{QStringLiteral("RPM"), SiPrefix::None, SiPrefix::None, 0, 0, 5};
    // One decimal keeps negative milli values (e.g. -396.0 mA) within six cells.
    static const QuantityFormat kAmpere{QStringLiteral("A"), SiPrefix::Milli, SiPrefix::None, 2, 1, 6};
    static const QuantityFormat kWatt{QStringLiteral("W"), SiPrefix::Milli, SiPrefix::None, 2, 1, 6};
    static const QuantityFormat kUnknown{QString(), SiPrefix::None, SiPrefix::None, 2, 2, 6};
    switch (unit) {
        case SensorUnit::Celsius: return kCelsius;
        case SensorUnit::Fahrenheit: return kFahrenheit;
        case SensorUnit::Volt: return kVolt;
        case SensorUnit::Rpm: return kRpm;
        case SensorUnit::Ampere: return kAmpere;
        case SensorUnit::Watt: return kWatt;
        case SensorUnit::Unknown: break;
    }
    return kUnknown;
}

double SensorFormat::prefixFactor(const SiPrefix prefix) {
    return std::pow(10.0, 3 * static_cast<int>(prefix));
}

QString SensorFormat::unitSymbol(const SensorUnit unit, const SiPrefix prefix) {
    const QString &symbol = quantityFormat(unit).symbol;
    if (symbol.isEmpty())
        return symbol;
    switch (prefix) {
        case SiPrefix::Milli: return QLatin1Char('m') + symbol;
        case SiPrefix::None: break;
        case SiPrefix::Kilo: return QLatin1Char('k') + symbol;
    }
    return symbol;
}

SensorScale SensorFormat::scaleFor(const SensorUnit unit, const SiPrefix prefix) {
    const QuantityFormat &format = quantityFormat(unit);
    return {prefix, prefix == SiPrefix::None ? format.decimals : format.prefixedDecimals};
}

SensorScale SensorFormat::scaleOf(const SensorReading &reading) {
    return reading.scale.value_or(scaleFor(reading.unit, SiPrefix::None));
}

QString SensorFormat::valueDigits(const SensorUnit unit, const SensorScale &scale, const std::optional<double> value) {
    const int width = quantityFormat(unit).width;
    const int decimals = scale.decimals;
    if (!value) {
        const QString integer(width - decimals - (decimals > 0 ? 1 : 0), QLatin1Char('-'));
        return decimals > 0 ? integer + QLatin1Char('.') + QString(decimals, QLatin1Char('-')) : integer;
    }
    return QStringLiteral("%1").arg(*value / prefixFactor(scale.prefix), width, 'f', decimals, QLatin1Char(' '));
}

QString SensorFormat::valueDigits(const SensorReading &reading) {
    return valueDigits(reading.unit, scaleOf(reading), reading.value);
}

QString SensorFormat::unitSymbol(const SensorReading &reading) {
    return unitSymbol(reading.unit, scaleOf(reading).prefix);
}

QString SensorFormat::valueWithUnit(const SensorReading &reading, const double value) {
    const QString digits = valueDigits(reading.unit, scaleOf(reading), value).trimmed();
    const QString symbol = unitSymbol(reading);
    return symbol.isEmpty() ? digits : digits + QLatin1Char(' ') + symbol;
}
