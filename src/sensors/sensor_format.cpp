// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/sensor_format.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    using SensorFormat::QuantityFormat;
    using SensorFormat::ScaleRange;

    [[nodiscard]] QVector<ScaleRange> buildRanges(const QuantityFormat &format) {
        QVector<ScaleRange> ranges;
        const int smallest = static_cast<int>(format.smallestPrefix);
        const int largest = static_cast<int>(format.largestPrefix);
        for (int p = smallest; p <= largest; ++p) {
            const auto prefix = static_cast<SiPrefix>(p);
            const double factor = SensorFormat::prefixFactor(prefix);
            if (prefix == SiPrefix::Milli) {
                // Milli shows whole numbers up to the base unit: "880 mV", "396 mA".
                ranges.append({{prefix, 0}, 1000.0 * factor});
                continue;
            }
            // Three decades per prefix ("1.052", "12.10", "230.1"); the largest prefix goes
            // on with whole numbers until all cells are digits.
            const int decades = p == largest ? SensorFormat::kValueCells : 3;
            for (int k = 0; k < decades; ++k) {
                const int decimals = std::clamp(SensorFormat::kSignificantDigits - 1 - k, 0, format.maxDecimals);
                const bool last = p == largest && k == decades - 1;
                const double upper = last ? std::numeric_limits<double>::infinity() : std::pow(10.0, k + 1) * factor;
                ranges.append({{prefix, decimals}, upper});
            }
        }
        return ranges;
    }

    [[nodiscard]] double roundTo(const double value, const int decimals) {
        const double scale = std::pow(10.0, decimals);
        return std::round(value * scale) / scale;
    }

    /** Fixed-point text of @p value without a minus sign when it rounds to zero. */
    [[nodiscard]] QString fixed(const double value, const int decimals) {
        QString text = QString::number(value, 'f', decimals);
        if (text.startsWith(QLatin1Char('-'))
            && std::none_of(text.cbegin(), text.cend(), [](const QChar c) { return c >= u'1' && c <= u'9'; })) {
            text.remove(0, 1);
        }
        return text;
    }

    [[nodiscard]] int cellsOf(const QString &digits) {
        return static_cast<int>(digits.size() - digits.count(QLatin1Char('.')));
    }

    [[nodiscard]] QString rightAligned(const QString &digits) {
        return QString(std::max(0, SensorFormat::kValueCells - cellsOf(digits)), QLatin1Char(' ')) + digits;
    }
}

const SensorFormat::QuantityFormat &SensorFormat::quantityFormat(const SensorUnit unit) {
    // One row per quantity: symbol, prefixes, most decimals. The LCD and the tooltips
    // draw exactly what this table allows.
    static const QuantityFormat kCelsius{QStringLiteral("°C"), SiPrefix::None, SiPrefix::None, 1};
    static const QuantityFormat kFahrenheit{QStringLiteral("°F"), SiPrefix::None, SiPrefix::None, 1};
    static const QuantityFormat kVolt{QStringLiteral("V"), SiPrefix::Milli, SiPrefix::None, 3};
    static const QuantityFormat kRpm{QStringLiteral("RPM"), SiPrefix::None, SiPrefix::None, 0};
    static const QuantityFormat kAmpere{QStringLiteral("A"), SiPrefix::Milli, SiPrefix::Kilo, 3};
    static const QuantityFormat kWatt{QStringLiteral("W"), SiPrefix::Milli, SiPrefix::Kilo, 3};
    static const QuantityFormat kPercent{QStringLiteral("%"), SiPrefix::None, SiPrefix::None, 0};
    static const QuantityFormat kRelativeHumidity{QStringLiteral("%RH"), SiPrefix::None, SiPrefix::None, 0};
    // Energy meters are counters that grow for as long as the machine runs.
    static const QuantityFormat kJoule{QStringLiteral("J"), SiPrefix::None, SiPrefix::Giga, 3};
    static const QuantityFormat kHertz{QStringLiteral("Hz"), SiPrefix::None, SiPrefix::Mega, 3};
    static const QuantityFormat kUnknown{QString(), SiPrefix::None, SiPrefix::None, 3};
    switch (unit) {
        case SensorUnit::Celsius: return kCelsius;
        case SensorUnit::Fahrenheit: return kFahrenheit;
        case SensorUnit::Volt: return kVolt;
        case SensorUnit::Rpm: return kRpm;
        case SensorUnit::Ampere: return kAmpere;
        case SensorUnit::Watt: return kWatt;
        case SensorUnit::Percent: return kPercent;
        case SensorUnit::RelativeHumidity: return kRelativeHumidity;
        case SensorUnit::Joule: return kJoule;
        case SensorUnit::Hertz: return kHertz;
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
        case SiPrefix::Mega: return QLatin1Char('M') + symbol;
        case SiPrefix::Giga: return QLatin1Char('G') + symbol;
    }
    return symbol;
}

