// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors/sensor_reading.h"

#include <QWidget>

class LcdDisplayWidget;
class QLabel;

/** Composed sensor tile: title label above an LCD panel with value and range bar graph. */
class SensorValueWidget final : public QWidget {
    Q_OBJECT

public:
    /** Creates the card for @p reading with fixed height and theme width limits. */
    explicit SensorValueWidget(const SensorReading &reading, QWidget *parent = nullptr);

    /** Refreshes title, LCD (value and range bar graph) and, if its text changed, the details tooltip. */
    void setReading(const SensorReading &reading);

    /**
     * Tooltip with full sensor name, chip, a note when the sensor cannot be read, and the
     * firmware min/max limits in display units.
     */
    [[nodiscard]] static QString detailsToolTip(const SensorReading &reading);

protected:
    void resizeEvent(QResizeEvent *event) override;

    /** Derives the title font again when the card's font changes. */
    void changeEvent(QEvent *event) override;

    /** Keeps the fixed height at what the layout needs (it changes with the font). */
    bool event(QEvent *event) override;

private:
    /** Title font: the card's font scaled by the theme. */
    void updateTitleFont();

    /** Sets the fixed height to the layout's current height. */
    void updateFixedHeight();

    /** Long sensor names are elided; the full name is shown in the details tooltip. */
    void updateElidedTitle();

    QLabel *m_title;
    LcdDisplayWidget *m_lcdValue;
    QString m_fullTitle;
};
