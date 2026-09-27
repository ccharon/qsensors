// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors_panel.h"

#include "sensor_identity.h"
#include "theme/app_theme.h"
#include "sensor_value_widget.h"
#include "collapsible_section.h"

#include <QCoreApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
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

void SensorsPanel::restoreChipExpandedState(const QHash<QString, bool> &state, const QString &chipFingerprint) {
    setChipExpandedState(state);
    m_restoredFingerprint = chipFingerprint;
}

QHash<QString, bool> SensorsPanel::chipExpandedState() const {
    return m_chipExpanded;
}

QString SensorsPanel::chipFingerprint() const {
    return SensorIdentity::chipSetFingerprint(m_groups.keys());
}

int SensorsPanel::readingCount() const {
    int count = 0;
    for (const CategoryGroups &categories: m_groups)
        for (const QVector<SensorReading> &readings: categories)
            count += static_cast<int>(readings.size());
    return count;
}

void SensorsPanel::setReadings(const QVector<SensorReading> &readings, const int viewportWidth) {
    m_groups = groupReadingsByChip(readings);
    checkRestoredState();
    if (render(viewportWidth)) {
        emit structureChanged();
    }
}

void SensorsPanel::relayout(const int viewportWidth) {
    if (!m_groups.isEmpty()) {
        render(viewportWidth);
    }
}

int SensorsPanel::minimumRequiredWidth() const {
    int maxCategoryCount = 1;
    for (const CategoryGroups &categories: m_groups) {
        maxCategoryCount = std::max(maxCategoryCount, static_cast<int>(categories.size()));
    }

    // Each category needs at least one card column.
    const int panelHorizontalMargins = m_layout->contentsMargins().left() + m_layout->contentsMargins().right();
    return panelHorizontalMargins + AppTheme::kChipCardFrameWidthTotal + kChipContentHorizontalMargins
           + widthForColumns(maxCategoryCount);
}

void SensorsPanel::checkRestoredState() {
    if (m_restoredFingerprint.isEmpty()) {
        return;
    }
    const bool matches = m_restoredFingerprint == chipFingerprint();
    m_restoredFingerprint.clear();
    if (!matches) {
        setChipExpandedState({});
        emit layoutStateReset();
    }
}

bool SensorsPanel::render(const int viewportWidth) {
    // Batch updates avoid flicker while chip sections are reconciled/reordered.
    setUpdatesEnabled(false);
    bool structureChanged = removeStaleChipSections();
    for (auto it = m_groups.cbegin(); it != m_groups.cend(); ++it) {
        structureChanged |= reconcileChipSection(it.key(), it.value(), viewportWidth);
    }
    applyChipOrder();
    setUpdatesEnabled(true);
    return structureChanged;
}

bool SensorsPanel::removeStaleChipSections() {
    bool removed = false;
    for (auto it = m_chipSections.begin(); it != m_chipSections.end();) {
        if (!m_groups.contains(it.key())) {
            m_layout->removeWidget(it->card);
            delete it->card;
            it = m_chipSections.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    return removed;
}

bool SensorsPanel::reconcileChipSection(const QString &chipName, const CategoryGroups &categories, const int viewportWidth) {
    const int columnsPerCategory = columnsPerCategoryFor(static_cast<int>(categories.size()), viewportWidth);

    auto it = m_chipSections.find(chipName);
    ChipSection *section = it != m_chipSections.end() ? &it.value() : createChipSection(chipName);

    // Rebuild only when sensors or the column count changed; resizes within the same
    // column count keep the existing widgets.
    const QString structure = chipStructureFingerprint(categories);
    const bool sensorsChanged = section->structureFingerprint != structure;
    if (sensorsChanged || section->columnsPerCategory != columnsPerCategory) {
        rebuildChipSection(*section, categories, columnsPerCategory);
        section->structureFingerprint = structure;
        section->columnsPerCategory = columnsPerCategory;
    }

    for (const QVector<SensorReading> &readings: categories) {
        for (const SensorReading &reading: readings) {
            if (SensorValueWidget *widget = section->widgets.value(SensorIdentity::sensorKey(reading)))
                widget->setReading(reading);
        }
    }
    return sensorsChanged;
}

void SensorsPanel::applyChipOrder() {
    const QStringList order = m_groups.keys();
    if (order == m_chipOrder)
        return;

    while (QLayoutItem *item = m_layout->takeAt(0)) {
        delete item;
    }
    for (const QString &chipName: order) {
        m_layout->addWidget(m_chipSections.value(chipName).card);
    }
    m_layout->addStretch(1);
    m_chipOrder = order;
}

SensorsPanel::ChipSection *SensorsPanel::createChipSection(const QString &chipName) {
    ChipSection section{};
    section.card = new CollapsibleSection(chipName, m_chipExpanded.value(chipName, true), this);
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

void SensorsPanel::rebuildChipSection(ChipSection &section, const CategoryGroups &categories,
                                      const int columnsPerCategory) {
    // Replace only this chip's subtree; keep the outer chip card/header instance alive.
    while (QLayoutItem *item = section.categoryRow->takeAt(0)) {
        if (item->widget() != nullptr) {
            delete item->widget();
        }
        delete item;
    }
    section.widgets.clear();

    for (auto categoryIt = categories.cbegin(); categoryIt != categories.cend(); ++categoryIt) {
        const SensorCategory categoryName = categoryIt.key();
        const QVector<SensorReading> &categoryReadings = categoryIt.value();
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

SensorsPanel::ChipGroups SensorsPanel::groupReadingsByChip(const QVector<SensorReading> &readings) {
    ChipGroups grouped;
    for (const SensorReading &r: readings) {
        grouped[r.chip][r.category].push_back(r);
    }
    return grouped;
}

QString SensorsPanel::chipStructureFingerprint(const CategoryGroups &categories) {
    QStringList entries;
    for (const QVector<SensorReading> &readings: categories) {
        for (const SensorReading &reading: readings) {
            entries.push_back(SensorIdentity::sensorKey(reading));
        }
    }
    entries.sort();
    return entries.join(QLatin1Char('\n'));
}

int SensorsPanel::columnsPerCategoryFor(const int categoryCount, const int viewportWidth) {
    // Lower bound keeps the calculation stable while the viewport is not laid out yet.
    const int stableWidth = std::max(AppTheme::kMinStableViewportWidth, viewportWidth);
    const int categories = std::max(1, categoryCount);
    const int perCategoryWidth = std::max(
        AppTheme::kCardMinWidth,
        (stableWidth - kChipContentHorizontalMargins - ((categories - 1) * AppTheme::kUnifiedHorizontalSpacing)) / categories
    );
    return std::clamp(
        (perCategoryWidth + AppTheme::kUnifiedHorizontalSpacing) / (AppTheme::kCardMinWidth + AppTheme::kUnifiedHorizontalSpacing),
        1,
        AppTheme::kMaxColumnsPerCategory
    );
}

int SensorsPanel::widthForColumns(const int columns) {
    return (columns * AppTheme::kCardMinWidth) + ((columns - 1) * AppTheme::kUnifiedHorizontalSpacing);
}
