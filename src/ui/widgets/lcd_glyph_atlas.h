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
        enum class Key {
            Digit0,
            Digit1,
            Digit2,
            Digit3,
            Digit4,
            Digit5,
            Digit6,
            Digit7,
            Digit8,
            Digit9,
            Blank,
            Minus,
            Dot,
            UnitR,
            UnitC,
            UnitF,
            UnitV
        };

        explicit GlyphId(Key key);

        [[nodiscard]] const GlyphSpec &spec() const;
        [[nodiscard]] QStringView symbol() const;
        [[nodiscard]] int width() const;
        [[nodiscard]] bool hasAnchor() const;
        [[nodiscard]] int anchorX() const;
        [[nodiscard]] QRect sourceRect(bool alert, int glyphHeight) const;

        [[nodiscard]] static std::optional<GlyphId> bySymbol(QStringView symbol);

    private:
        Key m_key;
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
