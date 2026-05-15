// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QString>

namespace AppTheme {
    inline constexpr int kCardWidth = 170;
    inline constexpr int kCardMinHeight = 74;
    inline constexpr int kCardBorderPadding = 4;
    inline constexpr int kCategorySpacing = 12;
    inline constexpr int kGridSpacing = 3;
    inline constexpr int kMaxColumnsPerCategory = 3;
    inline constexpr int kSectionInset = 8;
    inline constexpr int kNarrowGap = 2;
    inline constexpr int kRangeBarHeight = 6;
    inline constexpr int kLcdLightThemeBrightness = 245;
    inline constexpr int kLcdDarkThemeBrightness = 230;

    [[nodiscard]] QString chipCardStyle();

    [[nodiscard]] QString sectionHeaderStyle();

    [[nodiscard]] QString settingsCardStyle();

    [[nodiscard]] QString sensorGroupStyle(const QString &borderColor);

    [[nodiscard]] QString spinBoxStyle();

    [[nodiscard]] QString progressBarStyle(bool hasRange);
}
