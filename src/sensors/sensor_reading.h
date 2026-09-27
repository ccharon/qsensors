// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QString>
#include <optional>

enum class SensorCategory {
    Voltages = 0,
    Temperatures = 1,
    Fans = 2,
    Currents = 10,
    Power = 11,
    Other = 12,
};

enum class SensorUnit {
    Celsius,
    Fahrenheit,
    Volt,
    Rpm,
    Ampere,
    Milliampere,
    Watt,
    Milliwatt,
    Unknown
};

[[nodiscard]] inline QString sensorUnitSymbol(const SensorUnit unit) {
    switch (unit) {
        case SensorUnit::Celsius: return QStringLiteral("°C");
        case SensorUnit::Fahrenheit: return QStringLiteral("°F");
        case SensorUnit::Volt: return QStringLiteral("V");
        case SensorUnit::Rpm: return QStringLiteral("RPM");
        case SensorUnit::Ampere: return QStringLiteral("A");
        case SensorUnit::Milliampere: return QStringLiteral("mA");
        case SensorUnit::Watt: return QStringLiteral("W");
        case SensorUnit::Milliwatt: return QStringLiteral("mW");
        case SensorUnit::Unknown: return QString();
    }
    return QString();
}

/** Normalized sensor sample used by the UI layer. */
struct SensorReading {
    QString chip;
    SensorCategory category = SensorCategory::Other;
    QString feature;
    int featureNumber = -1;
    int subfeatureNumber = -1;
    double value = 0.0;
    SensorUnit unit = SensorUnit::Unknown;
    std::optional<double> minValue;
    std::optional<double> maxValue;

    [[nodiscard]] bool hasRange() const { return minValue.has_value() || maxValue.has_value(); }
};
