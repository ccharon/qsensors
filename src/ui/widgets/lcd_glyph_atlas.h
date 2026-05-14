// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QChar>
#include <QImage>
#include <QRect>

/** Sprite atlas loader and glyph lookup for xsensors-style LCD rendering. */
class LcdGlyphAtlas final {
public:
    struct Glyph {
        int x;
        int y;
        int w;
    };

    /** Returns process-wide atlas instance loaded from Qt resources. */
    static const LcdGlyphAtlas &instance();

    /** True when atlas image could be loaded and preprocessed. */
    [[nodiscard]] bool isValid() const;

    /** Returns source rect for one glyph on the requested atlas row offset. */
    [[nodiscard]] QRect sourceRect(const Glyph &glyph, int yOffset, int glyphHeight) const;

    /** Returns atlas glyph definition for one symbol. */
    static bool glyphFor(QChar c, Glyph &glyph);

    /** Atlas image with transparent background. */
    [[nodiscard]] const QImage &image() const;

private:
    LcdGlyphAtlas();
    static QImage loadThemeWithWhiteTransparency();

    QImage m_theme;
};
