// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <algorithm>

/** Rules for the main window size that do not depend on widgets. */
namespace WindowSizing {
    /**
     * Maximum window height: the user cannot drag the window taller than its content,
     * but a window that is already taller (content shrank) is never forced smaller.
     * Always within the available screen height.
     */
    [[nodiscard]] constexpr int maximumHeight(const int contentHeight, const int currentHeight, const int screenHeight) {
        return std::min(std::max(contentHeight, currentHeight), std::max(screenHeight, currentHeight));
    }
}
