// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/widgets/sensor_value_widget.h"
#include "ui/theme/app_theme.h"
#include "ui/widgets/lcd_display_widget.h"
#include "sensors/sensor_format.h"

#include <QLabel>
#include <QResizeEvent>
#include <QVBoxLayout>

SensorValueWidget::SensorValueWidget(const SensorReading &reading, QWidget *parent)
    : QWidget(parent), m_title(new QLabel(this)), m_lcdValue(new LcdDisplayWidget(reading, this)) {
    setMinimumWidth(AppTheme::kCardMinWidth);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMaximumWidth(AppTheme::kCardMaxWidth);

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
        updateElidedTitle();
    }
    // Set on the card itself so hovering title or LCD shows the same details.
    const QString details = detailsToolTip(reading);
    if (toolTip() != details)
        setToolTip(details);
    m_lcdValue->setReading(reading);
}

QString SensorValueWidget::detailsToolTip(const SensorReading &reading) {
    const auto format = [&reading](const double v) { return SensorFormat::valueWithUnit(reading.unit, v); };

    QStringList lines;
    lines << QStringLiteral("<b>%1</b>").arg(reading.feature.toHtmlEscaped());
    lines << tr("Chip: %1").arg(reading.chip.toHtmlEscaped());
    // Only firmware limits: the default bar graph range is a guess, not a limit.
    if (reading.firmwareMin)
        lines << tr("Min: %1").arg(format(*reading.firmwareMin).toHtmlEscaped());
    if (reading.firmwareMax)
        lines << tr("Max: %1").arg(format(*reading.firmwareMax).toHtmlEscaped());
    if (!reading.hasFirmwareLimits())
        lines << tr("No limits available");
    return lines.join(QStringLiteral("<br>"));
}

void SensorValueWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateElidedTitle();
}

void SensorValueWidget::updateElidedTitle() {
    const int available = m_title->width() - m_title->contentsMargins().left();
    m_title->setText(m_title->fontMetrics().elidedText(m_fullTitle, Qt::ElideRight, std::max(available, 0)));
}
