// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "runtime_config.h"

#include <QString>
#include <QStringView>

std::optional<TemperatureUnit> temperatureUnitFromToken(const QStringView token) {
    const QStringView trimmed = token.trimmed();
    if (trimmed.compare(QStringLiteral("F"), Qt::CaseInsensitive) == 0)
        return TemperatureUnit::Fahrenheit;
    if (trimmed.compare(QStringLiteral("C"), Qt::CaseInsensitive) == 0)
        return TemperatureUnit::Celsius;
    return std::nullopt;
}

QString temperatureUnitToToken(const TemperatureUnit unit) {
    return unit == TemperatureUnit::Fahrenheit ? QStringLiteral("F") : QStringLiteral("C");
}

