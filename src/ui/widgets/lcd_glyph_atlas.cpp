// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "lcd_glyph_atlas.h"

#include <QColor>
#include <array>

namespace {
    using GlyphSpec = LcdGlyphAtlas::GlyphSpec;

    const std::array<GlyphSpec, 17> kGlyphs = {{
        {QStringLiteral("0"),   0,   0,   0,  30, 18, -1},
        {QStringLiteral("1"),  18,   0,  18,  30, 18, -1},
        {QStringLiteral("2"),  36,   0,  36,  30, 18, -1},
        {QStringLiteral("3"),  54,   0,  54,  30, 18, -1},
        {QStringLiteral("4"),  72,   0,  72,  30, 18, -1},
        {QStringLiteral("5"),  90,   0,  90,  30, 18, -1},
        {QStringLiteral("6"), 108,   0, 108,  30, 18, -1},
        {QStringLiteral("7"), 126,   0, 126,  30, 18, -1},
        {QStringLiteral("8"), 144,   0, 144,  30, 18, -1},
        {QStringLiteral("9"), 162,   0, 162,  30, 18, -1},
        {QStringLiteral(" "), 198,   0, 198,  30, 18, -1},
        {QStringLiteral("-"), 180,   0, 180,  30, 18, -1},
        {QStringLiteral("."), 171,  60, 171,  90,  6, -1},
        {QStringLiteral("RPM"),  0, 120,   0, 150, 57, 90},
        {QStringLiteral("°C"),   0,  60,   0,  90, 57, 96},
        {QStringLiteral("°F"),  57,  60,  57,  90, 57, 96},
        {QStringLiteral("V"),  114,  60, 114,  90, 57, 96},
    }};
}

LcdGlyphAtlas::GlyphId::GlyphId(const GlyphSpec *spec) : m_spec(spec) {}

QStringView LcdGlyphAtlas::GlyphId::symbol() const {
    return QStringView(m_spec->symbol);
}

int LcdGlyphAtlas::GlyphId::width() const {
    return m_spec->width;
}

bool LcdGlyphAtlas::GlyphId::hasAnchor() const {
    return m_spec->anchorX >= 0;
}

int LcdGlyphAtlas::GlyphId::anchorX() const {
    return m_spec->anchorX;
}

QRect LcdGlyphAtlas::GlyphId::sourceRect(const bool alert, const int glyphHeight) const {
    return alert
               ? QRect(m_spec->alertX, m_spec->alertY, m_spec->width, glyphHeight)
               : QRect(m_spec->normalX, m_spec->normalY, m_spec->width, glyphHeight);
}

std::optional<LcdGlyphAtlas::GlyphId> LcdGlyphAtlas::GlyphId::bySymbol(const QStringView symbol) {
    for (const GlyphSpec &spec: kGlyphs) {
        if (QStringView(spec.symbol) == symbol) {
            return GlyphId(&spec);
        }
    }
    return std::nullopt;
}

const LcdGlyphAtlas &LcdGlyphAtlas::instance() {
    static const LcdGlyphAtlas atlas;
    return atlas;
}

LcdGlyphAtlas::LcdGlyphAtlas() : m_theme(loadThemeWithWhiteTransparency()) {
}

bool LcdGlyphAtlas::isValid() const {
    return !m_theme.isNull();
}

const QImage &LcdGlyphAtlas::image() const {
    return m_theme;
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

