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

    /** Refreshes title, details tooltip and LCD (value and range bar graph). */
    void setReading(const SensorReading &reading);

    /** Tooltip with full sensor name, chip and the min/max limits in display units. */
    [[nodiscard]] static QString detailsToolTip(const SensorReading &reading);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    /** Long sensor names are elided; the full name is shown in the details tooltip. */
    void updateElidedTitle();

    QLabel *m_title;
    LcdDisplayWidget *m_lcdValue;
    QString m_fullTitle;
};
