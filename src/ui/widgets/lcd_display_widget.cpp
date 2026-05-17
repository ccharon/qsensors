// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "lcd_display_widget.h"

#include <QPainter>
#include <QPalette>

namespace {
    constexpr int kDigitHeight = 30;
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

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    const QRect inner = rect();
    const QColor lcdBg = palette().color(QPalette::Base);
    painter.fillRect(inner, lcdBg);
    painter.setClipRect(inner);

    int xPos = kDisplayPaddingX;
    // Draw numeric value glyphs left-to-right from the sprite atlas.
    const QString digits = valueDigitsFor(m_reading);
    for (const QChar c: digits) {
        const auto glyph = LcdGlyphAtlas::GlyphId::bySymbol(QStringView(&c, 1));
        if (!glyph.has_value()) {
            continue;
        }
        const QRect src = glyph->sourceRect(alert, kDigitHeight);
        const QRect dst(xPos, 0, glyph->width(), kDigitHeight);
        painter.drawImage(dst, atlas.image(), src);
        xPos += glyph->width();
    }

    // Draw fixed-position unit marker to match xsensors layout.
    const QString unitSymbol = sensorUnitSymbol(m_reading.unit);
    const auto unitGlyph = LcdGlyphAtlas::GlyphId::bySymbol(QStringView(unitSymbol));
    if (!unitGlyph.has_value() || !unitGlyph->hasAnchor()) {
        return;
    }
    const QRect src = unitGlyph->sourceRect(alert, kDigitHeight);
    const QRect dst(unitGlyph->anchorX() + kDisplayPaddingX, 0, unitGlyph->width(), kDigitHeight);
    painter.drawImage(dst, atlas.image(), src);
}

QString LcdDisplayWidget::valueDigitsFor(const SensorReading &reading) {
    if (reading.unit == SensorUnit::Rpm) {
        return QStringLiteral("%1").arg(reading.value, 5, 'f', 0, QChar(' '));
    }
    if (reading.unit == SensorUnit::Celsius) {
        return QStringLiteral("%1").arg(reading.value, 6, 'f', 1, QChar(' '));
    }
    if (reading.unit == SensorUnit::Volt) {
        return QStringLiteral("%1").arg(reading.value, 6, 'f', 2, QChar(' '));
    }
    return QStringLiteral("%1").arg(reading.value, 6, 'f', 2, QChar(' '));
}

bool LcdDisplayWidget::isAlertState(const SensorReading &reading) {
    if (!reading.hasRange) {
        return false;
    }
    if (reading.unit == SensorUnit::Rpm) {
        return reading.hasMin && reading.value < reading.minValue;
    }
    if (reading.unit == SensorUnit::Celsius) {
        return reading.hasMax && reading.value > reading.maxValue;
    }
    if (reading.unit == SensorUnit::Volt) {
        return (reading.hasMin && reading.value < reading.minValue) || (
                   reading.hasMax && reading.value > reading.maxValue);
    }
    return false;
}
