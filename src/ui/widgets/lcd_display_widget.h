// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "lcd_glyph_atlas.h"
#include "sensors_backend.h"

#include <QWidget>

/** Paints xsensors-style LCD digits using the imported theme sprite. */
class LcdDisplayWidget final : public QWidget {
    Q_OBJECT

public:
    explicit LcdDisplayWidget(const SensorReading &reading, QWidget *parent = nullptr);

    void setReading(const SensorReading &reading);

    QSize sizeHint() const override;

    /** Formats the numeric part exactly like xsensors for known units. */
    static QString valueDigitsFor(const SensorReading &reading);

    /** Alarm state decides which sprite row is used for rendering. */
    static bool isAlertState(const SensorReading &reading);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    SensorReading m_reading;
};
