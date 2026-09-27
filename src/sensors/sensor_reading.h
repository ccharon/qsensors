// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QString>
#include <optional>

// Normalized sensor data shared by backend, rules and UI.

/** Column a reading is grouped into; values are persisted in sensor keys. */
enum class SensorCategory {
    Voltages = 0,
    Temperatures = 1,
    Fans = 2,
    Currents = 10,
    Power = 11,
    Other = 12,
};

/** Unit of SensorReading::value after scaling and temperature conversion. */
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

/** Display symbol for @p unit, e.g. "°C" or "mA"; empty for Unknown. */
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
    QString chip; // chip name as printed by `sensors`, e.g. "coretemp-isa-0000"
    SensorCategory category = SensorCategory::Other;
    QString feature; // label from sensors.conf, or the libsensors feature name
    int featureNumber = -1; // libsensors numbering, stable while the chip exists
    int subfeatureNumber = -1;
    double value = 0.0;
    SensorUnit unit = SensorUnit::Unknown;
    std::optional<double> minValue; // native limit or the default range policy
    std::optional<double> maxValue;

    /** True when at least one limit is known. */
    [[nodiscard]] bool hasRange() const { return minValue.has_value() || maxValue.has_value(); }
};
