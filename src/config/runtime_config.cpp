// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "runtime_config.h"

#include <QString>
#include <QStringView>

TemperatureUnit temperatureUnitFromToken(const QStringView token) {
    return token.compare(QStringLiteral("F"), Qt::CaseInsensitive) == 0
               ? TemperatureUnit::Fahrenheit
               : TemperatureUnit::Celsius;
}

QString temperatureUnitToToken(const TemperatureUnit unit) {
    return unit == TemperatureUnit::Fahrenheit ? QStringLiteral("F") : QStringLiteral("C");
}

