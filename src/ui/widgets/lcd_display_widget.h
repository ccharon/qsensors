// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors_backend.h"

#include <QPolygonF>
#include <QVector>
#include <QWidget>
#include <optional>

class QPainter;

/** Paints an xsensors-inspired segment LCD: value, unit and a range bar graph. */
class LcdDisplayWidget final : public QWidget {
    Q_OBJECT

public:
    explicit LcdDisplayWidget(const SensorReading &reading, QWidget *parent = nullptr);

    void setReading(const SensorReading &reading);

    QSize sizeHint() const override;

    /** Formats the numeric part exactly like xsensors for known units. */
    static QString valueDigitsFor(const SensorReading &reading);

    /** Alarm state decides whether the lit segments use the alert color. */
    static bool isAlertState(const SensorReading &reading);

    /** Position of the value within its min/max range (0..1), nullopt without range. */
    static std::optional<double> rangeFraction(const SensorReading &reading);

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
