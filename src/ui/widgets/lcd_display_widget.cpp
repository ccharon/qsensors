// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/widgets/lcd_display_widget.h"
#include "sensors/sensor_format.h"
#include "sensors/sensors_policy.h"
#include "ui/theme/app_theme.h"
#include "ui/widgets/lcd_symbol_font.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <algorithm>
#include <cmath>

namespace {
    constexpr int kDigitHeight = 28;
    constexpr int kDisplayHeight = kDigitHeight + AppTheme::kLcdBarGap + AppTheme::kLcdBarHeight
                                   + 2 * AppTheme::kLcdPaddingY;

    /** One drawable part of the display in widget coordinates, lit or ghost. */
    struct Shape {
        QPainterPath path;
        bool lit = false;
    };

    /** Shear around the bottom of a box of @p height at @p origin, like the classic LCD digits. */
    QTransform slanted(const QPointF origin, const qreal height) {
        return QTransform(1, 0, -LcdSegmentFont::kSlant, 1, origin.x() + LcdSegmentFont::kSlant * height, origin.y());
    }

    /** Width of @p symbol at cap height @p height, slant overhang included. */
    qreal symbolWidth(const QString &symbol, const qreal height) {
        return LcdSymbolFont::width(symbol, height) + LcdSegmentFont::kSlant * height;
    }

    /** Annunciator @p symbol left-aligned in @p box, slanted like the digits. */
    void appendSymbol(QVector<Shape> &shapes, const QString &symbol, const QRectF &box) {
        if (symbol.isEmpty())
            return;
        shapes.append({slanted(box.topLeft(), box.height()).map(LcdSymbolFont::path(symbol, box.height())), true});
    }
}

