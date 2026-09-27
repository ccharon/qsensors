// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QColor>
#include <QPalette>
#include <QString>

class QWidget;

/** Layout metrics, LCD colors and style sheets; change the look here, not in widgets. */
namespace AppTheme {
    // Window size on first start, before any geometry was saved.
    inline constexpr int kInitialWindowWidth = 900;
    inline constexpr int kInitialWindowHeight = 520;
    // Sensor card width range; the column calculation uses the minimum.
    inline constexpr int kCardMinWidth = 150;
    inline constexpr int kCardWidth = 170;
    // Sensor card title: slightly smaller theme font, aligned with the LCD panel edge.
    inline constexpr qreal kCardTitleFontScale = 0.9;
    inline constexpr int kCardTitleInset = 2;
    inline constexpr int kCardTitleSpacing = 1;
    // Spacing between cards, categories and sections in px.
    inline constexpr int kGridSpacing = 4;
    inline constexpr int kCategoryVsGridSpacingDelta = 2;
    inline constexpr int kUnifiedHorizontalSpacing = kGridSpacing + kCategoryVsGridSpacingDelta;
    inline constexpr int kChipCardFrameWidthTotal = 2; // 1px left + 1px right
    inline constexpr int kSensorsPanelVerticalSpacing = 10;
    inline constexpr int kCategoryBlockSpacing = 3;
    // Extra width when fitting the window to its content on first show.
    inline constexpr int kInitialWidthFitPadding = 24;
    inline constexpr int kRestoredWidthFitPadding = 8;
    inline constexpr int kMaxColumnsPerCategory = 6;
    inline constexpr int kSectionInset = 8;
    inline constexpr int kNarrowGap = 2;
    // Lower bound for stable column calculation; prevents layout collapse at zero width.
    inline constexpr int kMinStableViewportWidth = 200;
    // Segment LCD colors (xsensors palette); ghost alpha applies to unlit segments.
    inline constexpr QRgb kLcdNormalRgb = 0x4a7c46;
    inline constexpr QRgb kLcdAlertRgb = 0xc14433;
    inline constexpr int kLcdGhostAlpha = 28;
    // Dark themes: brighter backlit segment colors with a glow.
    inline constexpr QRgb kLcdNormalDarkRgb = 0x7ccc72;
    inline constexpr QRgb kLcdAlertDarkRgb = 0xf0664f;
    inline constexpr int kLcdGhostAlphaDark = 20;
    inline constexpr int kLcdGlowAlpha = 34;
    // LCD backplane: tint of the palette base towards the LCD green, corner radius,
    // and alpha of the offset shadow cast by lit segments.
    inline constexpr qreal kLcdTintStrength = 0.13;
    inline constexpr qreal kLcdCornerRadius = 3.0;
    inline constexpr int kLcdSegmentShadowAlpha = 38;
    // Inner spacing between LCD segments and the display border.
    inline constexpr int kLcdPaddingX = 4;
    inline constexpr int kLcdPaddingY = 3;
    // Range bar graph along the bottom of the LCD.
    inline constexpr int kLcdBarHeight = 4;
    inline constexpr int kLcdBarGap = 3;
    inline constexpr qreal kLcdBarSegmentWidth = 3.5;
    inline constexpr qreal kLcdBarSegmentSpacing = 1.5;

    /** Frame of a CollapsibleSection (object name "sectionCard"). */
    [[nodiscard]] QString sectionCardStyle();

    /** Toggle header of a CollapsibleSection. */
    [[nodiscard]] QString sectionHeaderStyle();

    /** Minimum width for the settings spin boxes. */
    [[nodiscard]] QString spinBoxStyle();

    /** Minimum width for the settings combo box. */
    [[nodiscard]] QString comboBoxStyle();

    /**
     * Re-applies every style sheet below @p root. Qt resolves palette(...) only when a
     * sheet is set, so this is needed after a runtime light/dark switch.
     */
    void refreshStyleSheets(QWidget *root);
}
