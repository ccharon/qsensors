// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/widgets/lcd_segment_font.h"
#include "ui/widgets/lcd_symbol_font.h"
#include "sensors/sensor_format.h"

#include <QtTest/QtTest>

using namespace LcdSegmentFont;

// Verifies segment glyph masks, segment geometry, digit cell layout and the annunciator symbol font of the LCD.
class GlyphModelTest final : public QObject {
    Q_OBJECT

private slots:
    void digits_use_seven_segment_masks();
    void digits_show_ghost_segments();
    void unknown_characters_are_rejected();
    void every_segment_has_geometry_inside_cell();
    void layout_cells_right_aligns_the_value();
    void layout_cells_attach_decimal_point_to_previous_cell();
    void layout_cells_grow_for_long_values();
    void cells_width_and_fit_agree();
    void layout_skips_unsupported_characters();
    void symbol_font_draws_every_unit_and_state();
    void symbol_font_is_deterministic_and_inside_its_box();
    void symbol_font_tells_R_from_A();
};

void GlyphModelTest::digits_use_seven_segment_masks() {
    QCOMPARE(glyphFor(u'8')->lit, kSevenSegments);
    QCOMPARE(glyphFor(u'1')->lit, SegmentMask(B | C));
    QCOMPARE(glyphFor(u'-')->lit, SegmentMask(G1 | G2));
    QCOMPARE(glyphFor(u' ')->lit, SegmentMask(0));
    QVERIFY(!glyphFor(u'.').has_value());
    QVERIFY(supports(u"-12.10"));
    for (char c = '0'; c <= '9'; ++c) {
        const auto glyph = glyphFor(QChar::fromLatin1(c));
        QVERIFY(glyph.has_value());
        QVERIFY2((glyph->lit & ~kSevenSegments) == 0, "digits must not use diagonal segments");
    }
}

void GlyphModelTest::digits_show_ghost_segments() {
    QCOMPARE(glyphFor(u'4')->ghost, kSevenSegments);
    QCOMPARE(glyphFor(u' ')->ghost, kSevenSegments);
    QCOMPARE(glyphFor(u'O')->ghost, kSevenSegments);
}

void GlyphModelTest::unknown_characters_are_rejected() {
    QVERIFY(!glyphFor(u'Z').has_value());
    // Unit letters are no segment characters any more.
    QVERIFY(!glyphFor(u'V').has_value());
    QVERIFY(!glyphFor(u'm').has_value());
    QVERIFY(supports(u"12.34"));
    QVERIFY(supports(u"   OL"));
    QVERIFY(!supports(u"mW"));
    QVERIFY(!supports(u"Qx"));
}

void GlyphModelTest::every_segment_has_geometry_inside_cell() {
    const QSizeF cell(15.0, 30.0);
    // Tolerance covers floating point noise from path clipping.
    const QRectF bounds = QRectF(QPointF(0, 0), cell).adjusted(-1e-3, -1e-3, 1e-3, 1e-3);
    for (SegmentMask bit = 1; bit <= kLastSegment; bit <<= 1) {
        const QPolygonF polygon = segmentPolygon(static_cast<Segment>(bit), cell);
        QVERIFY2(polygon.size() >= 4, qPrintable(QString::number(bit)));
        const QRectF box = polygon.boundingRect();
        QVERIFY2(box.width() > 0 && box.height() > 0, qPrintable(QString::number(bit)));
        if (bit == DP) {
            // The decimal point sits in the gap to the next cell, on the baseline.
            QVERIFY(box.left() > cell.width());
            QVERIFY(box.right() < kDigitAdvance * cell.height());
            QCOMPARE(box.bottom(), cell.height());
            continue;
        }
        QVERIFY2(bounds.contains(box), qPrintable(QString::number(bit)));
    }
}

void GlyphModelTest::layout_cells_right_aligns_the_value() {
    constexpr qreal height = 30.0;
    const auto cells = layoutCells(u"880", 7, 5, height);
    QCOMPARE(cells.size(), 7);
    QCOMPARE(cells.at(6).symbol, QChar(u'0'));
    QCOMPARE(cells.at(4).symbol, QChar(u'8'));
    QCOMPARE(cells.at(3).glyph.lit, SegmentMask(0));
    // Unused cells are ghost digits.
    QCOMPARE(cells.at(0).glyph.ghost & kSevenSegments, kSevenSegments);
    // Fixed pitch, so equal cell counts put every digit at the same place.
    for (int i = 0; i < cells.size(); ++i) {
        QCOMPARE(cells.at(i).cell.left(), i * kDigitAdvance * height);
        QCOMPARE(cells.at(i).cell.width(), kDigitWidth * height);
        QCOMPARE(cells.at(i).cell.height(), height);
    }
    // The last digit ends in the same cell for any value.
    QCOMPARE(layoutCells(u"-12.35", 7, 5, height).last().cell, cells.last().cell);
}

