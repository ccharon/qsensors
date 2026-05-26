// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "runtime_config.h"

/** Reads/writes persisted runtime configuration via QSettings. */
namespace AppConfigStore {
    [[nodiscard]] RuntimeConfig loadRuntimeConfig();
    void saveRuntimeConfig(const RuntimeConfig &config);
}

