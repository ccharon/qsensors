// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "lcd_display_widget.h"
#include "lcd_segment_font.h"
#include "sensor_format.h"
#include "sensors_policy.h"
#include "theme/app_theme.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPalette>
#include <algorithm>
#include <cmath>

namespace {
    constexpr int kDigitHeight = 28;
    constexpr int kDisplayHeight = kDigitHeight + AppTheme::kLcdBarGap + AppTheme::kLcdBarHeight
                                   + 2 * AppTheme::kLcdPaddingY;
}

LcdDisplayWidget::LcdDisplayWidget(const SensorReading &reading, QWidget *parent)
    : QWidget(parent), m_reading(reading) {
    setMinimumHeight(kDisplayHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void LcdDisplayWidget::setReading(const SensorReading &reading) {
    m_reading = reading;
    update();
}

QSize LcdDisplayWidget::sizeHint() const {
    return {150, kDisplayHeight};
}

void LcdDisplayWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    paintPanel(painter);
    painter.setClipRect(rect().adjusted(1, 1, -1, -1));
    painter.setPen(Qt::NoPen);

    // A shadow is invisible on a dark backplane; dark themes use a glow.
    const bool dark = hasDarkBase();
    const bool alert = SensorsPolicy::isAlertState(m_reading);
    const QColor lit(dark ? (alert ? AppTheme::kLcdAlertDarkRgb : AppTheme::kLcdNormalDarkRgb)
                          : (alert ? AppTheme::kLcdAlertRgb : AppTheme::kLcdNormalRgb));
    QColor ghost(dark ? AppTheme::kLcdNormalDarkRgb : AppTheme::kLcdNormalRgb);
    ghost.setAlpha(dark ? AppTheme::kLcdGhostAlphaDark : AppTheme::kLcdGhostAlpha);
    const QColor shadow(0, 0, 0, AppTheme::kLcdSegmentShadowAlpha);

    // Everything scales with the widget; long readings shrink to fit the width.
    const QString value = SensorFormat::valueDigits(m_reading.unit, m_reading.value);
    const QString unit = sensorUnitSymbol(m_reading.unit);
    // Digits fill the area above the range bar graph that runs along the bottom.
    const qreal barTop = height() - AppTheme::kLcdPaddingY - AppTheme::kLcdBarHeight;
    const qreal digitArea = barTop - AppTheme::kLcdBarGap - AppTheme::kLcdPaddingY;
    const qreal maxHeight = std::max<qreal>(digitArea, 1.0);
    const qreal maxWidth = std::max<qreal>(width() - 2 * AppTheme::kLcdPaddingX, 1.0);
    const qreal digitHeight = LcdSegmentFont::fitHeight(value, unit, maxHeight, maxWidth);
    const auto layout = LcdSegmentFont::layoutText(value, unit, digitHeight);
    const qreal baselineOffset = AppTheme::kLcdPaddingY + maxHeight - digitHeight;
    const QVector<QPolygonF> barSegments = barGraphSegments(QRectF(AppTheme::kLcdPaddingX, barTop,
                                                                   maxWidth, AppTheme::kLcdBarHeight));
    const int barLit = litBarSegments(m_reading, static_cast<int>(barSegments.size()));
    // Lit segments float above the backplane and cast a faint offset shadow.
    const qreal shadowOffset = std::max<qreal>(0.7, digitHeight * 0.035);

    enum class Pass { Shadow, Glow, Face };
    const auto drawSegments = [&](const Pass pass, const QPointF offset = {}, const qreal glowWidth = 0.0) {
        if (pass == Pass::Glow) {
            QColor halo = lit;
            halo.setAlpha(AppTheme::kLcdGlowAlpha);
            painter.setPen(QPen(halo, glowWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(halo);
        } else {
            painter.setPen(Qt::NoPen);
        }
        for (const LcdSegmentFont::PlacedGlyph &placed: layout) {
            const QRectF &cell = placed.cell;
            // Shear around the baseline so all cells lean like the classic LCD digits.
            painter.setTransform(QTransform(1, 0, -LcdSegmentFont::kSlant, 1,
                                            AppTheme::kLcdPaddingX + cell.left()
                                            + LcdSegmentFont::kSlant * cell.height() + offset.x(),
                                            baselineOffset + cell.top() + offset.y()));
            const LcdSegmentFont::SegmentMask drawn =
                    pass == Pass::Face ? (placed.glyph.lit | placed.glyph.ghost) : placed.glyph.lit;
            for (LcdSegmentFont::SegmentMask bit = 1; bit <= LcdSegmentFont::kLastSegment; bit <<= 1) {
                if (!(drawn & bit))
                    continue;
                if (pass == Pass::Shadow)
                    painter.setBrush(shadow);
                else if (pass == Pass::Face)
                    painter.setBrush((placed.glyph.lit & bit) ? lit : ghost);
                painter.drawPolygon(LcdSegmentFont::segmentPolygon(static_cast<LcdSegmentFont::Segment>(bit),
                                                                   cell.size()));
            }
        }
        painter.resetTransform();
        painter.translate(offset);
        for (int i = 0; i < barSegments.size(); ++i) {
            const bool on = i < barLit;
            if (!on && pass != Pass::Face)
                continue;
            if (pass == Pass::Shadow)
                painter.setBrush(shadow);
            else if (pass == Pass::Face)
                painter.setBrush(on ? lit : ghost);
            painter.drawPolygon(barSegments.at(i));
        }
        painter.resetTransform();
    };
    if (dark) {
        // Two soft halo layers approximate a blur without offscreen rendering.
        drawSegments(Pass::Glow, {}, shadowOffset * 3.0);
        drawSegments(Pass::Glow, {}, shadowOffset * 1.5);
    } else {
        drawSegments(Pass::Shadow, {shadowOffset, shadowOffset});
    }
    drawSegments(Pass::Face);
}

QVector<QPolygonF> LcdDisplayWidget::barGraphSegments(const QRectF &area) {
    // Slanted cells like the digits; the segment count follows the available width.
    const qreal slant = LcdSegmentFont::kSlant * area.height();
    const qreal usable = area.width() - slant;
    const qreal pitch = AppTheme::kLcdBarSegmentWidth + AppTheme::kLcdBarSegmentSpacing;
    const int count = std::max(1, static_cast<int>((usable + AppTheme::kLcdBarSegmentSpacing) / pitch));
    const qreal width = (usable - (count - 1) * AppTheme::kLcdBarSegmentSpacing) / count;

    QVector<QPolygonF> segments;
    segments.reserve(count);
    for (int i = 0; i < count; ++i) {
        const qreal x = area.left() + i * (width + AppTheme::kLcdBarSegmentSpacing);
        segments.append(QPolygonF{{
            {x + slant, area.top()}, {x + slant + width, area.top()},
            {x + width, area.bottom()}, {x, area.bottom()},
        }});
    }
    return segments;
}

int LcdDisplayWidget::litBarSegments(const SensorReading &reading, const int segmentCount) {
    const auto fraction = SensorsPolicy::rangeFraction(reading);
    if (!fraction || segmentCount <= 0) {
        return 0;
    }
    return static_cast<int>(std::lround(*fraction * segmentCount));
}

bool LcdDisplayWidget::hasDarkBase() const {
    return palette().color(QPalette::Base).lightnessF() < 0.5;
}

void LcdDisplayWidget::paintPanel(QPainter &painter) const {
    const auto blend = [](const QColor &a, const QColor &b, const qreal t) {
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                                a.greenF() + (b.greenF() - a.greenF()) * t,
                                a.blueF() + (b.blueF() - a.blueF()) * t);
    };

    // Backplane: palette base tinted towards the LCD green, so light and dark themes both work.
    const QColor base = palette().color(QPalette::Base);
    const QColor tint = blend(base, QColor(AppTheme::kLcdNormalRgb), AppTheme::kLcdTintStrength);
    const bool darkBase = hasDarkBase();

    const QRectF panel = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QLinearGradient gradient(panel.topLeft(), panel.bottomLeft());
    gradient.setColorAt(0.0, darkBase ? tint.darker(115) : tint.darker(106));
    gradient.setColorAt(0.35, tint);
    gradient.setColorAt(1.0, darkBase ? tint.lighter(112) : tint.lighter(103));

    painter.setPen(QPen(blend(tint, Qt::black, darkBase ? 0.45 : 0.28), 1.0));
    painter.setBrush(gradient);
    painter.drawRoundedRect(panel, AppTheme::kLcdCornerRadius, AppTheme::kLcdCornerRadius);

    // Recessed glass: shadow under the top bezel, faint reflection along the bottom edge.
    const qreal r = AppTheme::kLcdCornerRadius;
    painter.setPen(QPen(QColor(0, 0, 0, darkBase ? 70 : 34), 1.0));
    painter.drawLine(QPointF(panel.left() + r, panel.top() + 1.0), QPointF(panel.right() - r, panel.top() + 1.0));
    painter.setPen(QPen(QColor(255, 255, 255, darkBase ? 18 : 90), 1.0));
    painter.drawLine(QPointF(panel.left() + r, panel.bottom() - 1.0), QPointF(panel.right() - r, panel.bottom() - 1.0));
}

