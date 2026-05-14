// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "lcd_glyph_atlas.h"

#include <QColor>

const LcdGlyphAtlas &LcdGlyphAtlas::instance() {
    static const LcdGlyphAtlas atlas;
    return atlas;
}

LcdGlyphAtlas::LcdGlyphAtlas() : m_theme(loadThemeWithWhiteTransparency()) {}

bool LcdGlyphAtlas::isValid() const {
    return !m_theme.isNull();
}

QRect LcdGlyphAtlas::sourceRect(const Glyph &glyph, const int yOffset, const int glyphHeight) const {
    return {glyph.x, glyph.y + yOffset, glyph.w, glyphHeight};
}

const QImage &LcdGlyphAtlas::image() const {
    return m_theme;
}

bool LcdGlyphAtlas::glyphFor(const QChar c, Glyph &glyph) {
    if (c >= QChar('0') && c <= QChar('9')) {
        glyph = {18 * (c.toLatin1() - '0'), 0, 18};
        return true;
    }
    if (c == QChar(' ')) {
        glyph = {198, 0, 18};
        return true;
    }
    if (c == QChar('-')) {
        glyph = {180, 0, 18};
        return true;
    }
    if (c == QChar('.') || c == QChar(',')) {
        glyph = {171, 60, 6};
        return true;
    }
    if (c == QChar('R')) {
        glyph = {0, 120, 57};
        return true;
    }
    if (c == QChar('C')) {
        glyph = {0, 60, 57};
        return true;
    }
    if (c == QChar('F')) {
        glyph = {57, 60, 57};
        return true;
    }
    if (c == QChar('V')) {
        glyph = {114, 60, 57};
        return true;
    }
    return false;
}

QImage LcdGlyphAtlas::loadThemeWithWhiteTransparency() {
    QImage src(QStringLiteral(":/themes/xsensors-theme.png"));
    if (src.isNull()) {
        return {};
    }
    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (const QColor c = QColor::fromRgb(line[x]); c.red() >= 248 && c.green() >= 248 && c.blue() >= 248) {
                line[x] = qRgba(c.red(), c.green(), c.blue(), 0);
            }
        }
    }
    return img;
}
