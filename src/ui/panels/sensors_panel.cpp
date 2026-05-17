// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors_panel.h"

#include "theme/app_theme.h"
#include "sensor_value_widget.h"

#include <QCoreApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QStringList>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {
    constexpr int kUnifiedHorizontalSpacing = AppTheme::kGridSpacing + AppTheme::kCategoryVsGridSpacingDelta;

    QString translatedCategoryName(const SensorCategory category) {
        switch (category) {
            case SensorCategory::Voltages:
                return QCoreApplication::translate("SensorsPanel", "Voltages");
            case SensorCategory::Temperatures:
                return QCoreApplication::translate("SensorsPanel", "Temperatures");
            case SensorCategory::Fans:
                return QCoreApplication::translate("SensorsPanel", "Fans");
            case SensorCategory::Currents:
                return QCoreApplication::translate("SensorsPanel", "Currents");
            case SensorCategory::Power:
                return QCoreApplication::translate("SensorsPanel", "Power");
            case SensorCategory::Other:
                return QCoreApplication::translate("SensorsPanel", "Other");
        }
        return QCoreApplication::translate("SensorsPanel", "Other");
    }
}

SensorsPanel::SensorsPanel(QWidget *parent) : QWidget(parent), m_layout(new QVBoxLayout(this)) {
    m_layout->setContentsMargins(AppTheme::kSectionInset, AppTheme::kSectionInset, AppTheme::kSectionInset, AppTheme::kSectionInset);
    m_layout->setSpacing(AppTheme::kSensorsPanelVerticalSpacing);
    m_layout->addStretch(1);
}

void SensorsPanel::setChipExpandedState(const QHash<QString, bool> &state) {
    m_chipExpanded = state;
}

QHash<QString, bool> SensorsPanel::chipExpandedState() const {
    return m_chipExpanded;
}

void SensorsPanel::setReadings(const QVector<SensorReading> &readings, const int viewportWidth) {
    m_readings = readings;
    renderReadings(viewportWidth, false);
}

void SensorsPanel::relayout(const int viewportWidth) {
    if (m_readings.isEmpty()) {
        return;
    }

    renderReadings(viewportWidth, true);
}

int SensorsPanel::minimumRequiredWidth() const {
    const QMap<QString, QMap<SensorCategory, QVector<SensorReading> > > grouped = groupReadingsByChip(m_readings);
    int maxCategoryCount = 1;
    for (auto chipIt = grouped.cbegin(); chipIt != grouped.cend(); ++chipIt) {
        maxCategoryCount = std::max(maxCategoryCount, static_cast<int>(chipIt.value().size()));
    }

    const int panelHorizontalMargins = m_layout->contentsMargins().left() + m_layout->contentsMargins().right();
    const int chipContentHorizontalMargins = AppTheme::kSectionInset * 2;
    const int categoriesWidth = (maxCategoryCount * AppTheme::kCardMinWidth) + ((maxCategoryCount - 1) * kUnifiedHorizontalSpacing);
    return panelHorizontalMargins + AppTheme::kChipCardFrameWidthTotal + chipContentHorizontalMargins + categoriesWidth;
}

void SensorsPanel::renderReadings(const int viewportWidth, const bool forceRebuild) {
    const QMap<QString, QMap<SensorCategory, QVector<SensorReading> > > grouped = groupReadingsByChip(m_readings);
    const int stableViewportWidth = computeStableViewportWidth(viewportWidth);

    // Batch updates avoid flicker while chip sections are reconciled/reordered.
    setUpdatesEnabled(false);

    QStringList orderedChips;
    orderedChips.reserve(grouped.size());
    for (auto chipIt = grouped.cbegin(); chipIt != grouped.cend(); ++chipIt) {
        orderedChips.push_back(chipIt.key());
    }
    removeStaleChipSections(grouped);

    QHash<QString, SensorValueWidget *> reconciledWidgets;
    for (const QString &chipName: orderedChips) {
        const QMap<SensorCategory, QVector<SensorReading> > &categories = grouped.value(chipName);
        reconcileChipSection(chipName, categories, stableViewportWidth, forceRebuild, reconciledWidgets);
    }

    applyChipOrder(orderedChips);

    // Keep stable widget instances whenever structure matches; value refresh happens below.
    m_sensorWidgets = reconciledWidgets;
    updateVisibleReadings();
    setUpdatesEnabled(true);
    update();
    emit chipExpandedStateChanged(m_chipExpanded);
}

