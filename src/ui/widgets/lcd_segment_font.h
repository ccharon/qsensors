// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QChar>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>
#include <optional>

/**
 * Vector segment font for the LCD display.
 *
 * Characters are described as masks over a 14-segment cell plus a decimal
 * point. Geometry is computed in cell-local coordinates, so the display scales
 * freely with widget size and device pixel ratio.
 *
 * Segment names:
 *
 *      ---A---
 *     |\  |  /|
 *     F H I J B
 *     |  \|/  |
 *      -G1-G2-
 *     |  /|\  |
 *     E K L M C
 *     |/  |  \|
 *      ---D---   DP
 *
 * N and O are continuous strokes for the lower half of "V" and "W"; together
 * with F and B they form proper letter shapes with clean mitered tips.
 */
namespace LcdSegmentFont {
    /** One bit per segment, as drawn in the diagram above. */
    enum Segment : quint32 {
        A = 1u << 0,
        B = 1u << 1,
        C = 1u << 2,
        D = 1u << 3,
        E = 1u << 4,
        F = 1u << 5,
        G1 = 1u << 6,
        G2 = 1u << 7,
        H = 1u << 8, // diagonal: center -> top-left
        I = 1u << 9, // vertical: center -> top
        J = 1u << 10, // diagonal: center -> top-right
        K = 1u << 11, // diagonal: center -> bottom-left
        L = 1u << 12, // vertical: center -> bottom
        M = 1u << 13, // diagonal: center -> bottom-right
        DP = 1u << 14,
        N = 1u << 15, // lower half of "V": one mitered stroke through the bottom center
        O = 1u << 16, // lower half of "W": one mitered stroke with a center peak
    };

    using SegmentMask = quint32;

    /** Highest segment bit; iterate `for (bit = 1; bit <= kLastSegment; bit <<= 1)`. */
    inline constexpr SegmentMask kLastSegment = O;

    /** Segments of a classic 7-segment digit; used for digit ghosting. */
    inline constexpr SegmentMask kSevenSegments = A | B | C | D | E | F | G1 | G2;

    /** Glyph description; widths are relative to the cell height. */
    struct Glyph final {
        SegmentMask lit = 0;
        SegmentMask ghost = 0; // unlit segments drawn faintly, like a real LCD
        qreal width = 0.0; // drawable cell width / height
        qreal advance = 0.0; // horizontal pen advance / height
    };

    /** Returns glyph for a supported character, std::nullopt otherwise. */
    [[nodiscard]] std::optional<Glyph> glyphFor(QChar c);

    /** True when every character of @p text has a glyph. */
    [[nodiscard]] bool supports(QStringView text);

    /** Horizontal shear applied to every cell (x offset per unit of height above baseline). */
    inline constexpr qreal kSlant = 0.10;

    /** Unit symbols are rendered at this fraction of the digit height, baseline aligned. */
    inline constexpr qreal kUnitScale = 0.70;

    /** Gap between value and unit, relative to the digit height. */
    inline constexpr qreal kUnitGap = 0.18;

    /** Polygon of @p segment inside an unslanted cell of @p cellSize at origin. */
    [[nodiscard]] QPolygonF segmentPolygon(Segment segment, const QSizeF &cellSize);

    /** Positioned glyph produced by layoutText(). */
    struct PlacedGlyph final {
        QChar symbol;
        Glyph glyph;
        QRectF cell; // unslanted cell rectangle in widget coordinates
    };

    /**
     * Lays out @p value at full @p height followed by @p unit at kUnitScale,
     * sharing one baseline at @p height. Unsupported characters are skipped.
     */
    [[nodiscard]] QVector<PlacedGlyph> layoutText(QStringView value, QStringView unit, qreal height);

    /** Total horizontal extent of a layout, including slant overhang. */
    [[nodiscard]] qreal layoutWidth(const QVector<PlacedGlyph> &layout);

    /**
     * Largest digit height <= @p maxHeight at which value and unit fit into
     * @p maxWidth; long readings shrink so nothing is clipped.
     */
    [[nodiscard]] qreal fitHeight(QStringView value, QStringView unit, qreal maxHeight, qreal maxWidth);
}
