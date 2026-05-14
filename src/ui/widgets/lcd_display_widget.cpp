// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "lcd_display_widget.h"

#include <QPainter>
#include <QPixmap>

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

    static const QPixmap theme(QStringLiteral(":/themes/xsensors-theme.png"));
    if (theme.isNull()) {
        return;
    }

    const bool alert = isAlertState(m_reading);
    const int yOffset = alert ? kRowOffsetAlert : 0;

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    const QRect inner = rect();
    painter.fillRect(inner, QColor(238, 243, 238));
    painter.setClipRect(inner);

    int xPos = kDisplayPaddingX;
    // Draw number glyphs left-to-right from the sprite atlas.
    const QString digits = valueDigitsFor(m_reading);
    for (const QChar c : digits) {
        Glyph g{};
        if (!glyphFor(c, g)) {
            continue;
        }
        const QRect src(g.x, g.y + yOffset, g.w, kDigitHeight);
        const QRect dst(xPos, 0, g.w, kDigitHeight);
        painter.drawPixmap(dst, theme, src);
        xPos += g.w;
    }

    // Draw fixed-position unit marker to match xsensors layout.
    const QChar unit = unitGlyphFor(m_reading);
    Glyph unitGlyph{};
    if (glyphFor(unit, unitGlyph) && unit != QChar(' ')) {
        int unitX = -1;
        if (unit == QChar('R')) {
            unitX = 90;
        } else if (unit == QChar('C') || unit == QChar('F') || unit == QChar('V')) {
            unitX = 96;
        }
        if (unitX >= 0) {
            const QRect src(unitGlyph.x, unitGlyph.y + yOffset, unitGlyph.w, kDigitHeight);
            const QRect dst(unitX + kDisplayPaddingX, 0, unitGlyph.w, kDigitHeight);
            painter.drawPixmap(dst, theme, src);
        }
    }
}

bool LcdDisplayWidget::glyphFor(const QChar c, Glyph &glyph) {
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
        return QChar('R');
    }
    if (reading.unit == QStringLiteral("°C")) {
        return QChar('C');
    }
    if (reading.unit == QStringLiteral("°F")) {
        return QChar('F');
    }
    if (reading.unit == QStringLiteral("V")) {
        return QChar('V');
    }
    return QChar(' ');
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
        return (reading.hasMin && reading.value < reading.minValue) || (reading.hasMax && reading.value > reading.maxValue);
    }
    return false;
}
