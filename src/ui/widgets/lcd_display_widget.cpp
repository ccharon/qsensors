// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "lcd_display_widget.h"

#include <QPainter>
#include <QPalette>

namespace {
    constexpr int kDigitHeight = 30;
    constexpr int kRowOffsetAlert = 30;
    constexpr int kDisplayPaddingX = 0;
}

LcdDisplayWidget::LcdDisplayWidget(const SensorReading &reading, QWidget *parent)
    : QWidget(parent), m_reading(reading) {
    setMinimumHeight(kDigitHeight + 2);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void LcdDisplayWidget::setReading(const SensorReading &reading) {
    m_reading = reading;
    update();
}

QSize LcdDisplayWidget::sizeHint() const {
    return {150, kDigitHeight + 2};
}

void LcdDisplayWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    const LcdGlyphAtlas &atlas = LcdGlyphAtlas::instance();
    if (!atlas.isValid()) {
        return;
    }

    const bool alert = isAlertState(m_reading);
    const int yOffset = alert ? kRowOffsetAlert : 0;

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    const QRect inner = rect();
    const QColor panelBg = palette().color(QPalette::Window);
    const QColor lcdBg = panelBg.lightness() > 140
                             ? QColor(
                                 AppTheme::kLcdLightThemeBrightness,
                                 AppTheme::kLcdLightThemeBrightness,
                                 AppTheme::kLcdLightThemeBrightness
                             )
                             : QColor(
                                 AppTheme::kLcdDarkThemeBrightness,
                                 AppTheme::kLcdDarkThemeBrightness,
                                 AppTheme::kLcdDarkThemeBrightness
                             );
    painter.fillRect(inner, lcdBg);
    painter.setClipRect(inner);

    int xPos = kDisplayPaddingX;
    // Draw number glyphs left-to-right from the sprite atlas.
    const QString digits = valueDigitsFor(m_reading);
    for (const QChar c: digits) {
        LcdGlyphAtlas::Glyph g{};
        if (!LcdGlyphAtlas::glyphFor(c, g)) {
            continue;
        }
        const QRect src = atlas.sourceRect(g, yOffset, kDigitHeight);
        const QRect dst(xPos, 0, g.w, kDigitHeight);
        painter.drawImage(dst, atlas.image(), src);
        xPos += g.w;
    }

    // Draw fixed-position unit marker to match xsensors layout.
    const QChar unit = unitGlyphFor(m_reading);
    LcdGlyphAtlas::Glyph unitGlyph{};
    if (LcdGlyphAtlas::glyphFor(unit, unitGlyph) && unit != QChar(' ')) {
        int unitX = -1;
        if (unit == QChar('R')) {
            unitX = 90;
        } else if (unit == QChar('C') || unit == QChar('F') || unit == QChar('V')) {
            unitX = 96;
        }
        if (unitX >= 0) {
            const QRect src = atlas.sourceRect(unitGlyph, yOffset, kDigitHeight);
            const QRect dst(unitX + kDisplayPaddingX, 0, unitGlyph.w, kDigitHeight);
            painter.drawImage(dst, atlas.image(), src);
        }
    }
}

QString LcdDisplayWidget::valueDigitsFor(const SensorReading &reading) {
    if (reading.unit == QStringLiteral("RPM")) {
        return QStringLiteral("%1").arg(reading.value, 5, 'f', 0, QChar(' '));
    }
    if (reading.unit == QStringLiteral("°C")) {
        return QStringLiteral("%1").arg(reading.value, 6, 'f', 1, QChar(' '));
    }
    if (reading.unit == QStringLiteral("V")) {
        return QStringLiteral("%1").arg(reading.value, 6, 'f', 2, QChar(' '));
    }
    return QStringLiteral("%1").arg(reading.value, 6, 'f', 2, QChar(' '));
}

QChar LcdDisplayWidget::unitGlyphFor(const SensorReading &reading) {
    if (reading.unit == QStringLiteral("RPM")) {
        return {'R'};
    }
    if (reading.unit == QStringLiteral("°C")) {
        return {'C'};
    }
    if (reading.unit == QStringLiteral("°F")) {
        return {'F'};
    }
    if (reading.unit == QStringLiteral("V")) {
        return {'V'};
    }
    return {' '};
}

bool LcdDisplayWidget::isAlertState(const SensorReading &reading) {
    if (!reading.hasRange) {
        return false;
    }
    if (reading.unit == QStringLiteral("RPM")) {
        return reading.hasMin && reading.value < reading.minValue;
    }
    if (reading.unit == QStringLiteral("°C")) {
        return reading.hasMax && reading.value > reading.maxValue;
    }
    if (reading.unit == QStringLiteral("V")) {
        return (reading.hasMin && reading.value < reading.minValue) || (
                   reading.hasMax && reading.value > reading.maxValue);
    }
    return false;
}
