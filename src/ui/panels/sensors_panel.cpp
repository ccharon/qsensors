// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors_panel.h"

#include "sensor_value_widget.h"

#include <QCoreApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QMetaEnum>
#include <QStringList>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {
    QString translatedCategoryName(const SensorCategory category) {
        const QMetaEnum metaEnum = QMetaEnum::fromType<SensorCategory>();
        const char *key = metaEnum.valueToKey(static_cast<int>(category));
        if (key == nullptr) {
            key = metaEnum.valueToKey(static_cast<int>(SensorCategory::Other));
        }
        if (key == nullptr) {
            return {};
        }
        return QCoreApplication::translate("MainWindow", key);
    }
}

SensorsPanel::SensorsPanel(QWidget *parent) : QWidget(parent), m_layout(new QVBoxLayout(this)) {
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(10);
}

void SensorsPanel::setChipExpandedState(const QHash<QString, bool> &state) {
    m_chipExpanded = state;
}

QHash<QString, bool> SensorsPanel::chipExpandedState() const {
    return m_chipExpanded;
}

void SensorsPanel::setReadings(const QVector<SensorReading> &readings, const int viewportWidth) {
    m_readings = readings;
    const QString currentStructureFingerprint = structureFingerprint(m_readings);
    if (m_lastStructureFingerprint != currentStructureFingerprint) {
        m_lastStructureFingerprint = currentStructureFingerprint;
        renderReadings(viewportWidth);
    } else {
        updateVisibleReadings();
    }
}

void SensorsPanel::relayout(const int viewportWidth) {
    if (m_readings.isEmpty()) {
        return;
    }
    renderReadings(viewportWidth);
}

