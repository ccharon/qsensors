// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensor_value_widget.h"
#include "lcd_display_widget.h"

#include <QGroupBox>
#include <QProgressBar>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

SensorValueWidget::SensorValueWidget(const SensorReading &reading, QWidget *parent)
    : QWidget(parent), m_groupBox(new QGroupBox(this)), m_lcdValue(new LcdDisplayWidget(reading, this)), m_rangeBar(new QProgressBar(this)) {
    setMinimumSize(150, 74);
    setMaximumWidth(170);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_groupBox->setTitle(reading.feature + QStringLiteral(":"));
    m_groupBox->setStyleSheet(QStringLiteral(
        "QGroupBox {"
        "  border: 1px solid #b7b7b7;"
        "  margin-top: 8px;"
        "  padding-top: 4px;"
        "  padding-bottom: 0px;"
        "  background: #ececec;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 6px;"
        "  top: 0px;"
        "  padding: 0 2px;"
        "  font-size: 11px;"
        "  background: #ececec;"
        "}"
    ));

    auto *groupLayout = new QVBoxLayout(m_groupBox);
    groupLayout->setContentsMargins(4, 2, 4, 0);
    groupLayout->setSpacing(1);

    setReading(reading);

    groupLayout->addWidget(m_lcdValue, 1);
    groupLayout->addWidget(m_rangeBar);
    layout->addWidget(m_groupBox);

    setLayout(layout);
}

void SensorValueWidget::setReading(const SensorReading &reading) {
    m_groupBox->setTitle(reading.feature + QStringLiteral(":"));
    m_lcdValue->setReading(reading);

    // Keep bar behavior aligned with xsensors-style limit semantics.
    if (reading.hasRange) {
        constexpr int scale = 1000;
        double min = reading.hasMin ? reading.minValue : reading.value;
        double max = reading.hasMax ? reading.maxValue : reading.value;

        if (reading.unit == QStringLiteral("V") && reading.hasMin && reading.hasMax &&
            min < 0.0 && max < 0.0 && max < min) {
            std::swap(min, max);
        }

        if (!reading.hasMin && reading.hasMax) {
            min = (reading.unit == QStringLiteral("°C")) ? 0.0 : std::max(0.0, reading.maxValue * 0.2);
        } else if (reading.hasMin && !reading.hasMax) {
            if (reading.unit == QStringLiteral("RPM")) {
                max = std::max(reading.value, reading.minValue * 2.0);
            } else {
                max = std::max(reading.value, reading.minValue + std::max(1.0, std::abs(reading.minValue) * 0.5));
            }
        }

        if (max <= min) {
            max = min + 1.0;
        }

        const double clampedValue = std::clamp(reading.value, min, max);

        m_rangeBar->setRange(static_cast<int>(min * scale), static_cast<int>(max * scale));
        m_rangeBar->setValue(static_cast<int>(clampedValue * scale));
        m_rangeBar->setTextVisible(false);
        m_rangeBar->setFixedHeight(6);
        m_rangeBar->setStyleSheet(QStringLiteral(
            "QProgressBar {"
            "  border: none;"
            "  background: #d9d9d9;"
            "}"
            "QProgressBar::chunk {"
            "  background: #4db8ff;"
            "}"
        ));
    } else {
        m_rangeBar->setFixedHeight(6);
        m_rangeBar->setRange(0, 1000);
        m_rangeBar->setValue(0);
        m_rangeBar->setTextVisible(false);
        m_rangeBar->setStyleSheet(QStringLiteral(
            "QProgressBar {"
            "  border: none;"
            "  background: #d9d9d9;"
            "}"
            "QProgressBar::chunk {"
            "  background: transparent;"
            "}"
        ));
    }
}
