// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensor_format.h"

QString SensorFormat::valueDigits(const SensorUnit unit, const double value) {
    const auto pad = [value](const int width, const int decimals) {
        return QStringLiteral("%1").arg(value, width, 'f', decimals, QLatin1Char(' '));
    };
    switch (unit) {
        case SensorUnit::Rpm:
            return pad(5, 0);
        case SensorUnit::Celsius:
        case SensorUnit::Fahrenheit:
            return pad(6, 1);
        case SensorUnit::Milliampere:
        case SensorUnit::Milliwatt:
            // One decimal keeps negative milli values (e.g. -396.0 mA) within six cells.
            return pad(6, 1);
        case SensorUnit::Volt:
        case SensorUnit::Ampere:
        case SensorUnit::Watt:
        case SensorUnit::Unknown:
            break;
    }
    return pad(6, 2);
}

QString SensorFormat::valueWithUnit(const SensorUnit unit, const double value) {
    const QString digits = valueDigits(unit, value).trimmed();
    const QString symbol = sensorUnitSymbol(unit);
    return symbol.isEmpty() ? digits : digits + QLatin1Char(' ') + symbol;
}