void GlyphModelTest::layout_cells_attach_decimal_point_to_previous_cell() {
    const auto cells = layoutCells(u"1.052", 6, 5, 30.0);
    QCOMPARE(cells.size(), 6);
    QCOMPARE(cells.at(2).symbol, QChar(u'1'));
    QVERIFY(cells.at(2).glyph.lit & DP);
    QVERIFY(!(cells.at(3).glyph.lit & DP));
    // Faint points between the value cells only; none after the last cell and none in extra ghost cells.
    QVERIFY(cells.at(3).glyph.ghost & DP);
    QVERIFY(cells.at(1).glyph.ghost & DP);
    QVERIFY(!(cells.at(0).glyph.ghost & DP));
    QVERIFY(!(cells.last().glyph.ghost & DP));
    // Dashes of a missing value keep the decimal point of their scale.
    const auto missing = layoutCells(u"----.-", 5, 5, 30.0);
    QCOMPARE(missing.size(), 5);
    QVERIFY(missing.at(3).glyph.lit & DP);
    QCOMPARE(missing.at(4).glyph.lit, SegmentMask(G1 | G2));
}

void GlyphModelTest::layout_cells_grow_for_long_values() {
    const auto cells = layoutCells(u"123456", 5, 5, 30.0);
    QCOMPARE(cells.size(), 6);
    QCOMPARE(cells.first().symbol, QChar(u'1'));
}

void GlyphModelTest::cells_width_and_fit_agree() {
    constexpr qreal height = 28.0;
    QCOMPARE(cellsWidth(0, height), 0.0);
    QCOMPARE(cellsWidth(1, height), (kDigitWidth + kSlant) * height);
    for (int count = 1; count <= 8; ++count) {
        QCOMPARE(cellsThatFit(cellsWidth(count, height), height), count);
        QCOMPARE(cellsThatFit(cellsWidth(count, height) - 0.1, height), count - 1);
    }
}

void GlyphModelTest::layout_skips_unsupported_characters() {
    const auto layout = layoutCells(u"1?2", 3, 3, 30.0);
    QCOMPARE(layout.size(), 3);
    QCOMPARE(layout.at(1).symbol, QChar(u'1'));
    QCOMPARE(layout.at(2).symbol, QChar(u'2'));
    QCOMPARE(layout.at(2).cell.left() - layout.at(1).cell.left(), kDigitAdvance * 30.0);
}

void GlyphModelTest::symbol_font_draws_every_unit_and_state() {
    QStringList symbols = SensorFormat::allUnitSymbols();
    symbols << QStringLiteral("HI") << QStringLiteral("LO");
    for (const QString &symbol: symbols) {
        QVERIFY2(LcdSymbolFont::supports(symbol), qPrintable(symbol));
        const QPainterPath path = LcdSymbolFont::path(symbol, 14.0);
        QVERIFY2(!path.isEmpty(), qPrintable(symbol));
        // Every letter contributes ink: wider symbols cover more.
        QVERIFY2(LcdSymbolFont::width(symbol, 14.0) > 0.0, qPrintable(symbol));
    }
    QVERIFY(!LcdSymbolFont::supports(u"x"));
    QVERIFY(LcdSymbolFont::path(u"", 14.0).isEmpty());
    // Width is linear in the height and grows with each letter.
    QCOMPARE(LcdSymbolFont::width(u"mV", 28.0), 2.0 * LcdSymbolFont::width(u"mV", 14.0));
    QVERIFY(LcdSymbolFont::width(u"mV", 14.0) > LcdSymbolFont::width(u"V", 14.0));
}

void GlyphModelTest::symbol_font_is_deterministic_and_inside_its_box() {
    for (const QString &symbol: SensorFormat::allUnitSymbols()) {
        constexpr qreal height = 20.0;
        const QPainterPath path = LcdSymbolFont::path(symbol, height);
        QCOMPARE(path, LcdSymbolFont::path(symbol, height));
        const QRectF box = path.boundingRect();
        const qreal tolerance = 1e-6;
        QVERIFY2(box.top() >= -tolerance && box.bottom() <= height + tolerance, qPrintable(symbol));
        QVERIFY2(box.left() >= -tolerance, qPrintable(symbol));
        QVERIFY2(box.right() <= LcdSymbolFont::width(symbol, height) + tolerance, qPrintable(symbol));
        // Bold letters reach the baseline (the degree sign alone does not).
        QVERIFY2(box.bottom() > height * 0.95, qPrintable(symbol));
    }
}

void GlyphModelTest::symbol_font_tells_R_from_A() {
    // The 14-segment R and A differed by one diagonal; the symbol shapes differ widely.
    constexpr qreal height = 20.0;
    const QPainterPath r = LcdSymbolFont::path(u"R", height);
    const QPainterPath a = LcdSymbolFont::path(u"A", height);
    // R has a full-height stem on the left, A slants away from the left edge.
    QVERIFY(r.contains(QPointF(0.05 * height, 0.30 * height)));
    QVERIFY(!a.contains(QPointF(0.05 * height, 0.30 * height)));
    // R closes its bowl with a bar at mid height where A is open between its legs.
    QVERIFY(r.contains(QPointF(0.30 * height, 0.52 * height)));
    QVERIFY(!a.contains(QPointF(0.30 * height, 0.52 * height)));
}

QTEST_APPLESS_MAIN(GlyphModelTest)
#include "test_glyph_model.moc"
