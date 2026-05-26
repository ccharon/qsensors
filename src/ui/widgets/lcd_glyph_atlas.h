// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QImage>
#include <QChar>
#include <QRect>
#include <QString>
#include <optional>

/** Sprite atlas loader and glyph lookup for xsensors-style LCD rendering. */
class LcdGlyphAtlas final {
public:
    struct GlyphSpec final {
        QString symbol;
        int normalX;
        int normalY;
        int alertX;
        int alertY;
        int width;
        int anchorX; // -1 means "no fixed anchor"
    };

    class GlyphId final {
    public:
        [[nodiscard]] QStringView symbol() const;
        [[nodiscard]] int width() const;
        [[nodiscard]] bool hasAnchor() const;
        [[nodiscard]] int anchorX() const;
        [[nodiscard]] QRect sourceRect(bool alert, int glyphHeight) const;

        [[nodiscard]] static std::optional<GlyphId> bySymbol(QStringView symbol);

    private:
        explicit GlyphId(const GlyphSpec *spec);
        const GlyphSpec *m_spec;
    };

    /** Returns process-wide atlas instance loaded from Qt resources. */
    [[nodiscard]] static const LcdGlyphAtlas &instance();

    /** True when atlas image could be loaded and preprocessed. */
    [[nodiscard]] bool isValid() const;

    /** Atlas image with transparent background. */
    [[nodiscard]] const QImage &image() const;

private:
    LcdGlyphAtlas();

    static QImage loadThemeWithWhiteTransparency();

    QImage m_theme;
};
