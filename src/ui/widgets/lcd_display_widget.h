// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors/sensor_reading.h"
#include "ui/widgets/lcd_segment_font.h"

#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QWidget>

class QPainter;

/**
 * Paints an xsensors-inspired segment LCD in the style of a multimeter: the value
 * right-aligned in a fixed number of digit cells, the unit in a fixed field at the right
 * edge with a HI/LO annunciator under it, and a range bar graph along the bottom. Every card of the same width puts its
 * cells, decimal points and units at the same place; extra width adds ghost digits on
 * the left.
 */
class LcdDisplayWidget final : public QWidget {
    Q_OBJECT

public:
    /** Creates the display showing @p reading. */
    explicit LcdDisplayWidget(const SensorReading &reading, QWidget *parent = nullptr);

    /**
     * Shows @p reading; schedules a repaint only if the drawn digits, unit, lit bar segments
     * or alert state (including which annunciator, HI or LO) change.
     */
    void setReading(const SensorReading &reading);

    /** Card width with digits, bar graph and padding at their default height. */
    QSize sizeHint() const override;

    /** Number of lit bar graph segments out of @p segmentCount. */
    static int litBarSegments(const SensorReading &reading, int segmentCount);

    /** Slanted bar graph cells filling @p area; count follows the available width. */
    static QVector<QPolygonF> barGraphSegments(const QRectF &area);

    /** Where everything goes for a display of a given size; independent of the reading. */
    struct Layout {
        qreal digitHeight = 0.0;
        int cellCount = 0; // digit cells, at least SensorFormat::kValueCells
        QRectF cells; // unslanted box of all digit cells, widget coordinates
        QRectF unitBox; // unslanted box of the unit symbol; symbols are left-aligned in it
        QRectF stateBox; // unslanted box of the HI/LO annunciator under the unit
        QRectF bar; // range bar graph, ending before the state annunciator
    };

    /** Layout for a display of @p size. */
    [[nodiscard]] static Layout layoutFor(const QSizeF &size);

    /** Width the unit field needs at @p digitHeight: the widest unit symbol of the unit table. */
    [[nodiscard]] static qreal unitFieldWidth(qreal digitHeight);

    /** Annunciator text for @p alert: "HI", "LO" or empty. LCD symbols, not translated. */
    [[nodiscard]] static QString stateSymbol(AlertState alert);

    /** Smallest display width that shows the value cells and the unit field at full digit height. */
    [[nodiscard]] static int minimumFullHeightWidth();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    /** Everything the display draws that depends on the reading; equal states paint identically. */
    struct DisplayState {
        QString digits;
        QString unit;
        int litSegments = 0;
        AlertState alert = AlertState::None;
        bool operator==(const DisplayState &) const = default;
    };

    /** State of @p reading at the current widget size. */
    [[nodiscard]] DisplayState displayStateFor(const SensorReading &reading) const;

    /** Recessed, slightly tinted LCD backplane behind the segments. */
    void paintPanel(QPainter &painter) const;

    /** True when the palette base is dark; selects backlit colors and glow. */
    [[nodiscard]] bool hasDarkBase() const;

    SensorReading m_reading;
    DisplayState m_painted; // state of the last paint; compared by setReading()
    bool m_hasPainted = false;
};