void SensorsPanel::clearContent() {
    while (QLayoutItem *item = m_layout->takeAt(0)) {
        if (item->widget() != nullptr) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void SensorsPanel::renderReadings(const int viewportWidth) {
    setUpdatesEnabled(false);
    clearContent();
    m_sensorWidgets.clear();

    QMap<QString, QMap<SensorCategory, QVector<SensorReading> > > grouped;
    for (const SensorReading &r: m_readings) {
        grouped[r.chip][r.category].push_back(r);
    }

    const int scrollbarReserve = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    const int stableViewportWidth = std::max(200, viewportWidth - scrollbarReserve);
    constexpr int kCardWidth = 170;
    constexpr int kCategorySpacing = 12;
    constexpr int kGridSpacing = 3;
    constexpr int kMaxColumnsPerCategory = 3;
    constexpr int kChipContentHorizontalMargins = 16;

    for (auto chipIt = grouped.cbegin(); chipIt != grouped.cend(); ++chipIt) {
        const QString &chipName = chipIt.key();
        const QMap<SensorCategory, QVector<SensorReading> > &categories = chipIt.value();
        QList<SensorCategory> orderedCategories = categories.keys();
        std::sort(orderedCategories.begin(), orderedCategories.end(),
                  [](const SensorCategory a, const SensorCategory b) {
                      return static_cast<int>(a) < static_cast<int>(b);
                  });

        auto *chipCard = new QFrame(this);
        chipCard->setObjectName(QStringLiteral("chipCard"));
        chipCard->setStyleSheet(QStringLiteral(
            "#chipCard {"
            "  border: 1px solid palette(mid);"
            "  background: palette(window);"
            "  color: palette(window-text);"
            "}"
        ));

        auto *chipLayout = new QVBoxLayout(chipCard);
        chipLayout->setContentsMargins(0, 0, 0, 0);
        chipLayout->setSpacing(0);

        auto *header = new QToolButton(chipCard);
        header->setText(chipName);
        header->setCheckable(true);
        header->setChecked(m_chipExpanded.value(chipName, true));
        header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        header->setArrowType(header->isChecked() ? Qt::DownArrow : Qt::RightArrow);
        header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        header->setStyleSheet(QStringLiteral(
            "QToolButton {"
            "  border: none;"
            "  text-align: left;"
            "  font-weight: bold;"
            "  padding: 6px 8px;"
            "  background: palette(button);"
            "  color: palette(button-text);"
            "}"
        ));

        auto *chipContent = new QWidget(chipCard);
        auto *chipContentLayout = new QVBoxLayout(chipContent);
        chipContentLayout->setContentsMargins(8, 8, 8, 8);
        chipContentLayout->setSpacing(8);
        auto *categoryRow = new QHBoxLayout();
        categoryRow->setSpacing(kCategorySpacing);
        chipContentLayout->addLayout(categoryRow);

        chipContent->setVisible(header->isChecked());
        connect(header, &QToolButton::toggled, this, [this, header, chipContent, chipName](bool expanded) {
            m_chipExpanded[chipName] = expanded;
            header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
            chipContent->setVisible(expanded);
            emit chipExpandedStateChanged(m_chipExpanded);
        });

        chipLayout->addWidget(header);
        chipLayout->addWidget(chipContent);
        m_layout->addWidget(chipCard);
        m_chipExpanded.insert(chipName, header->isChecked());

        const int categoryCount = std::max(1, static_cast<int>(orderedCategories.size()));
        const int perCategoryWidth = std::max(
            kCardWidth,
            (stableViewportWidth - kChipContentHorizontalMargins - ((categoryCount - 1) * kCategorySpacing)) /
            categoryCount
        );
        const int columnsPerCategory = std::clamp(
            (perCategoryWidth + kGridSpacing) / (kCardWidth + kGridSpacing),
            1,
            kMaxColumnsPerCategory
        );

        for (const SensorCategory categoryName: orderedCategories) {
            const QVector<SensorReading> &categoryReadings = categories.value(categoryName);

            auto *categoryContainer = new QWidget(chipContent);
            categoryContainer->setMinimumWidth(perCategoryWidth);
            auto *categoryContainerLayout = new QVBoxLayout(categoryContainer);
            categoryContainerLayout->setContentsMargins(0, 0, 0, 0);
            categoryContainerLayout->setSpacing(3);

            auto *categoryTitle = new QLabel(translatedCategoryName(categoryName) + QStringLiteral(":"), categoryContainer);
            QFont catFont = categoryTitle->font();
            catFont.setBold(true);
            categoryTitle->setFont(catFont);
            categoryContainerLayout->addWidget(categoryTitle);

            auto *categoryGrid = new QGridLayout();
            categoryGrid->setContentsMargins(0, 0, 0, 0);
            categoryGrid->setSpacing(kGridSpacing);
            categoryGrid->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            for (int i = 0; i < categoryReadings.size(); ++i) {
                const int row = i / columnsPerCategory;
                const int col = i % columnsPerCategory;
                auto *sensorWidget = new SensorValueWidget(categoryReadings[i], categoryContainer);
                categoryGrid->addWidget(sensorWidget, row, col, Qt::AlignLeft | Qt::AlignTop);
                m_sensorWidgets.insert(sensorKey(categoryReadings[i]), sensorWidget);
            }
            categoryGrid->setColumnStretch(columnsPerCategory, 1);
            categoryContainerLayout->addLayout(categoryGrid);
            categoryContainerLayout->addStretch(1);

            categoryRow->addWidget(categoryContainer, 1);
        }
    }

    m_layout->addStretch(1);
    setUpdatesEnabled(true);
    emit chipExpandedStateChanged(m_chipExpanded);
}

void SensorsPanel::updateVisibleReadings() {
    for (const SensorReading &reading: m_readings) {
        if (SensorValueWidget *widget = m_sensorWidgets.value(sensorKey(reading), nullptr)) {
            widget->setReading(reading);
        }
    }
}

QString SensorsPanel::structureFingerprint(const QVector<SensorReading> &readings) {
    QStringList entries;
    entries.reserve(readings.size());
    for (const SensorReading &r: readings) {
        entries.push_back(sensorKey(r));
    }
    entries.sort();
    return entries.join(QStringLiteral("\n"));
}

QString SensorsPanel::sensorKey(const SensorReading &reading) {
    return reading.chip + QStringLiteral("|")
           + QString::number(static_cast<int>(reading.category)) + QStringLiteral("|")
           + reading.feature + QStringLiteral("|") + reading.unit;
}