QMap<QString, QMap<SensorCategory, QVector<SensorReading> > > SensorsPanel::groupReadingsByChip(const QVector<SensorReading> &readings) {
    QMap<QString, QMap<SensorCategory, QVector<SensorReading> > > grouped;
    for (const SensorReading &r: readings) {
        grouped[r.chip][r.category].push_back(r);
    }
    return grouped;
}

void SensorsPanel::removeStaleChipSections(
    const QMap<QString, QMap<SensorCategory, QVector<SensorReading> > > &grouped) {

    for (auto it = m_chipSections.begin(); it != m_chipSections.end();) {
        if (!grouped.contains(it.key())) {
            if (it->card != nullptr) {
                m_layout->removeWidget(it->card);
                delete it->card;
            }
            it = m_chipSections.erase(it);
        } else {
            ++it;
        }
    }
}

int SensorsPanel::computeStableViewportWidth(const int viewportWidth) {
    return std::max(200, viewportWidth);
}

int SensorsPanel::widthForColumns(const int columns) {
    return (columns * AppTheme::kCardMinWidth) + ((columns - 1) * kUnifiedHorizontalSpacing);
}

void SensorsPanel::reconcileChipSection(
    const QString &chipName,
    const QMap<SensorCategory, QVector<SensorReading> > &categories,
    const int stableViewportWidth,
    const bool forceRebuild,
    QHash<QString, SensorValueWidget *> &reconciledWidgets
) {
    constexpr int kChipContentHorizontalMargins = AppTheme::kSectionInset * 2;
    const int categoryCount = std::max(1, static_cast<int>(categories.size()));
    const int perCategoryWidth = std::max(
        AppTheme::kCardMinWidth,
        (stableViewportWidth - kChipContentHorizontalMargins - ((categoryCount - 1) * kUnifiedHorizontalSpacing)) / categoryCount
    );
    const int columnsPerCategory = std::clamp(
        (perCategoryWidth + kUnifiedHorizontalSpacing) / (AppTheme::kCardMinWidth + kUnifiedHorizontalSpacing),
        1,
        AppTheme::kMaxColumnsPerCategory
    );

    auto it = m_chipSections.find(chipName);
    ChipSection *section = it != m_chipSections.end() ? &it.value() : createChipSection(chipName);
    if (section == nullptr) {
        return;
    }

    const QString structure = chipStructureFingerprint(categories);
    if (forceRebuild || section->structureFingerprint != structure) {
        rebuildChipSection(*section, categories, columnsPerCategory);
        section->structureFingerprint = structure;
    }

    for (auto it = section->widgets.constBegin(); it != section->widgets.constEnd(); ++it) {
        reconciledWidgets.insert(it.key(), it.value());
    }
}

void SensorsPanel::applyChipOrder(const QStringList &orderedChips) {
    while (QLayoutItem *item = m_layout->takeAt(0)) {
        delete item;
    }
    for (const QString &chipName: orderedChips) {
        auto it = m_chipSections.constFind(chipName);
        if (it != m_chipSections.cend() && it->card != nullptr) {
            m_layout->addWidget(it->card);
        }
    }
    m_layout->addStretch(1);
}

SensorsPanel::ChipSection *SensorsPanel::createChipSection(const QString &chipName) {
    ChipSection section{};
    section.card = new QFrame(this);
    section.card->setObjectName(QStringLiteral("chipCard"));
    section.card->setStyleSheet(AppTheme::chipCardStyle());

    auto *chipLayout = new QVBoxLayout(section.card);
    chipLayout->setContentsMargins(0, 0, 0, 0);
    chipLayout->setSpacing(0);

    section.header = new QToolButton(section.card);
    section.header->setText(chipName);
    section.header->setCheckable(true);
    section.header->setChecked(m_chipExpanded.value(chipName, true));
    section.header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    section.header->setArrowType(section.header->isChecked() ? Qt::DownArrow : Qt::RightArrow);
    section.header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    section.header->setStyleSheet(AppTheme::sectionHeaderStyle());

    section.content = new QWidget(section.card);
    auto *chipContentLayout = new QVBoxLayout(section.content);
    chipContentLayout->setContentsMargins(
        AppTheme::kSectionInset, AppTheme::kSectionInset,
        AppTheme::kSectionInset, AppTheme::kSectionInset
    );

    chipContentLayout->setSpacing(AppTheme::kSectionInset);
    section.categoryRow = new QHBoxLayout();
    section.categoryRow->setSpacing(kUnifiedHorizontalSpacing);
    chipContentLayout->addLayout(section.categoryRow);

    section.content->setVisible(section.header->isChecked());
    connect(section.header, &QToolButton::toggled, this, [this, chipName](bool expanded) {
        auto it = m_chipSections.find(chipName);
        if (it == m_chipSections.end()) {
            return;
        }
        ChipSection &liveSection = it.value();
        m_chipExpanded[chipName] = expanded;
        liveSection.header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        liveSection.content->setVisible(expanded);
        emit chipExpandedStateChanged(m_chipExpanded);
    });

    chipLayout->addWidget(section.header);
    chipLayout->addWidget(section.content);
    m_chipExpanded.insert(chipName, section.header->isChecked());
    m_chipSections.insert(chipName, section);
    return &m_chipSections[chipName];
}

