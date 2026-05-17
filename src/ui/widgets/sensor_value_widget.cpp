// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensor_value_widget.h"
#include "theme/app_theme.h"
#include "lcd_display_widget.h"

#include <QGroupBox>
#include <QPalette>
#include <QProgressBar>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

SensorValueWidget::SensorValueWidget(const SensorReading &reading, QWidget *parent)
    : QWidget(parent), m_groupBox(new QGroupBox(this)), m_lcdValue(new LcdDisplayWidget(reading, this)), m_rangeBar(new QProgressBar(this)) {
    setMinimumWidth(AppTheme::kCardMinWidth);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMaximumWidth(AppTheme::kCardWidth);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_groupBox->setTitle(reading.feature + QStringLiteral(":"));
    const QColor panelBg = palette().color(QPalette::Window);
    const QString borderColor = panelBg.lightness() > 140 ? QStringLiteral("palette(mid)") : QStringLiteral("rgb(230,230,230)");
    m_groupBox->setStyleSheet(AppTheme::sensorGroupStyle(borderColor));
    m_groupBox->setContentsMargins(0,12,0,0);

    auto *groupLayout = new QVBoxLayout(m_groupBox);
    groupLayout->setContentsMargins(AppTheme::kCardBorderPadding, 3, AppTheme::kCardBorderPadding, 3);
    groupLayout->setSpacing(0);

    setReading(reading);

    groupLayout->addWidget(m_lcdValue);
    groupLayout->addWidget(m_rangeBar);
    layout->addWidget(m_groupBox);

    setLayout(layout);
    setFixedHeight(layout->sizeHint().height());
}

void SensorValueWidget::setReading(const SensorReading &reading) {
    m_groupBox->setTitle(reading.feature + QStringLiteral(":"));
    m_lcdValue->setReading(reading);

    // Keep bar behavior aligned with xsensors-style limit semantics.
    if (reading.hasRange) {
        constexpr int scale = 1000;
        // Backend provides finalized range policy values; widget only renders.
        double min = reading.minValue;
        double max = reading.maxValue;

        if (max <= min) {
            max = min + 1.0;
        }

        const double clampedValue = std::clamp(reading.value, min, max);

        m_rangeBar->setRange(static_cast<int>(min * scale), static_cast<int>(max * scale));
        m_rangeBar->setValue(static_cast<int>(clampedValue * scale));
        m_rangeBar->setTextVisible(false);
        m_rangeBar->setFixedHeight(AppTheme::kRangeBarHeight);
        m_rangeBar->setStyleSheet(AppTheme::progressBarStyle(true));
    } else {
        m_rangeBar->setFixedHeight(AppTheme::kRangeBarHeight);
        m_rangeBar->setRange(0, 1000);
        m_rangeBar->setValue(0);
        m_rangeBar->setTextVisible(false);
        m_rangeBar->setStyleSheet(AppTheme::progressBarStyle(false));
    }
}
