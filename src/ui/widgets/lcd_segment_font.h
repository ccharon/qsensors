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
 * Vector 7-segment font for the digits of the LCD display.
 *
 * Characters are masks over a 7-segment cell plus a decimal point. Geometry is
 * computed in cell-local coordinates, so the display scales freely with widget size
 * and device pixel ratio. Unit symbols are not segment characters; they are drawn by
 * LcdSymbolFont.
 *
 * Segment names:
 *
 *      ---A---
 *     |       |
 *     F       B
 *     |       |
 *      -G1-G2-
 *     |       |
 *     E       C
 *     |       |
 *      ---D---  DP
 *
 * The middle bar is split into G1 and G2 with flat inner ends, so it reads as one bar
 * with a hairline seam. The decimal point lies in the gap after the cell.
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
        DP = 1u << 8,
    };

    using SegmentMask = quint32;

    /** Highest segment bit; iterate `for (bit = 1; bit <= kLastSegment; bit <<= 1)`. */
    inline constexpr SegmentMask kLastSegment = DP;

    /** Segments of a classic 7-segment digit; used for digit ghosting. */
    inline constexpr SegmentMask kSevenSegments = A | B | C | D | E | F | G1 | G2;

    /** Lit and faintly drawn (ghost) segments of a digit cell. */
    struct Glyph final {
        SegmentMask lit = 0;
        SegmentMask ghost = 0; // unlit segments drawn faintly, like a real LCD
    };

    /** Glyph for a digit, '-', ' ' or the letters of "OL"; std::nullopt otherwise. */
    [[nodiscard]] std::optional<Glyph> glyphFor(QChar c);

    /** True when every character of @p text has a glyph or is a decimal point. */
    [[nodiscard]] bool supports(QStringView text);

    /** Horizontal shear applied to every cell (x offset per unit of height above baseline). */
    inline constexpr qreal kSlant = 0.10;

    /** Drawable width of a digit cell and the pitch between cells, relative to the digit height. */
    inline constexpr qreal kDigitWidth = 0.50;
    inline constexpr qreal kDigitAdvance = 0.66;

    /** Polygon of @p segment for an unslanted cell of @p cellSize at origin. */
    [[nodiscard]] QPolygonF segmentPolygon(Segment segment, const QSizeF &cellSize);

    /** Positioned glyph produced by layoutCells(). */
    struct PlacedGlyph final {
        QChar symbol;
        Glyph glyph;
        QRectF cell; // unslanted cell rectangle, relative to the first cell
    };

    /** Width of @p cellCount digit cells at @p height, slant overhang included. */
    [[nodiscard]] qreal cellsWidth(int cellCount, qreal height);

    /** Number of digit cells at @p height that fit into @p width. */
    [[nodiscard]] int cellsThatFit(qreal width, qreal height);

    /**
     * Lays out @p digits (digit characters, '-', ' ', "OL", with '.' lighting the decimal
     * point of the preceding cell) right-aligned in @p cellCount cells starting at x = 0.
     * Unused cells on the left are ghost digits. The rightmost @p valueCells cells form
     * the value field and show faint decimal points between them. Cells grow to the left
     * if @p digits needs more than @p cellCount.
     */
    [[nodiscard]] QVector<PlacedGlyph> layoutCells(QStringView digits, int cellCount, int valueCells, qreal height);
}
