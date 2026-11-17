// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QString>
#include <optional>

// Normalized sensor data shared by backend, rules and UI.

/** Column a reading is grouped into. */
enum class SensorCategory {
    Voltages = 0,
    Temperatures = 1,
    Fans = 2,
    Currents = 10,
    Power = 11,
    Other = 12,
};

/**
 * Quantity of SensorReading::value, named by its base unit. Values are always in the base
 * unit (no prefix); temperatures are in °C or °F after the temperature unit setting.
 * How a value is shown (SI prefix, decimals) is a separate SensorScale.
 */
enum class SensorUnit {
    Celsius,
    Fahrenheit,
    Volt,
    Rpm,
    Ampere,
    Watt,
    Percent, // PWM duty cycle
    RelativeHumidity, // %RH
    Joule,
    Hertz,
    Unknown // stays last: the unit table iterates up to it
};

/** SI prefix of a displayed value; the power of ten is three times the enum value. */
enum class SiPrefix {
    Milli = -1,
    None = 0,
    Kilo = 1,
    Mega = 2,
    Giga = 3,
};

/** Which firmware limit a reading violates, if any; see SensorsPolicy::alertState(). */
enum class AlertState {
    None,
    Low, // below the minimum
    High, // above the maximum
};

/** How a value is displayed: SI prefix and number of decimals. */
struct SensorScale {
    SiPrefix prefix = SiPrefix::None;
    int decimals = 0;

    bool operator==(const SensorScale &) const = default;
};

/** Normalized sensor sample used by the UI layer. */
struct SensorReading {
    QString chip; // chip name as printed by `sensors`, e.g. "coretemp-isa-0000"
    SensorCategory category = SensorCategory::Other;
    QString feature; // label from sensors.conf, or the libsensors feature name
    int featureNumber = -1; // libsensors numbering, stable while the chip exists
    int subfeatureNumber = -1;
    // Measured value; nullopt while the sensor is listed but cannot be read (or reads non-finite).
    std::optional<double> value = std::nullopt;
    SensorUnit unit = SensorUnit::Unknown;
    // Bar graph range: the firmware limit, or the default range policy where the firmware has none.
    std::optional<double> minValue;
    std::optional<double> maxValue;
    // Limits reported by the firmware; only these raise an alert (plus the temperature fallback).
    std::optional<double> firmwareMin;
    std::optional<double> firmwareMax;
    // Display scale chosen by the pipeline; nullopt lets the formatter pick one for the value.
    std::optional<SensorScale> scale;

    /** True when at least one bar graph limit is known. */
    [[nodiscard]] bool hasRange() const { return minValue.has_value() || maxValue.has_value(); }

    /** True when the sensor delivered a value in this poll. */
    [[nodiscard]] bool hasValue() const { return value.has_value(); }

    /** True when the firmware reports at least one limit. */
    [[nodiscard]] bool hasFirmwareLimits() const { return firmwareMin.has_value() || firmwareMax.has_value(); }
};
