// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/widgets/lcd_segment_font.h"

#include <QHash>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <algorithm>
#include <cmath>

namespace LcdSegmentFont {
    namespace {
        // Proportions relative to the cell height.
        constexpr qreal kThickness = 0.12;
        constexpr qreal kGap = 0.25; // relative to thickness
        constexpr qreal kDiagonalThickness = 0.85; // relative to thickness

        constexpr qreal kDecimalPointSize = 0.10; // square, relative to the cell height
        constexpr qreal kLetterWidth = 0.62;
        constexpr qreal kLetterAdvance = 0.72;

        Glyph digit(const SegmentMask lit) {
            return {lit, kSevenSegments, kDigitWidth, kDigitAdvance};
        }

        Glyph letter(const SegmentMask lit) {
            return {lit, 0, kLetterWidth, kLetterAdvance};
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

                {u'A', letter(A | B | C | E | F | G1 | G2)},
                {u'C', letter(A | D | E | F)},
                {u'F', letter(A | E | F | G1)},
                {u'M', letter(B | C | E | F | H | J)},
                {u'P', letter(A | B | E | F | G1 | G2)},
                {u'R', letter(A | B | E | F | G1 | G2 | M)},
                {u'V', letter(F | B | N)},
                {u'W', {F | B | O, 0, 0.86, 0.96}}, // wide: two Vs
                {u'G', letter(A | C | D | E | F | G2)},
                {u'H', letter(B | C | E | F | G1 | G2)},
                {u'J', letter(B | C | D | E)},
                {u'k', letter(E | F | G1 | J | M)},
                {u'z', letter(D | G1 | K)},
                {u'%', letter(F | C | J | K)},
                {u'm', letter(C | E | G1 | G2 | L)},
                {QChar(0x00B0), {A | B | F | G1 | G2, 0, 0.46, 0.52}}, // degree sign
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

        // Diagonal filling the inner corner between `corner` and `center`, with
        // axis-aligned ends so it tucks neatly against neighbouring segments.
        QPolygonF diagonal(const QPointF corner, const QPointF center, const qreal td) {
            const qreal spanX = std::abs(center.x() - corner.x());
            const qreal spanY = std::abs(center.y() - corner.y());
            const qreal len = std::hypot(spanX, spanY);
            if (len <= 0.0)
                return {};
            const qreal sx = center.x() > corner.x() ? 1.0 : -1.0;
            const qreal sy = center.y() > corner.y() ? 1.0 : -1.0;
            const qreal dx = std::min(td * len / spanY, spanX * 0.9);
            const qreal dy = std::min(td * len / spanX, spanY * 0.9);
            return QPolygonF{{
                corner, corner + QPointF(sx * dx, 0), center - QPointF(0, sy * dy),
                center, center - QPointF(sx * dx, 0), corner + QPointF(0, sy * dy),
            }};
        }
    }

