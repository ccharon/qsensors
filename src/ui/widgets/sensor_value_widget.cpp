// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensor_value_widget.h"
#include "theme/app_theme.h"
#include "lcd_display_widget.h"

#include <QLabel>
#include <QResizeEvent>
#include <QVBoxLayout>

SensorValueWidget::SensorValueWidget(const SensorReading &reading, QWidget *parent)
    : QWidget(parent), m_title(new QLabel(this)), m_lcdValue(new LcdDisplayWidget(reading, this)) {
    setMinimumWidth(AppTheme::kCardMinWidth);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMaximumWidth(AppTheme::kCardWidth);

    // The LCD panel is the card; the title is plain theme text above it.
    QFont titleFont = m_title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * AppTheme::kCardTitleFontScale);
    m_title->setFont(titleFont);
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_title->setContentsMargins(AppTheme::kCardTitleInset, 0, 0, 0);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(AppTheme::kCardTitleSpacing);
    layout->addWidget(m_title);
    layout->addWidget(m_lcdValue);

    setReading(reading);
    setFixedHeight(layout->sizeHint().height());
}

void SensorValueWidget::setReading(const SensorReading &reading) {
    const QString newTitle = reading.feature + QStringLiteral(":");
    if (m_fullTitle != newTitle) {
        m_fullTitle = newTitle;
        m_title->setToolTip(reading.feature);
        updateElidedTitle();
    }
    m_lcdValue->setReading(reading);
}

void SensorValueWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateElidedTitle();
}

void SensorValueWidget::updateElidedTitle() {
    const int available = m_title->width() - m_title->contentsMargins().left();
    m_title->setText(m_title->fontMetrics().elidedText(m_fullTitle, Qt::ElideRight, std::max(available, 0)));
}
