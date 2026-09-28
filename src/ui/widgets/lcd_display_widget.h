// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors/sensor_reading.h"

#include <QPolygonF>
#include <QVector>
#include <QWidget>

class QPainter;

/** Paints an xsensors-inspired segment LCD: value, unit and a range bar graph. */
class LcdDisplayWidget final : public QWidget {
    Q_OBJECT

public:
    /** Creates the display showing @p reading. */
    explicit LcdDisplayWidget(const SensorReading &reading, QWidget *parent = nullptr);

    /** Shows @p reading and schedules a repaint. */
    void setReading(const SensorReading &reading);

    /** Card width with digits, bar graph and padding at their default height. */
    QSize sizeHint() const override;

    /** Number of lit bar graph segments out of @p segmentCount. */
    static int litBarSegments(const SensorReading &reading, int segmentCount);

    /** Slanted bar graph cells filling @p area; count follows the available width. */
    static QVector<QPolygonF> barGraphSegments(const QRectF &area);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    /** Recessed, slightly tinted LCD backplane behind the segments. */
    void paintPanel(QPainter &painter) const;

    /** True when the palette base is dark; selects backlit colors and glow. */
    [[nodiscard]] bool hasDarkBase() const;

    SensorReading m_reading;
};
