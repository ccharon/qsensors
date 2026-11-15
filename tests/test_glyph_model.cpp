// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/widgets/lcd_segment_font.h"

#include <QtTest/QtTest>

using namespace LcdSegmentFont;

// Verifies segment glyph masks, segment geometry and value/unit layout of the vector LCD.
class GlyphModelTest final : public QObject {
    Q_OBJECT

private slots:
    void digits_use_seven_segment_masks();
    void digits_show_ghost_segments_units_do_not();
    void unknown_characters_are_rejected();
    void every_segment_has_geometry_inside_cell();
    void layout_places_unit_after_value_on_shared_baseline();
    void layout_skips_unsupported_characters();
    void fit_height_shrinks_only_when_too_wide();
};

void GlyphModelTest::digits_use_seven_segment_masks() {
    QCOMPARE(glyphFor(u'8')->lit, kSevenSegments);
    QCOMPARE(glyphFor(u'1')->lit, SegmentMask(B | C));
    QCOMPARE(glyphFor(u'-')->lit, SegmentMask(G1 | G2));
    QCOMPARE(glyphFor(u' ')->lit, SegmentMask(0));
    QCOMPARE(glyphFor(u'.')->lit, SegmentMask(DP));
    for (char c = '0'; c <= '9'; ++c) {
        const auto glyph = glyphFor(QChar::fromLatin1(c));
        QVERIFY(glyph.has_value());
        QVERIFY2((glyph->lit & ~kSevenSegments) == 0, "digits must not use diagonal segments");
    }
}

void GlyphModelTest::digits_show_ghost_segments_units_do_not() {
    QCOMPARE(glyphFor(u'4')->ghost, kSevenSegments);
    QCOMPARE(glyphFor(u' ')->ghost, kSevenSegments);
    QCOMPARE(glyphFor(u'W')->ghost, SegmentMask(0));
    QCOMPARE(glyphFor(QChar(0x00B0))->ghost, SegmentMask(0));
}

void GlyphModelTest::unknown_characters_are_rejected() {
    QVERIFY(!glyphFor(u'Z').has_value());
    QVERIFY(supports(u"12.34"));
    QVERIFY(supports(u"mW"));
    QVERIFY(!supports(u"kHz"));
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
        QVERIFY2(bounds.contains(box), qPrintable(QString::number(bit)));
    }
}

void GlyphModelTest::layout_places_unit_after_value_on_shared_baseline() {
    constexpr qreal height = 30.0;
    const auto layout = layoutText(u"  1.23", u"mW", height);
    QCOMPARE(layout.size(), 8);

    const QRectF lastDigit = layout.at(5).cell;
    const QRectF unitM = layout.at(6).cell;
    QCOMPARE(lastDigit.height(), height);
    QCOMPARE(unitM.height(), height * kUnitScale);
    QCOMPARE(unitM.bottom(), lastDigit.bottom());
    QVERIFY(unitM.left() > lastDigit.right());
    QVERIFY(layout.at(7).cell.left() > unitM.right());

    // RPM is the widest unit; it must still fit the default card-sized display.
    QVERIFY(layoutWidth(layoutText(u" 1534", u"RPM", height)) <= 150.0);
    QVERIFY(layoutWidth(layoutText(u"  1.23", u"V", height)) <= 150.0);
}

void GlyphModelTest::layout_skips_unsupported_characters() {
    const auto layout = layoutText(u"1?2", u"", 30.0);
    QCOMPARE(layout.size(), 2);
    QCOMPARE(layout.at(0).symbol, QChar(u'1'));
    QCOMPARE(layout.at(1).symbol, QChar(u'2'));
    QCOMPARE(layout.at(1).cell.left(), layout.at(0).glyph.advance * 30.0);
}

void GlyphModelTest::fit_height_shrinks_only_when_too_wide() {
    constexpr qreal height = 28.0;
    QCOMPARE(fitHeight(u"  1.23", u"V", height, 500.0), height);

    const qreal fitted = fitHeight(u"-12345.67", u"mW", height, 120.0);
    QVERIFY(fitted < height);
    QVERIFY(layoutWidth(layoutText(u"-12345.67", u"mW", fitted)) <= 120.0 + 1e-6);
}

QTEST_APPLESS_MAIN(GlyphModelTest)
#include "test_glyph_model.moc"
