// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QPainterPath>
#include <QStringView>

/**
 * Annunciator symbols of the LCD: units ("mV", "°C", "%RH", "kHz") and state words
 * ("HI", "LO") drawn like the pre-shaped symbols of a real LCD.
 *
 * The letters form a bold, narrow face built in code from stroked center lines, so the
 * result does not depend on the fonts installed and is the same on every system. Each
 * glyph is clipped to its box, which gives flat tops, feet and stem ends. Geometry is
 * unslanted and relative to the cap height; the display applies the segment slant.
 */
namespace LcdSymbolFont {
    /** Stroke width of the letters, relative to the cap height. */
    inline constexpr qreal kStrokeWeight = 0.18;

    /** Space between letters, relative to the cap height. */
    inline constexpr qreal kLetterSpacing = 0.11;

    /** True when every character of @p text has a glyph. */
    [[nodiscard]] bool supports(QStringView text);

    /**
     * Filled outline of @p text with cap height @p height: top-left at the origin,
     * baseline at y = @p height, unslanted. Unsupported characters are skipped.
     */
    [[nodiscard]] QPainterPath path(QStringView text, qreal height);

    /** Advance width of @p text at cap height @p height, without slant overhang. */
    [[nodiscard]] qreal width(QStringView text, qreal height);
}
