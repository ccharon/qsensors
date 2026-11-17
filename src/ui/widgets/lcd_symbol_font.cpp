// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/widgets/lcd_symbol_font.h"

#include <QHash>
#include <QLineF>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QTransform>
#include <QVector>
#include <algorithm>
#include <optional>

namespace LcdSymbolFont {
    namespace {
        // Glyph coordinates: cap height 1, y down, baseline at y = 1, lower case x-height
        // from y = kXTop. Ends meant to be flat on the glyph box run past it by kOver and are
        // cut by the clip.
        constexpr qreal kHalf = kStrokeWeight / 2.0;
        constexpr qreal kOver = 0.3;
        constexpr qreal kXTop = 0.30;
        // Path geometry is built once at this cap height and scaled.
        constexpr qreal kBuildHeight = 100.0;

        struct Stroke {
            QPolygonF points;
            qreal radius = 0.0; // corner rounding
            bool closed = false;
            qreal weight = kStrokeWeight;
        };

        struct Glyph {
            qreal width = 0.0;
            QVector<Stroke> strokes;
        };

        Stroke line(const QPolygonF &points, const qreal radius = 0.0) {
            return {points, radius, false, kStrokeWeight};
        }

        Stroke ring(const QRectF &box, const qreal radius, const qreal weight) {
            return {QPolygonF{box.topLeft(), box.topRight(), box.bottomRight(), box.bottomLeft()}, radius, true, weight};
        }

        const QHash<QChar, Glyph> &glyphTable() {
            constexpr qreal c = kHalf;
            constexpr qreal o = kOver;
            static const QHash<QChar, Glyph> table = {
                {u'A', {0.62, {line({{0.04, 1 + o}, {0.31, 0.0}, {0.58, 1 + o}}),
                               line({{0.15, 0.68}, {0.47, 0.68}})}}},
                {u'C', {0.56, {line({{0.56, c}, {c, c}, {c, 1 - c}, {0.56, 1 - c}}, 0.22)}}},
                {u'F', {0.50, {line({{0.5 + o, c}, {c, c}, {c, 1 + o}}),
                               line({{c, 0.50}, {0.44, 0.50}})}}},
                {u'G', {0.60, {line({{0.58, c}, {c, c}, {c, 1 - c}, {0.6 - c, 1 - c}, {0.6 - c, 0.52}, {0.30, 0.52}}, 0.20)}}},
                {u'H', {0.60, {line({{c, -o}, {c, 1 + o}}), line({{0.6 - c, -o}, {0.6 - c, 1 + o}}),
                               line({{c, 0.50}, {0.6 - c, 0.50}})}}},
                {u'I', {0.20, {line({{0.10, -o}, {0.10, 1 + o}})}}},
                {u'J', {0.50, {line({{0.5 - c, -o}, {0.5 - c, 1 - c}, {c, 1 - c}, {c, 0.62}}, 0.20)}}},
                {u'L', {0.50, {line({{c, -o}, {c, 1 - c}, {0.5 + o, 1 - c}})}}},
                {u'M', {0.76, {line({{c, 1 + o}, {c, 0.0}, {0.38, 0.62}, {0.76 - c, 0.0}, {0.76 - c, 1 + o}})}}},
                {u'O', {0.60, {ring({c, c, 0.6 - 2 * c, 1 - 2 * c}, 0.22, kStrokeWeight)}}},
                {u'P', {0.58, {line({{c, 1 + o}, {c, c}, {0.58 - c, c}, {0.58 - c, 0.55}, {c, 0.55}}, 0.18)}}},
                {u'R', {0.60, {line({{c, 1 + o}, {c, c}, {0.6 - c, c}, {0.6 - c, 0.52}, {c, 0.52}}, 0.18),
                               line({{0.30, 0.52}, {0.62, 1 + o}})}}},
                {u'V', {0.60, {line({{0.04, -o}, {0.30, 1.0}, {0.56, -o}})}}},
                {u'W', {0.90, {line({{0.03, -o}, {0.24, 1.0}, {0.45, 0.20}, {0.66, 1.0}, {0.87, -o}})}}},
                {u'k', {0.54, {line({{c, -o}, {c, 1 + o}}), line({{0.56, kXTop}, {c, 0.74}}),
                               line({{0.26, 0.60}, {0.58, 1 + o}})}}},
                {u'm', {0.82, {line({{c, 1 + o}, {c, kXTop + c}, {0.82 - c, kXTop + c}, {0.82 - c, 1 + o}}, 0.16),
                               line({{0.41, kXTop + c}, {0.41, 1 + o}})}}},
                {u'z', {0.50, {line({{0.02, kXTop + c}, {0.5 - c, kXTop + c}, {c, 1 - c}, {0.5 + o, 1 - c}})}}},
                // Degree sign: a small ring at cap height, lighter than the letters.
                {QChar(0x00B0), {0.34, {ring({0.06, 0.06, 0.22, 0.24}, 0.10, 0.12)}}},
                {u'%', {0.80, {ring({0.07, 0.06, 0.22, 0.30}, 0.09, 0.12), ring({0.51, 0.64, 0.22, 0.30}, 0.09, 0.12),
                               line({{0.66, -o}, {0.14, 1 + o}})}}},
            };
            return table;
        }

