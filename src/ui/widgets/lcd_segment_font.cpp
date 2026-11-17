// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/widgets/lcd_segment_font.h"

#include <QHash>
#include <algorithm>

namespace LcdSegmentFont {
    namespace {
        // Proportions relative to the cell height.
        constexpr qreal kThickness = 0.12;
        constexpr qreal kGap = 0.25; // relative to thickness
        constexpr qreal kDecimalPointSize = 0.10; // square

        Glyph digit(const SegmentMask lit) {
            return {lit, kSevenSegments};
        }

        const QHash<QChar, Glyph> &glyphTable() {
            static const QHash<QChar, Glyph> table = {
                {u'0', digit(A | B | C | D | E | F)},
                {u'1', digit(B | C)},
                {u'2', digit(A | B | G1 | G2 | E | D)},
                {u'3', digit(A | B | C | D | G1 | G2)},
                {u'4', digit(F | G1 | G2 | B | C)},
                {u'5', digit(A | F | G1 | G2 | C | D)},
                {u'6', digit(A | F | E | D | C | G1 | G2)},
                {u'7', digit(A | B | C)},
                {u'8', digit(kSevenSegments)},
                {u'9', digit(A | B | C | D | F | G1 | G2)},
                {u'-', digit(G1 | G2)},
                {u' ', digit(0)},
                {u'O', digit(A | B | C | D | E | F)}, // overload: "OL"
                {u'L', digit(D | E | F)},
            };
            return table;
        }

        enum class End { Pointed, Flat };

        // Horizontal bar; flat ends are used where the middle bar is split into G1/G2,
        // so both halves read as one continuous bar with a hairline seam.
        QPolygonF horizontal(const qreal xa, const qreal xb, const qreal y, const qreal t, const qreal g,
                             const End left = End::Pointed, const End right = End::Pointed) {
            const qreal h = t / 2.0;
            QPolygonF poly;
            if (left == End::Pointed)
                poly << QPointF(xa + g, y) << QPointF(xa + g + h, y - h);
            else
                poly << QPointF(xa + g / 2.0, y + h) << QPointF(xa + g / 2.0, y - h);
            if (right == End::Pointed)
                poly << QPointF(xb - g - h, y - h) << QPointF(xb - g, y) << QPointF(xb - g - h, y + h);
            else
                poly << QPointF(xb - g / 2.0, y - h) << QPointF(xb - g / 2.0, y + h);
            if (left == End::Pointed)
                poly << QPointF(xa + g + h, y + h);
            return poly;
        }

        QPolygonF vertical(const qreal x, const qreal ya, const qreal yb, const qreal t, const qreal g) {
            const qreal h = t / 2.0;
            return QPolygonF{{
                {x, ya + g}, {x + h, ya + g + h}, {x + h, yb - g - h},
                {x, yb - g}, {x - h, yb - g - h}, {x - h, ya + g + h},
            }};
        }
    }

    std::optional<Glyph> glyphFor(const QChar c) {
        const auto &table = glyphTable();
        const auto it = table.constFind(c);
        return it != table.cend() ? std::optional(*it) : std::nullopt;
    }

    bool supports(const QStringView text) {
        // A '.' is the decimal point of the preceding digit cell.
        return std::all_of(text.begin(), text.end(),
                           [](const QChar c) { return c == u'.' || glyphFor(c).has_value(); });
    }

    QPolygonF segmentPolygon(const Segment segment, const QSizeF &cellSize) {
        const qreal w = cellSize.width();
        const qreal h = cellSize.height();
        const qreal t = kThickness * h;
        const qreal g = kGap * t;
        const qreal x0 = t / 2.0, x1 = w - t / 2.0, xc = w / 2.0;
        const qreal y0 = t / 2.0, y1 = h - t / 2.0, ym = h / 2.0;

        switch (segment) {
            case A: return horizontal(x0, x1, y0, t, g);
            case D: return horizontal(x0, x1, y1, t, g);
            case G1: return horizontal(x0, xc, ym, t, g, End::Pointed, End::Flat);
            case G2: return horizontal(xc, x1, ym, t, g, End::Flat, End::Pointed);
            case F: return vertical(x0, y0, ym, t, g);
            case E: return vertical(x0, ym, y1, t, g);
            case B: return vertical(x1, y0, ym, t, g);
            case C: return vertical(x1, ym, y1, t, g);
            case DP: {
                // In the gap after the cell, on the baseline, like the point of a real LCD digit.
                const qreal size = kDecimalPointSize * h;
                const qreal x = w + (kDigitAdvance - kDigitWidth) * h / 2.0 - size / 2.0;
                return QPolygonF(QRectF(x, h - size, size, size));
            }
        }
        return {};
    }

    qreal cellsWidth(const int cellCount, const qreal height) {
        if (cellCount <= 0)
            return 0.0;
        return ((cellCount - 1) * kDigitAdvance + kDigitWidth + kSlant) * height;
    }

    int cellsThatFit(const qreal width, const qreal height) {
        // The tolerance keeps an exact fit from losing a cell to rounding.
        constexpr qreal kTolerance = 1e-9;
        const qreal spare = width - cellsWidth(1, height) + kTolerance;
        if (height <= 0.0 || spare < 0.0)
            return 0;
        return 1 + static_cast<int>(spare / (kDigitAdvance * height));
    }

    QVector<PlacedGlyph> layoutCells(const QStringView digits, const int cellCount, const int valueCells,
                                     const qreal height) {
        // Characters with a cell; a '.' lights the decimal point of the cell before it.
        QVector<Glyph> value;
        QVector<QChar> symbols;
        for (const QChar c: digits) {
            if (c == u'.') {
                if (!value.isEmpty())
                    value.last().lit |= DP;
                continue;
            }
            if (const auto glyph = glyphFor(c)) {
                value.append(*glyph);
                symbols.append(c);
            }
        }

        const int count = std::max<int>(cellCount, static_cast<int>(value.size()));
        const int firstValueCell = count - std::max<int>(valueCells, static_cast<int>(value.size()));
        const int firstDigit = count - static_cast<int>(value.size());
        QVector<PlacedGlyph> placed;
        placed.reserve(count);
        for (int i = 0; i < count; ++i) {
            Glyph glyph = i >= firstDigit ? value.at(i - firstDigit) : digit(0);
            // Value cells show a faint decimal point between them; the last one has none after it.
            if (i >= firstValueCell && i < count - 1)
                glyph.ghost |= DP;
            const QRectF cell(i * kDigitAdvance * height, 0.0, kDigitWidth * height, height);
            placed.append({i >= firstDigit ? symbols.at(i - firstDigit) : QChar(u' '), glyph, cell});
        }
        return placed;
    }
}
