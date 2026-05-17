// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QString>

namespace AppTheme {
    inline constexpr int kCardMinWidth = 150;
    inline constexpr int kCardWidth = 170;
    inline constexpr int kCardBorderPadding = 2;
    inline constexpr int kGridSpacing = 4;
    inline constexpr int kCategoryVsGridSpacingDelta = 2;
    inline constexpr int kChipCardFrameWidthTotal = 2; // 1px left + 1px right
    inline constexpr int kSensorsPanelVerticalSpacing = 10;
    inline constexpr int kCategoryBlockSpacing = 3;
    inline constexpr int kInitialWidthFitPadding = 24;
    inline constexpr int kRestoredWidthFitPadding = 8;
    inline constexpr int kMaxColumnsPerCategory = 5;
    inline constexpr int kSectionInset = 8;
    inline constexpr int kNarrowGap = 2;
    inline constexpr int kRangeBarHeight = 4;
    inline constexpr int kLcdLightThemeBrightness = 245;
    inline constexpr int kLcdDarkThemeBrightness = 230;

    [[nodiscard]] QString chipCardStyle();

    [[nodiscard]] QString sectionHeaderStyle();

    [[nodiscard]] QString settingsCardStyle();

    [[nodiscard]] QString sensorGroupStyle(const QString &borderColor);

    [[nodiscard]] QString spinBoxStyle();

    [[nodiscard]] QString progressBarStyle(bool hasRange);
}
