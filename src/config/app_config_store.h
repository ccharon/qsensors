// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "config/runtime_config.h"

/** Reads/writes persisted runtime configuration via QSettings. */
namespace AppConfigStore {
    /** Loads the runtime config; missing or invalid values fall back to defaults and are logged. */
    [[nodiscard]] RuntimeConfig loadRuntimeConfig();

    /** Writes the runtime config to disk; returns false (and logs) if it could not be stored. */
    [[nodiscard]] bool saveRuntimeConfig(const RuntimeConfig &config);
}
