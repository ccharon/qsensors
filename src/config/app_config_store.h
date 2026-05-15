// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "runtime_config.h"

/** Reads/writes persisted runtime configuration via QSettings. */
class AppConfigStore final {
public:
    [[nodiscard]] static RuntimeConfig loadRuntimeConfig();
    static void saveRuntimeConfig(const RuntimeConfig &config);
};

