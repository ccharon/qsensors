// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors_panel.h"

#include "sensor_identity.h"
#include "theme/app_theme.h"
#include "sensor_value_widget.h"
#include "collapsible_section.h"

#include <QCoreApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QStringList>
#include <QVBoxLayout>
#include <algorithm>

namespace {
    constexpr int kChipContentHorizontalMargins = AppTheme::kSectionInset * 2;

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
    for (auto it = m_chipSections.begin(); it != m_chipSections.end(); ++it) {
        it->card->setExpanded(state.value(it.key(), true));
        m_chipExpanded.insert(it.key(), it->card->isExpanded());
    }
}

QHash<QString, bool> SensorsPanel::chipExpandedState() const {
    return m_chipExpanded;
}

void SensorsPanel::setReadings(const QVector<SensorReading> &readings, const int viewportWidth) {
    m_readings = readings;
    renderReadings(viewportWidth);
}

void SensorsPanel::relayout(const int viewportWidth) {
    if (m_readings.isEmpty()) {
        return;
    }

    renderReadings(viewportWidth);
}

int SensorsPanel::minimumRequiredWidth() const {
    int maxCategoryCount = 1;
    for (auto chipIt = m_groupedCache.cbegin(); chipIt != m_groupedCache.cend(); ++chipIt) {
        maxCategoryCount = std::max(maxCategoryCount, static_cast<int>(chipIt.value().size()));
    }

    // Each category needs at least one card column.
    const int panelHorizontalMargins = m_layout->contentsMargins().left() + m_layout->contentsMargins().right();
    return panelHorizontalMargins + AppTheme::kChipCardFrameWidthTotal + kChipContentHorizontalMargins
           + widthForColumns(maxCategoryCount);
}

void SensorsPanel::renderReadings(const int viewportWidth) {
    m_groupedCache = groupReadingsByChip(m_readings);
    const QMap<QString, QMap<SensorCategory, QVector<SensorReading> > > &grouped = m_groupedCache;
    const int stableViewportWidth = computeStableViewportWidth(viewportWidth);

    // Batch updates avoid flicker while chip sections are reconciled/reordered.
    setUpdatesEnabled(false);

    const QStringList orderedChips = grouped.keys();
    removeStaleChipSections(grouped);

    for (const QString &chipName: orderedChips) {
        const QMap<SensorCategory, QVector<SensorReading> > &categories = grouped.value(chipName);
        reconcileChipSection(chipName, categories, stableViewportWidth);
    }

    applyChipOrder(orderedChips);

    m_sensorWidgets.clear();
    for (auto it = m_chipSections.constBegin(); it != m_chipSections.constEnd(); ++it) {
        for (auto wit = it->widgets.constBegin(); wit != it->widgets.constEnd(); ++wit) {
            m_sensorWidgets.insert(wit.key(), wit.value());
        }
    }
    updateVisibleReadings();
    setUpdatesEnabled(true);
    update();
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
            m_layout->removeWidget(it->card);
            delete it->card;
            it = m_chipSections.erase(it);
        } else {
            ++it;
        }
    }
}

int SensorsPanel::computeStableViewportWidth(const int viewportWidth) {
    return std::max(AppTheme::kMinStableViewportWidth, viewportWidth);
}

int SensorsPanel::widthForColumns(const int columns) {
    return (columns * AppTheme::kCardMinWidth) + ((columns - 1) * AppTheme::kUnifiedHorizontalSpacing);
}