    namespace {
        // Lower-half letter stroke (V/W): a polyline from the left edge through the
        // given valleys (and peaks between them) to the right edge, stroked with
        // mitered joins slightly thinner than the straight segments (diagonals read
        // heavier). The start is cut with a 45° chevron that mirrors the pointed tip
        // of the F/B segment above, so the gap between them has constant width.
        QPolygonF lowerStroke(const QVector<qreal> &valleys, const QVector<qreal> &peaks,
                              const qreal w, const qreal h, const qreal t, const qreal g) {
            const qreal width = kDiagonalThickness * t;
            const qreal x0 = t / 2.0, x1 = w - t / 2.0;
            const qreal top = h / 2.0 + g; // apex of the chevron cut, one gap below F/B tips
            const qreal valleyY = h - width * 1.1; // mitered outer tip lands on the bottom edge
            const qreal peakY = top + t * 1.6;

            // Centerline starts above the cut at x0/x1 so the clip produces the chevron.
            const qreal lead = t;
            QVector<QPointF> points;
            points << QPointF(x0 - (valleys.first() - x0) * lead / (valleyY - top), top - lead);
            for (int i = 0; i < valleys.size(); ++i) {
                points << QPointF(valleys.at(i), valleyY);
                if (i < peaks.size())
                    points << QPointF(peaks.at(i), peakY);
            }
            points << QPointF(x1 + (x1 - valleys.last()) * lead / (valleyY - top), top - lead);

            QPainterPath line;
            line.addPolygon(QPolygonF(points));
            QPainterPathStroker stroker;
            stroker.setWidth(width);
            stroker.setJoinStyle(Qt::MiterJoin);
            stroker.setMiterLimit(8.0);
            stroker.setCapStyle(Qt::FlatCap);

            QPainterPath clip;
            clip.addPolygon(QPolygonF{{
                {0.0, top + x0}, {x0, top}, {x0 + t, top + t}, {x1 - t, top + t},
                {x1, top}, {w, top + (w - x1)}, {w, h}, {0.0, h},
            }});
            clip.closeSubpath();
            const QList<QPolygonF> polygons = stroker.createStroke(line).simplified().intersected(clip).toFillPolygons();
            return polygons.isEmpty() ? QPolygonF() : polygons.first();
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

        // Inner box corners for diagonals and center verticals.
        const qreal il = x0 + t / 2.0 + g, ir = x1 - t / 2.0 - g;
        const qreal it = y0 + t / 2.0 + g, ib = y1 - t / 2.0 - g;
        const qreal imt = ym - t / 2.0 - g, imb = ym + t / 2.0 + g;
        const qreal td = kDiagonalThickness * t;

        switch (segment) {
            case A: return horizontal(x0, x1, y0, t, g);
            case D: return horizontal(x0, x1, y1, t, g);
            case G1: return horizontal(x0, xc, ym, t, g, End::Pointed, End::Flat);
            case G2: return horizontal(xc, x1, ym, t, g, End::Flat, End::Pointed);
            case F: return vertical(x0, y0, ym, t, g);
            case E: return vertical(x0, ym, y1, t, g);
            case B: return vertical(x1, y0, ym, t, g);
            case C: return vertical(x1, ym, y1, t, g);
            case I: return vertical(xc, y0 + t / 2.0, ym - t / 2.0, t, g);
            case L: return vertical(xc, ym + t / 2.0, y1 - t / 2.0, t, g);
            case H: return diagonal({il, it}, {xc, imt}, td);
            case J: return diagonal({ir, it}, {xc, imt}, td);
            case K: return diagonal({il, ib}, {xc, imb}, td);
            case M: return diagonal({ir, ib}, {xc, imb}, td);
            case DP: {
                // In the gap after the cell, on the baseline, like the point of a real LCD digit.
                const qreal size = kDecimalPointSize * h;
                const qreal x = w + (kDigitAdvance - kDigitWidth) * h / 2.0 - size / 2.0;
                return QPolygonF(QRectF(x, h - size, size, size));
            }
            case N: return lowerStroke({xc}, {}, w, h, t, g);
            case O: return lowerStroke({w * 0.27, w * 0.73}, {xc}, w, h, t, g);
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
            if (const auto glyph = glyphFor(c); glyph && (glyph->ghost & kSevenSegments) == kSevenSegments) {
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
            Glyph glyph = i >= firstDigit ? value.at(i - firstDigit) : *glyphFor(u' ');
            // Value cells show a faint decimal point between them; the last one has none after it.
            if (i >= firstValueCell && i < count - 1)
                glyph.ghost |= DP;
            const QRectF cell(i * kDigitAdvance * height, 0.0, kDigitWidth * height, height);
            placed.append({i >= firstDigit ? symbols.at(i - firstDigit) : QChar(u' '), glyph, cell});
        }
        return placed;
    }

    QVector<PlacedGlyph> layoutText(const QStringView value, const QStringView unit, const qreal height) {
        QVector<PlacedGlyph> placed;
        placed.reserve(value.size() + unit.size());
        qreal x = 0.0;

        const auto place = [&](const QStringView text, const qreal cellHeight) {
            for (const QChar c: text) {
                const auto glyph = glyphFor(c);
                if (!glyph)
                    continue;
                placed.append({c, *glyph, QRectF(x, height - cellHeight, glyph->width * cellHeight, cellHeight)});
                x += glyph->advance * cellHeight;
            }
        };

        place(value, height);
        if (!unit.isEmpty()) {
            x += kUnitGap * height;
            place(unit, height * kUnitScale);
        }
        return placed;
    }

    qreal layoutWidth(const QVector<PlacedGlyph> &layout) {
        qreal right = 0.0;
        for (const PlacedGlyph &p: layout)
            right = std::max(right, p.cell.right() + kSlant * p.cell.height());
        return right;
    }
}