void SensorsPanel::rebuildChipSection(
    ChipSection &section,
    const QMap<SensorCategory, QVector<SensorReading> > &categories,
    const int columnsPerCategory
) {
    // Replace only this chip's subtree; keep the outer chip card/header instance alive.
    while (QLayoutItem *item = section.categoryRow->takeAt(0)) {
        if (item->widget() != nullptr) {
            delete item->widget();
        }
        delete item;
    }
    section.widgets.clear();

    QList<SensorCategory> orderedCategories = categories.keys();
    std::sort(orderedCategories.begin(), orderedCategories.end(), [](const SensorCategory a, const SensorCategory b) {
        return static_cast<int>(a) < static_cast<int>(b);
    });

    for (const SensorCategory categoryName: orderedCategories) {
        const QVector<SensorReading> &categoryReadings = categories.value(categoryName);
        const int usedColumns = std::max(1, std::min(columnsPerCategory, static_cast<int>(categoryReadings.size())));
        const int categoryWidth = widthForColumns(usedColumns);

        auto *categoryContainer = new QWidget(section.content);
        categoryContainer->setMinimumWidth(categoryWidth);
        categoryContainer->setMaximumWidth(categoryWidth);
        categoryContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        auto *categoryContainerLayout = new QVBoxLayout(categoryContainer);
        categoryContainerLayout->setContentsMargins(0, 0, 0, 0);
        categoryContainerLayout->setSpacing(AppTheme::kCategoryBlockSpacing);

        auto *categoryTitle = new QLabel(translatedCategoryName(categoryName) + QStringLiteral(":"), categoryContainer);
        QFont catFont = categoryTitle->font();
        catFont.setBold(true);
        categoryTitle->setFont(catFont);
        categoryContainerLayout->addWidget(categoryTitle);

        auto *categoryGrid = new QGridLayout();
        categoryGrid->setContentsMargins(0, 0, 0, 0);
        categoryGrid->setHorizontalSpacing(kUnifiedHorizontalSpacing);
        categoryGrid->setVerticalSpacing(AppTheme::kGridSpacing);
        categoryGrid->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        for (int i = 0; i < categoryReadings.size(); ++i) {
            const int row = i / columnsPerCategory;
            const int col = i % columnsPerCategory;
            auto *sensorWidget = new SensorValueWidget(categoryReadings[i], categoryContainer);
            categoryGrid->addWidget(sensorWidget, row, col, Qt::AlignLeft | Qt::AlignTop);
            section.widgets.insert(sensorKey(categoryReadings[i]), sensorWidget);
        }

        categoryGrid->setColumnStretch(columnsPerCategory, 1);
        categoryContainerLayout->addLayout(categoryGrid);
        categoryContainerLayout->addStretch(1);
        section.categoryRow->addWidget(categoryContainer, 0, Qt::AlignLeft | Qt::AlignTop);
    }

    section.categoryRow->addStretch(1);
}

void SensorsPanel::updateVisibleReadings() {
    for (const SensorReading &reading: m_readings) {
        if (SensorValueWidget *widget = m_sensorWidgets.value(sensorKey(reading), nullptr)) {
            widget->setReading(reading);
        }
    }
}

QString SensorsPanel::chipStructureFingerprint(const QMap<SensorCategory, QVector<SensorReading> > &categories) {
    QStringList entries;

    for (auto catIt = categories.cbegin(); catIt != categories.cend(); ++catIt) {
        for (const SensorReading &reading: catIt.value()) {
            entries.push_back(sensorKey(reading));
        }
    }

    entries.sort();
    return entries.join(QStringLiteral("\n"));
}

QString SensorsPanel::sensorKey(const SensorReading &reading) {
    return reading.chip + QStringLiteral("|")
           + QString::number(static_cast<int>(reading.category)) + QStringLiteral("|")
           + QString::number(reading.featureNumber) + QStringLiteral("|")
           + QString::number(reading.subfeatureNumber) + QStringLiteral("|")
           + QString::number(static_cast<int>(reading.unit)) + QStringLiteral("|")
           + reading.feature;
}