const QVector<SensorFormat::ScaleRange> &SensorFormat::scaleRanges(const SensorUnit unit) {
    static const auto build = [](const SensorUnit u) { return buildRanges(quantityFormat(u)); };
    static const QVector<ScaleRange> kCelsius = build(SensorUnit::Celsius);
    static const QVector<ScaleRange> kFahrenheit = build(SensorUnit::Fahrenheit);
    static const QVector<ScaleRange> kVolt = build(SensorUnit::Volt);
    static const QVector<ScaleRange> kRpm = build(SensorUnit::Rpm);
    static const QVector<ScaleRange> kAmpere = build(SensorUnit::Ampere);
    static const QVector<ScaleRange> kWatt = build(SensorUnit::Watt);
    static const QVector<ScaleRange> kPercent = build(SensorUnit::Percent);
    static const QVector<ScaleRange> kRelativeHumidity = build(SensorUnit::RelativeHumidity);
    static const QVector<ScaleRange> kJoule = build(SensorUnit::Joule);
    static const QVector<ScaleRange> kHertz = build(SensorUnit::Hertz);
    static const QVector<ScaleRange> kUnknown = build(SensorUnit::Unknown);
    switch (unit) {
        case SensorUnit::Celsius: return kCelsius;
        case SensorUnit::Fahrenheit: return kFahrenheit;
        case SensorUnit::Volt: return kVolt;
        case SensorUnit::Rpm: return kRpm;
        case SensorUnit::Ampere: return kAmpere;
        case SensorUnit::Watt: return kWatt;
        case SensorUnit::Percent: return kPercent;
        case SensorUnit::RelativeHumidity: return kRelativeHumidity;
        case SensorUnit::Joule: return kJoule;
        case SensorUnit::Hertz: return kHertz;
        case SensorUnit::Unknown: break;
    }
    return kUnknown;
}

int SensorFormat::baseRangeIndex(const SensorUnit unit) {
    const QVector<ScaleRange> &ranges = scaleRanges(unit);
    for (int i = 0; i < ranges.size(); ++i) {
        if (ranges.at(i).scale.prefix == SiPrefix::None)
            return i;
    }
    return 0;
}

int SensorFormat::naturalRangeIndex(const SensorUnit unit, const double magnitude) {
    const QVector<ScaleRange> &ranges = scaleRanges(unit);
    for (int i = 0; i < ranges.size(); ++i) {
        const ScaleRange &range = ranges.at(i);
        const double factor = prefixFactor(range.scale.prefix);
        if (roundTo(magnitude / factor, range.scale.decimals) < range.upperBound / factor)
            return i;
    }
    return static_cast<int>(ranges.size()) - 1;
}

SensorScale SensorFormat::scaleOf(const SensorReading &reading) {
    if (reading.scale)
        return *reading.scale;
    const int index = reading.value && std::isfinite(*reading.value)
                          ? naturalRangeIndex(reading.unit, std::abs(*reading.value))
                          : baseRangeIndex(reading.unit);
    return scaleRanges(reading.unit).at(index).scale;
}

SensorFormat::DisplayValue SensorFormat::display(const SensorUnit unit, const SensorScale &scale,
                                                 const std::optional<double> value) {
    if (!value || !std::isfinite(*value)) {
        const int decimals = std::clamp(scale.decimals, 0, kValueCells - 1);
        QString dashes(kValueCells - decimals, QLatin1Char('-'));
        if (decimals > 0)
            dashes += QLatin1Char('.') + QString(decimals, QLatin1Char('-'));
        return {dashes, unitSymbol(unit, scale.prefix)};
    }

    // The scale's prefix first, with fewer decimals if needed; then larger prefixes.
    const int largest = static_cast<int>(quantityFormat(unit).largestPrefix);
    for (int p = static_cast<int>(scale.prefix); p <= std::max(largest, static_cast<int>(scale.prefix)); ++p) {
        const auto prefix = static_cast<SiPrefix>(p);
        const double scaled = *value / prefixFactor(prefix);
        const int firstDecimals = prefix == scale.prefix ? scale.decimals : quantityFormat(unit).maxDecimals;
        for (int decimals = std::max(firstDecimals, 0); decimals >= 0; --decimals) {
            const QString digits = fixed(scaled, decimals);
            if (cellsOf(digits) <= kValueCells)
                return {rightAligned(digits), unitSymbol(unit, prefix)};
        }
    }
    return {rightAligned(QStringLiteral("OL")), unitSymbol(unit, scale.prefix)};
}

SensorFormat::DisplayValue SensorFormat::display(const SensorReading &reading) {
    return display(reading.unit, scaleOf(reading), reading.value);
}

QString SensorFormat::valueWithUnit(const SensorReading &reading, const double value) {
    const DisplayValue shown = display(reading.unit, scaleOf(reading), value);
    const QString digits = shown.digits.trimmed();
    return shown.symbol.isEmpty() ? digits : digits + QLatin1Char(' ') + shown.symbol;
}
