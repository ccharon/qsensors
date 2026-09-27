// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors_backend.h"

#include <QWidget>

class LcdDisplayWidget;
class QLabel;

/** Composed sensor tile: title label above an LCD panel with value and range bar graph. */
class SensorValueWidget final : public QWidget {
    Q_OBJECT

public:
    explicit SensorValueWidget(const SensorReading &reading, QWidget *parent = nullptr);

    /** Refreshes title and LCD (value and range bar graph). */
    void setReading(const SensorReading &reading);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    /** Long sensor names are elided; the full name is available as tooltip. */
    void updateElidedTitle();

    QLabel *m_title;
    LcdDisplayWidget *m_lcdValue;
    QString m_fullTitle;
};
