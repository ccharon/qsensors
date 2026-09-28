// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors/sensor_reading.h"

#include <QString>

/** Text formatting of sensor values, shared by the LCD and tooltips. */
namespace SensorFormat {
    /** Right-aligned digits with the per-unit precision used on the LCD, e.g. " 38.3" or " 1534". */
    [[nodiscard]] QString valueDigits(SensorUnit unit, double value);

    /** @p value with LCD precision and unit symbol, trimmed, e.g. "9.89 V". */
    [[nodiscard]] QString valueWithUnit(SensorUnit unit, double value);
}