        std::optional<Glyph> glyphFor(const QChar c) {
            const auto &table = glyphTable();
            const auto it = table.constFind(c);
            return it != table.cend() ? std::optional(*it) : std::nullopt;
        }

        /** Center line of @p stroke at @p scale with rounded corners. */
        QPainterPath centerLine(const Stroke &stroke, const qreal scale) {
            QPolygonF points;
            for (const QPointF &p: stroke.points)
                points << p * scale;
            const int n = static_cast<int>(points.size());
            QPainterPath path;
            if (n < 2)
                return path;

            const auto corner = [&](const int i, const bool first) {
                const QPointF p = points.at((i + n) % n);
                const QPointF prev = points.at((i - 1 + n) % n);
                const QPointF next = points.at((i + 1) % n);
                const qreal r = std::min({stroke.radius * scale, QLineF(p, prev).length() / 2.0,
                                          QLineF(p, next).length() / 2.0});
                const QPointF in = p + (prev - p) * (r / QLineF(p, prev).length());
                const QPointF out = p + (next - p) * (r / QLineF(p, next).length());
                if (first)
                    path.moveTo(in);
                else
                    path.lineTo(in);
                path.quadTo(p, out);
            };

            if (stroke.closed) {
                corner(0, true);
                for (int i = 1; i < n; ++i)
                    corner(i, false);
                path.closeSubpath();
            } else {
                path.moveTo(points.first());
                for (int i = 1; i < n - 1; ++i)
                    corner(i, false);
                path.lineTo(points.last());
            }
            return path;
        }

        /** Outline of @p glyph at kBuildHeight, clipped to its box. */
        QPainterPath outline(const Glyph &glyph) {
            QPainterPath shape;
            shape.setFillRule(Qt::WindingFill);
            for (const Stroke &stroke: glyph.strokes) {
                QPainterPathStroker stroker;
                stroker.setWidth(stroke.weight * kBuildHeight);
                stroker.setJoinStyle(Qt::MiterJoin);
                stroker.setMiterLimit(4.0);
                stroker.setCapStyle(Qt::FlatCap);
                shape = shape.united(stroker.createStroke(centerLine(stroke, kBuildHeight)));
            }
            QPainterPath box;
            box.addRect(0.0, 0.0, glyph.width * kBuildHeight, kBuildHeight);
            return shape.intersected(box).simplified();
        }

        qreal spacingBetween(const QChar left) {
            // The degree sign hugs its letter, like "°C" on a real display.
            return left == QChar(0x00B0) ? kLetterSpacing * 0.4 : kLetterSpacing;
        }

        /** Outline of @p text at kBuildHeight; cached, there are only a few symbols. */
        const QPainterPath &textOutline(const QStringView text) {
            static QHash<QString, QPainterPath> cache;
            const QString key = text.toString();
            if (const auto it = cache.constFind(key); it != cache.cend())
                return *it;
            QPainterPath path;
            qreal x = 0.0;
            std::optional<QChar> previous;
            for (const QChar c: text) {
                const auto glyph = glyphFor(c);
                if (!glyph)
                    continue;
                if (previous)
                    x += spacingBetween(*previous) * kBuildHeight;
                path.addPath(outline(*glyph).translated(x, 0.0));
                x += glyph->width * kBuildHeight;
                previous = c;
            }
            return *cache.insert(key, path);
        }
    }

    bool supports(const QStringView text) {
        return std::all_of(text.begin(), text.end(), [](const QChar c) { return glyphFor(c).has_value(); });
    }

    QPainterPath path(const QStringView text, const qreal height) {
        const qreal scale = height / kBuildHeight;
        return QTransform::fromScale(scale, scale).map(textOutline(text));
    }

    qreal width(const QStringView text, const qreal height) {
        qreal total = 0.0;
        std::optional<QChar> previous;
        for (const QChar c: text) {
            const auto glyph = glyphFor(c);
            if (!glyph)
                continue;
            if (previous)
                total += spacingBetween(*previous);
            total += glyph->width;
            previous = c;
        }
        return total * height;
    }
}
