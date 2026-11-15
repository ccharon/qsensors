// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors/sensor_reading.h"

#include <QString>
#include <optional>

/** Text formatting of sensor values, shared by the LCD and tooltips. */
namespace SensorFormat {
    /**
     * Right-aligned digits with the per-unit precision used on the LCD, e.g. "  38.3" or
     * " 1534". Without a value, every digit cell shows a dash, e.g. "----.-".
     */
    [[nodiscard]] QString valueDigits(SensorUnit unit, std::optional<double> value);

    /** @p value with LCD precision and unit symbol, trimmed, e.g. "9.89 V". */
    [[nodiscard]] QString valueWithUnit(SensorUnit unit, double value);
}
