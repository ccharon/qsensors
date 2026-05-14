// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors_backend.h"

#include <QWidget>

class LcdDisplayWidget;
class QProgressBar;
class QGroupBox;

// Composed sensor tile: framed title, LCD display and min/max progress bar.
class SensorValueWidget final : public QWidget {
    Q_OBJECT

public:
    explicit SensorValueWidget(const SensorReading &reading, QWidget *parent = nullptr);
    void setReading(const SensorReading &reading);

private:
    QGroupBox *m_groupBox;
    LcdDisplayWidget *m_lcdValue;
    QProgressBar *m_rangeBar;
};
