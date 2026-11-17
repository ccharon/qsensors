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
    // Hardware-supplied text: never interpreted as rich text.
    m_title->setTextFormat(Qt::PlainText);
    updateTitleFont();
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_title->setContentsMargins(AppTheme::kCardTitleInset, 0, 0, 0);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(AppTheme::kCardTitleSpacing);
    layout->addWidget(m_title);
    layout->addWidget(m_lcdValue);

    setReading(reading);
    updateFixedHeight();
}

void SensorValueWidget::updateTitleFont() {
    // Derived from the card's own font, so a font change on the card carries over to the title.
    QFont titleFont = font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * AppTheme::kCardTitleFontScale);
    m_title->setFont(titleFont);
}

void SensorValueWidget::updateFixedHeight() {
    const int height = layout()->sizeHint().height();
    if (height != minimumHeight() || height != maximumHeight()) {
        setFixedHeight(height);
    }
}

void SensorValueWidget::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        updateTitleFont();
    }
}

bool SensorValueWidget::event(QEvent *event) {
    const bool handled = QWidget::event(event);
    // The layout reports a changed size hint (e.g. after a font change) this way.
    if (event->type() == QEvent::LayoutRequest) {
        updateFixedHeight();
    }
    return handled;
}

void SensorValueWidget::setReading(const SensorReading &reading) {
    const QString newTitle = reading.feature + QStringLiteral(":");
    if (m_fullTitle != newTitle) {
        m_fullTitle = newTitle;
        updateElidedTitle();
    }
    // Set on the card itself so hovering title or LCD shows the same details.
    // Compared as text, so a field added to the tooltip cannot go stale; an unchanged tooltip
    // is left alone so one that is currently shown does not flicker.
    const QString toolTipText = detailsToolTip(reading);
    if (toolTipText != toolTip()) {
        setToolTip(toolTipText);
    }
    m_lcdValue->setReading(reading);
}

QString SensorValueWidget::detailsToolTip(const SensorReading &reading) {
    const auto format = [&reading](const double v) { return SensorFormat::valueWithUnit(reading, v); };

    QStringList lines;
    lines << QStringLiteral("<b>%1</b>").arg(reading.feature.toHtmlEscaped());
    lines << tr("Chip: %1").arg(reading.chip.toHtmlEscaped());
    if (!reading.hasValue())
        lines << tr("The sensor cannot be read at the moment");
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