void SensorsPanel::reconcileChipSection(
    const QString &chipName,
    const QMap<SensorCategory, QVector<SensorReading> > &categories,
    const int stableViewportWidth
) {
    const int categoryCount = std::max(1, static_cast<int>(categories.size()));
    const int perCategoryWidth = std::max(
        AppTheme::kCardMinWidth,
        (stableViewportWidth - kChipContentHorizontalMargins - ((categoryCount - 1) * AppTheme::kUnifiedHorizontalSpacing)) / categoryCount
    );
    const int columnsPerCategory = std::clamp(
        (perCategoryWidth + AppTheme::kUnifiedHorizontalSpacing) / (AppTheme::kCardMinWidth + AppTheme::kUnifiedHorizontalSpacing),
        1,
        AppTheme::kMaxColumnsPerCategory
    );

    auto it = m_chipSections.find(chipName);
    ChipSection *section = it != m_chipSections.end() ? &it.value() : createChipSection(chipName);

    // Rebuild only when sensors or the column count changed; resizes within the same
    // column count keep the existing widgets.
    const QString structure = chipStructureFingerprint(categories);
    if (section->structureFingerprint != structure || section->columnsPerCategory != columnsPerCategory) {
        rebuildChipSection(*section, categories, columnsPerCategory);
        section->structureFingerprint = structure;
        section->columnsPerCategory = columnsPerCategory;
    }
}

void SensorsPanel::applyChipOrder(const QStringList &orderedChips) {
    // Aktuelle Reihenfolge aus dem Layout lesen (Stretch-Items haben kein Widget).
    QStringList current;
    for (int i = 0; i < m_layout->count(); ++i) {
        if (QWidget *w = m_layout->itemAt(i)->widget())
            current << w->property("chipName").toString();
    }
    if (current == orderedChips)
        return;

    while (QLayoutItem *item = m_layout->takeAt(0)) {
        delete item;
    }
    for (const QString &chipName: orderedChips) {
        auto it = m_chipSections.constFind(chipName);
        if (it != m_chipSections.cend()) {
            m_layout->addWidget(it->card);
        }
    }
    m_layout->addStretch(1);
}

SensorsPanel::ChipSection *SensorsPanel::createChipSection(const QString &chipName) {
    ChipSection section{};
    section.card = new CollapsibleSection(chipName, m_chipExpanded.value(chipName, true), this);
    section.card->setProperty("chipName", chipName);
    section.categoryRow = new QHBoxLayout();
    section.categoryRow->setSpacing(AppTheme::kUnifiedHorizontalSpacing);
    section.card->contentLayout()->addLayout(section.categoryRow);

    connect(section.card, &CollapsibleSection::expandedChanged, this, [this, chipName](const bool expanded) {
        m_chipExpanded[chipName] = expanded;
    });

    m_chipExpanded.insert(chipName, section.card->isExpanded());
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

    for (const SensorCategory categoryName: categories.keys()) {
        const QVector<SensorReading> &categoryReadings = categories.value(categoryName);
        const int usedColumns = std::max(1, std::min(columnsPerCategory, static_cast<int>(categoryReadings.size())));
        const int categoryWidth = widthForColumns(usedColumns);

        auto *categoryContainer = new QWidget(section.card->content());
        categoryContainer->setFixedWidth(categoryWidth);
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
        categoryGrid->setHorizontalSpacing(AppTheme::kUnifiedHorizontalSpacing);
        categoryGrid->setVerticalSpacing(AppTheme::kGridSpacing);
        categoryGrid->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        for (int i = 0; i < categoryReadings.size(); ++i) {
            const int row = i / columnsPerCategory;
            const int col = i % columnsPerCategory;
            auto *sensorWidget = new SensorValueWidget(categoryReadings[i], categoryContainer);
            categoryGrid->addWidget(sensorWidget, row, col, Qt::AlignLeft | Qt::AlignTop);
            section.widgets.insert(SensorIdentity::sensorKey(categoryReadings[i]), sensorWidget);
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
        if (SensorValueWidget *widget = m_sensorWidgets.value(SensorIdentity::sensorKey(reading), nullptr)) {
            widget->setReading(reading);
        }
    }
}

QString SensorsPanel::chipStructureFingerprint(const QMap<SensorCategory, QVector<SensorReading> > &categories) {
    QStringList entries;

    for (auto catIt = categories.cbegin(); catIt != categories.cend(); ++catIt) {
        for (const SensorReading &reading: catIt.value()) {
            entries.push_back(SensorIdentity::sensorKey(reading));
        }
    }

    entries.sort();
    return entries.join(QStringLiteral("\n"));
}

