// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QLatin1String>

/** QSettings keys; renaming one requires a schema migration (see SettingsSchema). */
namespace SettingsKeys {
    inline constexpr QLatin1String kSchemaVersion("meta/schema_version");
    inline constexpr QLatin1String kPollingIntervalSec("runtime/polling_interval_sec");
    inline constexpr QLatin1String kFanDefaultMaxRpm("runtime/fan_default_max_rpm");
    inline constexpr QLatin1String kTemperatureUnit("runtime/temperature_unit");
    inline constexpr QLatin1String kWindowGeometry("ui/geometry");
    inline constexpr QLatin1String kChipExpandedGroup("ui/chips");
    inline constexpr QLatin1String kSensorFingerprint("sensors/fingerprint");
}
