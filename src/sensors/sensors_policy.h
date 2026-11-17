// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors/sensor_format.h"
#include "sensors/sensor_reading.h"

#include <algorithm>
#include <cmath>
#include <optional>

/** Rules applied to raw readings: display prefix, default ranges and alert state. */
namespace SensorsPolicy {
    /** Milli-unit readings at or above this no longer fit the six-cell LCD value ("9999.9"). */
    inline constexpr double kMilliUnitOverflow = 10000.0;
    /** Base-unit readings below this lose too much precision at two decimals ("0.04"). */
    inline constexpr double kBaseUnitUnderflow = 0.1;
    /**
     * Upper bar graph limit, and alert threshold, for temperatures whose firmware
     * reports no maximum (°C).
     */
    inline constexpr double kDefaultTemperatureMaxC = 100.0;
    /**
     * Temperature limits at or below this (°C) are the 0 K placeholder, e.g. NVMe
     * secondary sensors reporting low = -273.15 °C.
     */
    inline constexpr double kTemperatureLimitAbsoluteZeroC = -273.0;
    /**
     * Temperature limits above this (°C) are placeholders, e.g. NVMe high = 65261.85 °C
     * (65535 K). Thermocouple converters such as the MAX31856 measure up to 1800 °C.
     */
    inline constexpr double kTemperatureLimitMaxPlausibleC = 2000.0;

    /** True when @p limitC (°C) is a real temperature limit rather than a placeholder. */
    [[nodiscard]] inline bool isPlausibleTemperatureLimitC(const double limitC) {
        return std::isfinite(limitC) && limitC > kTemperatureLimitAbsoluteZeroC
               && limitC <= kTemperatureLimitMaxPlausibleC;
    }

    /**
     * Drops firmware limits that are placeholders rather than limits, so that they are
     * treated as never reported. Expects raw source units (°C, V, RPM, A, W). Rules:
     * - non-finite limits;
     * - temperature limits outside (kTemperatureLimitAbsoluteZeroC, kTemperatureLimitMaxPlausibleC];
     * - max not above min: both, except a zero max with a positive min drops only the max
     *   (unset limit registers read 0, e.g. a fan with a minimum but no maximum);
     * - a zero max without a min for voltages, fans, currents and power: these read 0
     *   when the limit is unset (e.g. an unconfigured voltage input). Temperatures keep
     *   it, since 0 °C is a real limit for a freezer.
     * The measured value is not consulted, so the limits shown do not change from poll to poll.
     */
    inline void dropImplausibleFirmwareLimits(
        const SensorUnit unit,
        std::optional<double> &min,
        std::optional<double> &max
    ) {
        for (std::optional<double> *limit: {&min, &max}) {
            if (*limit && !std::isfinite(**limit))
                limit->reset();
            if (*limit && unit == SensorUnit::Celsius && !isPlausibleTemperatureLimitC(**limit))
                limit->reset();
        }

        if (min && max && *max <= *min) {
            if (*max != 0.0 || *min <= 0.0)
                min.reset();
            max.reset();
        }

        const bool zeroMeansUnset = unit == SensorUnit::Volt || unit == SensorUnit::Rpm
                                    || unit == SensorUnit::Ampere || unit == SensorUnit::Watt;
        if (zeroMeansUnset && !min && max && *max == 0.0)
            max.reset();
    }

    /**
     * Display prefix of a quantity whose unit table allows milli (current, power): milli for
     * sub-1 sensors. A nonzero firmware limit decides when present (a zero limit carries no
     * scale); otherwise @p latched carries the prefix between polls with hysteresis (milli
     * below kBaseUnitUnderflow, base from kMilliUnitOverflow) to avoid flicker. Without a
     * finite value the latched prefix is kept, or the base unit is used when nothing is
     * latched yet; the latch is left unchanged. Values and limits stay in the base unit.
     */
    [[nodiscard]] inline SiPrefix chooseMilliPrefix(
        const SensorUnit unit,
        const std::optional<double> value,
        const std::optional<double> min,
        const std::optional<double> max,
        std::optional<SiPrefix> *latched = nullptr
    ) {
        if (SensorFormat::quantityFormat(unit).smallestPrefix != SiPrefix::Milli) {
            return SiPrefix::None;
        }

        constexpr double kMilliScale = 1000.0;
        const auto isScaleReference = [](const std::optional<double> &limit) {
            return limit.has_value() && *limit != 0.0;
        };
        const bool latchedMilli = latched && latched->has_value() && **latched == SiPrefix::Milli;
        bool useMilli = false;
        if (isScaleReference(max) || isScaleReference(min)) {
            const double reference = isScaleReference(max) ? *max : *min;
            useMilli = std::isfinite(reference) && std::abs(reference) < 1.0;
        } else if (!value || !std::isfinite(*value)) {
            useMilli = latchedMilli;
        } else {
            const double magnitude = std::abs(*value);
            if (latched && latched->has_value()) {
                useMilli = latchedMilli ? magnitude * kMilliScale < kMilliUnitOverflow
                                        : magnitude < kBaseUnitUnderflow;
            } else {
                useMilli = magnitude < 1.0;
            }
            if (latched) {
                *latched = useMilli ? SiPrefix::Milli : SiPrefix::None;
            }
        }
        return useMilli ? SiPrefix::Milli : SiPrefix::None;
    }

