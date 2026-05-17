// SPDX-License-Identifier: GPL-2.0-or-later

#include "lcd_glyph_atlas.h"

#include <QtTest/QtTest>

// Verifies glyph lookup metadata and normal/alert atlas source rectangle selection.
class GlyphModelTest final : public QObject {
    Q_OBJECT

private slots:
    void lookup_by_symbol_returns_expected_glyph();
    void source_rect_switches_between_normal_and_alert_coords();
};

void GlyphModelTest::lookup_by_symbol_returns_expected_glyph() {
    const auto celsius = LcdGlyphAtlas::GlyphId::bySymbol(QStringView{QStringLiteral("°C")});
    QVERIFY(celsius.has_value());
    QVERIFY(celsius->hasAnchor());
    QCOMPARE(celsius->anchorX(), 96);
    QCOMPARE(celsius->width(), 57);

    const auto digit = LcdGlyphAtlas::GlyphId::bySymbol(QStringView{QStringLiteral("7")});
    QVERIFY(digit.has_value());
    QVERIFY(!digit->hasAnchor());
    QCOMPARE(digit->width(), 18);
}

void GlyphModelTest::source_rect_switches_between_normal_and_alert_coords() {
    const auto dot = LcdGlyphAtlas::GlyphId::bySymbol(QStringView{QStringLiteral(".")});
    QVERIFY(dot.has_value());

    const QRect normal = dot->sourceRect(false, 30);
    const QRect alert = dot->sourceRect(true, 30);

    QCOMPARE(normal, QRect(171, 60, 6, 30));
    QCOMPARE(alert, QRect(171, 90, 6, 30));
}

QTEST_APPLESS_MAIN(GlyphModelTest)
#include "test_glyph_model.moc"
