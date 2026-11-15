// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <algorithm>

/** Rules for the main window size that do not depend on widgets. */
namespace WindowSizing {
    /**
     * Maximum window width or height: the user cannot drag the window larger than its
     * content needs, but a window that is already larger (content shrank) is never
     * forced smaller. Always within the available screen size.
     */
    [[nodiscard]] constexpr int maximumExtent(const int content, const int current, const int screen) {
        return std::min(std::max(content, current), std::max(screen, current));
    }
}
