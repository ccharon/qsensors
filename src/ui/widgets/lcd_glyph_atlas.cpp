// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "lcd_glyph_atlas.h"

#include <QColor>
#include <array>

namespace {
    using Key = LcdGlyphAtlas::GlyphId::Key;
    using GlyphSpec = LcdGlyphAtlas::GlyphSpec;

    const std::array<std::pair<Key, GlyphSpec>, 17> kGlyphs = {{
        {Key::Digit0, {QStringLiteral("0"), 0, 0, 0, 30, 18, -1}},
        {Key::Digit1, {QStringLiteral("1"), 18, 0, 18, 30, 18, -1}},
        {Key::Digit2, {QStringLiteral("2"), 36, 0, 36, 30, 18, -1}},
        {Key::Digit3, {QStringLiteral("3"), 54, 0, 54, 30, 18, -1}},
        {Key::Digit4, {QStringLiteral("4"), 72, 0, 72, 30, 18, -1}},
        {Key::Digit5, {QStringLiteral("5"), 90, 0, 90, 30, 18, -1}},
        {Key::Digit6, {QStringLiteral("6"), 108, 0, 108, 30, 18, -1}},
        {Key::Digit7, {QStringLiteral("7"), 126, 0, 126, 30, 18, -1}},
        {Key::Digit8, {QStringLiteral("8"), 144, 0, 144, 30, 18, -1}},
        {Key::Digit9, {QStringLiteral("9"), 162, 0, 162, 30, 18, -1}},
        {Key::Blank, {QStringLiteral(" "), 198, 0, 198, 30, 18, -1}},
        {Key::Minus, {QStringLiteral("-"), 180, 0, 180, 30, 18, -1}},
        {Key::Dot, {QStringLiteral("."), 171, 60, 171, 90, 6, -1}},
        {Key::UnitR, {QStringLiteral("RPM"), 0, 120, 0, 150, 57, 90}},
        {Key::UnitC, {QStringLiteral("°C"), 0, 60, 0, 90, 57, 96}},
        {Key::UnitF, {QStringLiteral("°F"), 57, 60, 57, 90, 57, 96}},
        {Key::UnitV, {QStringLiteral("V"), 114, 60, 114, 90, 57, 96}},
    }};

    const GlyphSpec *findSpec(const Key key) {
        for (const auto &[id, spec]: kGlyphs) {
            if (id == key) {
                return &spec;
            }
        }
        return nullptr;
    }
}

LcdGlyphAtlas::GlyphId::GlyphId(const Key key) : m_key(key) {
}

const LcdGlyphAtlas::GlyphSpec &LcdGlyphAtlas::GlyphId::spec() const {
    const GlyphSpec *found = findSpec(m_key);
    Q_ASSERT(found != nullptr);
    return *found;
}

QStringView LcdGlyphAtlas::GlyphId::symbol() const {
    return QStringView(spec().symbol);
}

int LcdGlyphAtlas::GlyphId::width() const {
    return spec().width;
}

bool LcdGlyphAtlas::GlyphId::hasAnchor() const {
    return spec().anchorX >= 0;
}

int LcdGlyphAtlas::GlyphId::anchorX() const {
    return spec().anchorX;
}

QRect LcdGlyphAtlas::GlyphId::sourceRect(const bool alert, const int glyphHeight) const {
    const GlyphSpec &s = spec();
    return alert
               ? QRect(s.alertX, s.alertY, s.width, glyphHeight)
               : QRect(s.normalX, s.normalY, s.width, glyphHeight);
}

std::optional<LcdGlyphAtlas::GlyphId> LcdGlyphAtlas::GlyphId::bySymbol(const QStringView symbol) {
    for (const auto &[id, spec]: kGlyphs) {
        if (QStringView(spec.symbol) == symbol) {
            return GlyphId(id);
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

