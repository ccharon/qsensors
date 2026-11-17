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
    /**
     * Factor by which a value may leave its latched scale range before the scale changes:
     * a latched "mA" holds up to 9999 mA, a latched "1.052 V" down to 0.100 V.
     */
    inline constexpr double kScaleHysteresis = 10.0;
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
     * - a zero max without a min for every known quantity but temperature: these read 0
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

        const bool zeroMeansUnset = unit != SensorUnit::Celsius && unit != SensorUnit::Fahrenheit
                                    && unit != SensorUnit::Unknown;
        if (zeroMeansUnset && !min && max && *max == 0.0)
            max.reset();
    }

    /** Scale range a sensor keeps between polls, see chooseScale(). */
    struct ScaleLatch {
        int rangeIndex = 0; // into SensorFormat::scaleRanges()
        bool fromLimit = false; // taken from a firmware limit

        bool operator==(const ScaleLatch &) const = default;
    };

    /**
     * Display scale (SI prefix and decimals) of a reading: one of SensorFormat::scaleRanges().
     * The sensor's range decides, not the momentary value alone, so the display does not
     * flicker between scales:
     * - a nonzero firmware limit (the maximum, else the minimum; a zero limit carries no
     *   scale) gives the range it belongs to, and @p latch takes it over: while the limit is
     *   missing (a failed limit read) that range is kept;
     * - otherwise the latched range moves only when the value leaves it by a factor of
     *   kScaleHysteresis (up from ten times its upper bound, down below a tenth of its lower
     *   bound), to the range the value belongs to;
     * - the first value without a latch picks its own range.
     * Without a finite value the latched range is kept, or the base range is used when nothing
     * is latched yet; the latch is left unchanged. Expects values and limits in display units;
     * the formatter still falls back when a value does not fit the chosen scale.
     */
    [[nodiscard]] inline SensorScale chooseScale(
        const SensorUnit unit,
        const std::optional<double> value,
        const std::optional<double> min,
        const std::optional<double> max,
        std::optional<ScaleLatch> *latch = nullptr
    ) {
        const QVector<SensorFormat::ScaleRange> &ranges = SensorFormat::scaleRanges(unit);
        const int last = static_cast<int>(ranges.size()) - 1;
        const auto isScaleReference = [](const std::optional<double> &limit) {
            return limit.has_value() && std::isfinite(*limit) && *limit != 0.0;
        };
        std::optional<ScaleLatch> latched;
        if (latch && latch->has_value() && (*latch)->rangeIndex >= 0 && (*latch)->rangeIndex <= last)
            latched = *latch;

        ScaleLatch next;
        if (isScaleReference(max) || isScaleReference(min)) {
            const double reference = isScaleReference(max) ? *max : *min;
            next = {SensorFormat::naturalRangeIndex(unit, std::abs(reference)), true};
        } else if (!value || !std::isfinite(*value)) {
            return ranges.at(latched ? latched->rangeIndex : SensorFormat::baseRangeIndex(unit)).scale;
        } else if (latched && latched->fromLimit) {
            next = *latched;
        } else {
            const double magnitude = std::abs(*value);
            const int index = latched ? latched->rangeIndex : -1;
            const bool aboveRange = index >= 0 && index < last
                                    && magnitude >= ranges.at(index).upperBound * kScaleHysteresis;
            const bool belowRange = index > 0 && magnitude < ranges.at(index - 1).upperBound / kScaleHysteresis;
            next = {index < 0 || aboveRange || belowRange ? SensorFormat::naturalRangeIndex(unit, magnitude) : index,
                    false};
        }
        if (latch)
            *latch = next;
        return ranges.at(next.rangeIndex).scale;
    }

    /** Full scale of a PWM output as the kernel reports it (pwm*: 0..255). */
    inline constexpr double kPwmFullScale = 255.0;

    /** Duty cycle in percent of a PWM output reading @p duty out of kPwmFullScale; clamped to 0..100. */
    [[nodiscard]] inline double pwmDutyPercent(const double duty) {
        return std::clamp(duty * 100.0 / kPwmFullScale, 0.0, 100.0);
    }

    /**
     * Fills the bar graph range where the firmware reports no limits, per quantity:
     * temperatures 0..kDefaultTemperatureMaxC, fans 0..@p fanDefaultMaxRpm, PWM and humidity
     * 0..100 %, voltages, currents and power from the value (current and power mirrored
     * below zero when negative). Energy counters and frequencies have no natural scale and
     * get no default range, so their bar graph stays dark. Without a finite value no bar
     * graph is drawn; the range is only filled to stay valid. Expects raw source units.
     */
    inline void applyDefaultRangePolicy(
        const SensorUnit unit,
        const std::optional<double> value,
        std::optional<double> &min,
        std::optional<double> &max,
        const int fanDefaultMaxRpm
    ) {
        const auto fill = [&min, &max](const double low, const double high) {
            if (!min.has_value()) min = low;
            if (!max.has_value()) max = high;
        };
        const bool hasValue = value && std::isfinite(*value);
        switch (unit) {
            case SensorUnit::Celsius:
                fill(0.0, kDefaultTemperatureMaxC);
                break;
            case SensorUnit::Fahrenheit:
                fill(32.0, kDefaultTemperatureMaxC * 9.0 / 5.0 + 32.0);
                break;
            case SensorUnit::Rpm:
                fill(0.0, static_cast<double>(fanDefaultMaxRpm));
                break;
            case SensorUnit::Percent:
            case SensorUnit::RelativeHumidity:
                fill(0.0, 100.0);
                break;
            case SensorUnit::Joule:
            case SensorUnit::Hertz:
                return;
            case SensorUnit::Volt:
            case SensorUnit::Ampere:
            case SensorUnit::Watt: {
                if (!hasValue) {
                    fill(0.0, 1.0);
                    break;
                }
                const double measuredValue = *value;
                // Current and power are signed (battery charge/discharge); mirror the range below zero.
                const bool signedWithoutNativeMin = unit != SensorUnit::Volt && measuredValue < 0.0 && !min.has_value();
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
                break;
            }
            case SensorUnit::Unknown: {
                if (!hasValue) {
                    fill(0.0, 1.0);
                    break;
                }
                const double measuredValue = *value;
                if (!min.has_value() && !max.has_value()) {
                    const double span = std::max(1.0, std::abs(measuredValue) * 0.5);
                    min = measuredValue - span;
                    max = measuredValue + span;
                } else if (!min.has_value() && max.has_value()) {
                    min = *max - std::max(1.0, std::abs(*max) * 0.8);
                } else if (min.has_value() && !max.has_value()) {
                    max = *min + std::max(1.0, std::abs(*min) * 0.8);
                }
                break;
            }
        }

        // A lone firmware minimum above the default maximum must not leave the range inverted.
        if (min.has_value() && max.has_value() && *max <= *min) {
            *max = *min + std::max(1.0, std::abs(*min) * 0.5);
        }
    }

    /**
     * Which firmware limit the reading violates: fans below min (Low), temperatures above
     * max (High), electrical values, humidity and frequency below min (Low) or above max
     * (High); PWM outputs and energy counters never alert. Default ranges never alert,
     * except that temperatures without a firmware maximum are High above
     * kDefaultTemperatureMaxC. A reading without a value never alerts.
     */
    [[nodiscard]] inline AlertState alertState(const SensorReading &reading) {
        if (!reading.value) {
            return AlertState::None;
        }
        const double value = *reading.value;
        const bool belowMin = reading.firmwareMin && value < *reading.firmwareMin;
        const bool aboveMax = reading.firmwareMax && value > *reading.firmwareMax;
        const auto state = [](const bool low, const bool high) {
            return high ? AlertState::High : low ? AlertState::Low : AlertState::None;
        };
        switch (reading.unit) {
            case SensorUnit::Rpm:
                return state(belowMin, false);
            case SensorUnit::Celsius:
                return state(false, value > reading.firmwareMax.value_or(kDefaultTemperatureMaxC));
            case SensorUnit::Fahrenheit:
                return state(false, value > reading.firmwareMax.value_or(kDefaultTemperatureMaxC * 9.0 / 5.0 + 32.0));
            case SensorUnit::Volt:
            case SensorUnit::Ampere:
            case SensorUnit::Watt:
            case SensorUnit::RelativeHumidity:
            case SensorUnit::Hertz:
                return state(belowMin, aboveMax);
            case SensorUnit::Percent:
            case SensorUnit::Joule:
            case SensorUnit::Unknown:
                break;
        }
        return AlertState::None;
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