    /**
     * Applies qsensors default range policy when firmware does not expose limits. Without
     * a finite value no bar graph is drawn, so the range is only filled to stay valid.
     */
    inline void applyDefaultRangePolicy(
        const SensorCategory category,
        const std::optional<double> value,
        std::optional<double> &min,
        std::optional<double> &max,
        const int fanDefaultMaxRpm
    ) {
        if (!value || !std::isfinite(*value)) {
            if (!min.has_value()) min = 0.0;
            if (!max.has_value()) max = 1.0;
            return;
        }
        const double measuredValue = *value;

        if (category == SensorCategory::Temperatures) {
            if (!min.has_value()) {
                min = 0.0;
            }
            if (!max.has_value()) {
                max = kDefaultTemperatureMaxC;
            }
        }

        if (category == SensorCategory::Fans) {
            if (!min.has_value()) {
                min = 0.0;
            }
            if (!max.has_value()) {
                max = static_cast<double>(fanDefaultMaxRpm);
            }
        }

        if (category == SensorCategory::Voltages ||
            category == SensorCategory::Currents ||
            category == SensorCategory::Power) {
            // Current and power are signed (battery charge/discharge); mirror the range below zero.
            const bool signedWithoutNativeMin = category != SensorCategory::Voltages
                                                && measuredValue < 0.0 && !min.has_value();
            if (!min.has_value() && !max.has_value()) {
                min = 0.0;
                max = std::max(1.0, std::abs(measuredValue) * 1.5);
            } else if (!min.has_value() && max.has_value()) {
                min = std::max(0.0, *max * 0.2);
            } else if (min.has_value() && !max.has_value()) {
                max = std::max(measuredValue, *min + std::max(1.0, std::abs(*min) * 0.5));
            }
            if (signedWithoutNativeMin) {
                min = -std::max({1.0, std::abs(measuredValue) * 1.5, std::abs(*max)});
            }
        }

        if (category == SensorCategory::Other) {
            if (!min.has_value() && !max.has_value()) {
                const double span = std::max(1.0, std::abs(measuredValue) * 0.5);
                min = measuredValue - span;
                max = measuredValue + span;
            } else if (!min.has_value() && max.has_value()) {
                min = *max - std::max(1.0, std::abs(*max) * 0.8);
            } else if (min.has_value() && !max.has_value()) {
                max = *min + std::max(1.0, std::abs(*min) * 0.8);
            }
        }

        if (min.has_value() && max.has_value() && *max <= *min) {
            *max = *min + std::max(1.0, std::abs(*min) * 0.5);
        }
    }

    /**
     * True when the reading violates its firmware limits: fans below min, temperatures
     * above max, electrical values outside [min, max]. Default ranges never alert, except
     * that temperatures without a firmware maximum alert above kDefaultTemperatureMaxC.
     * A reading without a value never alerts.
     */
    [[nodiscard]] inline bool isAlertState(const SensorReading &reading) {
        if (!reading.value) {
            return false;
        }
        const double value = *reading.value;
        const bool belowMin = reading.firmwareMin && value < *reading.firmwareMin;
        const bool aboveMax = reading.firmwareMax && value > *reading.firmwareMax;
        switch (reading.unit) {
            case SensorUnit::Rpm:
                return belowMin;
            case SensorUnit::Celsius:
                return value > reading.firmwareMax.value_or(kDefaultTemperatureMaxC);
            case SensorUnit::Fahrenheit:
                return value > reading.firmwareMax.value_or(kDefaultTemperatureMaxC * 9.0 / 5.0 + 32.0);
            case SensorUnit::Volt:
            case SensorUnit::Ampere:
            case SensorUnit::Watt:
                return belowMin || aboveMax;
            case SensorUnit::Unknown:
                break;
        }
        return false;
    }

    /** Position of the value within its limits (0..1); nullopt without limits or value. */
    [[nodiscard]] inline std::optional<double> rangeFraction(const SensorReading &reading) {
        if (!reading.hasRange() || !reading.value || !std::isfinite(*reading.value)) {
            return std::nullopt;
        }
        const double value = *reading.value;
        const double min = reading.minValue.value_or(value);
        double max = reading.maxValue.value_or(min + 1.0);
        if (!(max > min)) {
            max = min + 1.0;
        }
        return std::clamp((value - min) / (max - min), 0.0, 1.0);
    }
}