LcdDisplayWidget::LcdDisplayWidget(const SensorReading &reading, QWidget *parent)
    : QWidget(parent), m_reading(reading) {
    setMinimumHeight(kDisplayHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void LcdDisplayWidget::setReading(const SensorReading &reading) {
    m_reading = reading;
    // Compared with what was last drawn, so a repeated reading and a pending repaint both stay correct.
    if (!m_hasPainted || displayStateFor(reading) != m_painted) {
        update();
    }
}

qreal LcdDisplayWidget::unitFieldWidth(const qreal digitHeight) {
    static const QStringList symbols = SensorFormat::allUnitSymbols();
    const qreal height = AppTheme::kLcdUnitScale * digitHeight;
    qreal width = 0.0;
    for (const QString &symbol: symbols)
        width = std::max(width, symbolWidth(symbol, height));
    return width;
}

LcdDisplayWidget::Layout LcdDisplayWidget::layoutFor(const QSizeF &size) {
    Layout layout;
    const qreal innerWidth = std::max<qreal>(size.width() - 2 * AppTheme::kLcdPaddingX, 1.0);
    const qreal barTop = size.height() - AppTheme::kLcdPaddingY - AppTheme::kLcdBarHeight;
    const qreal digitArea = std::max<qreal>(barTop - AppTheme::kLcdBarGap - AppTheme::kLcdPaddingY, 1.0);

    // Value cells, gap and unit field all scale with the digit height; shrink only when too narrow.
    const auto requiredWidth = [](const qreal height) {
        return LcdSegmentFont::cellsWidth(SensorFormat::kValueCells, height) + AppTheme::kLcdUnitGap * height
               + unitFieldWidth(height);
    };
    const qreal fullWidth = requiredWidth(digitArea);
    layout.digitHeight = fullWidth <= innerWidth ? digitArea : digitArea * innerWidth / fullWidth;
    const qreal h = layout.digitHeight;

    const qreal right = AppTheme::kLcdPaddingX + innerWidth;
    const qreal unitWidth = unitFieldWidth(h);
    const qreal valueRight = right - unitWidth - AppTheme::kLcdUnitGap * h;
    layout.cellCount = std::max(SensorFormat::kValueCells,
                                LcdSegmentFont::cellsThatFit(valueRight - AppTheme::kLcdPaddingX, h));
    const qreal cellsWidth = LcdSegmentFont::cellsWidth(layout.cellCount, h);
    const qreal baseline = barTop - AppTheme::kLcdBarGap;
    layout.cells = QRectF(valueRight - cellsWidth, baseline - h, cellsWidth, h);

    // The state annunciator sits on the bar graph row; the unit sits above it, no lower
    // than the digit baseline.
    const qreal barBottom = barTop + AppTheme::kLcdBarHeight;
    const qreal stateHeight = AppTheme::kLcdStateScale * h;
    layout.stateBox = QRectF(right - unitWidth, barBottom - stateHeight, unitWidth, stateHeight);
    const qreal unitHeight = AppTheme::kLcdUnitScale * h;
    const qreal unitBottom = std::min(baseline, layout.stateBox.top() - AppTheme::kLcdStateGap * h);
    layout.unitBox = QRectF(right - unitWidth, unitBottom - unitHeight, unitWidth, unitHeight);
    const qreal barRight = layout.stateBox.left() - AppTheme::kLcdUnitGap * h;
    layout.bar = QRectF(AppTheme::kLcdPaddingX, barTop, std::max<qreal>(barRight - AppTheme::kLcdPaddingX, 1.0),
                        AppTheme::kLcdBarHeight);
    return layout;
}

int LcdDisplayWidget::minimumFullHeightWidth() {
    const qreal h = kDigitHeight;
    const qreal width = LcdSegmentFont::cellsWidth(SensorFormat::kValueCells, h) + AppTheme::kLcdUnitGap * h
                        + unitFieldWidth(h) + 2 * AppTheme::kLcdPaddingX;
    return static_cast<int>(std::ceil(width));
}

LcdDisplayWidget::DisplayState LcdDisplayWidget::displayStateFor(const SensorReading &reading) const {
    const SensorFormat::DisplayValue shown = SensorFormat::display(reading);
    const Layout layout = layoutFor(size());
    return {shown.digits, shown.symbol,
            litBarSegments(reading, static_cast<int>(barGraphSegments(layout.bar).size())),
            SensorsPolicy::alertState(reading)};
}

QString LcdDisplayWidget::stateSymbol(const AlertState alert) {
    switch (alert) {
        case AlertState::High: return QStringLiteral("HI");
        case AlertState::Low: return QStringLiteral("LO");
        case AlertState::None: break;
    }
    return {};
}

QSize LcdDisplayWidget::sizeHint() const {
    return {AppTheme::kCardMinWidth, kDisplayHeight};
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
    const DisplayState state = displayStateFor(m_reading);
    m_painted = state;
    m_hasPainted = true;
    const bool alert = state.alert != AlertState::None;
    const QColor lit(dark ? (alert ? AppTheme::kLcdAlertDarkRgb : AppTheme::kLcdNormalDarkRgb)
                          : (alert ? AppTheme::kLcdAlertRgb : AppTheme::kLcdNormalRgb));
    QColor ghost(dark ? AppTheme::kLcdNormalDarkRgb : AppTheme::kLcdNormalRgb);
    ghost.setAlpha(dark ? AppTheme::kLcdGhostAlphaDark : AppTheme::kLcdGhostAlpha);
    const QColor shadow(0, 0, 0, AppTheme::kLcdSegmentShadowAlpha);

    // Collect everything to draw: digit segments, unit symbol and bar graph.
    const Layout layout = layoutFor(size());
    const qreal h = layout.digitHeight;
    QVector<Shape> shapes;
    const auto cells = LcdSegmentFont::layoutCells(state.digits, layout.cellCount, SensorFormat::kValueCells, h);
    // Digits that need more cells than the layout has grow to the left.
    const qreal cellsLeft = layout.cells.right() - LcdSegmentFont::cellsWidth(static_cast<int>(cells.size()), h);
    for (const LcdSegmentFont::PlacedGlyph &placed: cells) {
        const QTransform transform = slanted({cellsLeft + placed.cell.left(), layout.cells.top()}, h);
        const LcdSegmentFont::SegmentMask drawn = placed.glyph.lit | placed.glyph.ghost;
        for (LcdSegmentFont::SegmentMask bit = 1; bit <= LcdSegmentFont::kLastSegment; bit <<= 1) {
            if (!(drawn & bit))
                continue;
            QPainterPath path;
            path.addPolygon(LcdSegmentFont::segmentPolygon(static_cast<LcdSegmentFont::Segment>(bit),
                                                           placed.cell.size()));
            path.closeSubpath();
            shapes.append({transform.map(path), (placed.glyph.lit & bit) != 0});
        }
    }
    appendSymbol(shapes, state.unit, layout.unitBox);
    // The alert does not depend on colour alone.
    appendSymbol(shapes, stateSymbol(state.alert), layout.stateBox);
    const QVector<QPolygonF> barSegments = barGraphSegments(layout.bar);
    for (int i = 0; i < barSegments.size(); ++i) {
        QPainterPath path;
        path.addPolygon(barSegments.at(i));
        path.closeSubpath();
        shapes.append({path, i < state.litSegments});
    }

    // Lit segments float above the backplane and cast a faint offset shadow.
    const qreal shadowOffset = std::max<qreal>(0.7, h * 0.035);
    enum class Pass { Shadow, Glow, Face };
    const auto drawShapes = [&](const Pass pass, const QPointF offset = {}, const qreal glowWidth = 0.0) {
        if (pass == Pass::Glow) {
            QColor halo = lit;
            halo.setAlpha(AppTheme::kLcdGlowAlpha);
            painter.setPen(QPen(halo, glowWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(halo);
        } else {
            painter.setPen(Qt::NoPen);
            if (pass == Pass::Shadow)
                painter.setBrush(shadow);
        }
        painter.translate(offset);
        for (const Shape &shape: shapes) {
            if (!shape.lit && pass != Pass::Face)
                continue;
            if (pass == Pass::Face)
                painter.setBrush(shape.lit ? lit : ghost);
            painter.drawPath(shape.path);
        }
        painter.resetTransform();
    };
    if (dark) {
        // Two soft halo layers approximate a blur without offscreen rendering.
        drawShapes(Pass::Glow, {}, shadowOffset * 3.0);
        drawShapes(Pass::Glow, {}, shadowOffset * 1.5);
    } else {
        drawShapes(Pass::Shadow, {shadowOffset, shadowOffset});
    }
    drawShapes(Pass::Face);
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
        const auto mix = [t](const qreal from, const qreal to) {
            return static_cast<float>(from + (to - from) * t);
        };
        return QColor::fromRgbF(mix(a.redF(), b.redF()), mix(a.greenF(), b.greenF()),
                                mix(a.blueF(), b.blueF()));
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
