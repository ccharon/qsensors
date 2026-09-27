// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors_panel.h"

#include "sensor_identity.h"
#include "theme/app_theme.h"
#include "sensor_value_widget.h"
#include "collapsible_section.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QPainter>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <algorithm>

namespace {
    constexpr int kChipContentHorizontalMargins = AppTheme::kSectionInset * 2;
    const QString kChipMimeType = QStringLiteral("application/x-qsensors-chip");
    constexpr int kDropIndicatorHeight = 3;
    constexpr qreal kDragPixmapOpacity = 0.7;

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

SensorsPanel::SensorsPanel(QWidget *parent)
    : QWidget(parent), m_layout(new QVBoxLayout(this)), m_dropIndicator(new QWidget(this)) {
    m_layout->setContentsMargins(AppTheme::kSectionInset, AppTheme::kSectionInset, AppTheme::kSectionInset, AppTheme::kSectionInset);
    m_layout->setSpacing(AppTheme::kSensorsPanelVerticalSpacing);
    m_layout->addStretch(1);

    setAcceptDrops(true);
    m_dropIndicator->setAutoFillBackground(true);
    m_dropIndicator->setBackgroundRole(QPalette::Highlight);
    m_dropIndicator->hide();
}

void SensorsPanel::setChipOrder(const QStringList &order) {
    m_preferredOrder = order;
    m_preferredOrder.removeDuplicates();
    applyChipOrder();
}

QStringList SensorsPanel::chipOrder() const {
    QStringList order = displayOrder();
    for (const QString &chip: m_preferredOrder) {
        if (!order.contains(chip))
            order.append(chip);
    }
    return order;
}

void SensorsPanel::moveChip(const QString &chip, int targetIndex) {
    QStringList order = displayOrder();
    const int from = static_cast<int>(order.indexOf(chip));
    if (from < 0)
        return;
    targetIndex = std::clamp(targetIndex, 0, static_cast<int>(order.size()));
    order.removeAt(from);
    if (targetIndex > from)
        --targetIndex;
    order.insert(targetIndex, chip);

    // Absent chips keep their preference so they return to their place.
    for (const QString &known: std::as_const(m_preferredOrder)) {
        if (!order.contains(known))
            order.append(known);
    }
    m_preferredOrder = order;
    applyChipOrder();
}

QStringList SensorsPanel::displayOrder() const {
    QStringList order;
    for (const QString &chip: m_preferredOrder) {
        if (m_groups.contains(chip))
            order.append(chip);
    }
    for (auto it = m_groups.cbegin(); it != m_groups.cend(); ++it) {
        if (!order.contains(it.key()))
            order.append(it.key());
    }
    return order;
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
    return chipChromeWidth() + categoriesWidth(QVector<int>(maxCategoryCount, 1));
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

    // Columns are planned per chip; the card width is shared, so the chip with the
    // least spare width sets it for all cards.
    const int available = viewportWidth - chipChromeWidth();
    QHash<QString, QVector<int>> columnsByChip;
    int cardWidth = AppTheme::kCardMaxWidth;
    for (auto it = m_groups.cbegin(); it != m_groups.cend(); ++it) {
        const QVector<int> columns = columnsForCategories(sensorCounts(it.value()), available);
        columnsByChip.insert(it.key(), columns);
        cardWidth = std::min(cardWidth, cardWidthFor(columns, available));
    }
    for (auto it = m_groups.cbegin(); it != m_groups.cend(); ++it) {
        structureChanged |= reconcileChipSection(it.key(), it.value(), columnsByChip.value(it.key()), cardWidth);
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

bool SensorsPanel::reconcileChipSection(const QString &chipName, const CategoryGroups &categories,
                                        const QVector<int> &columns, const int cardWidth) {
    auto it = m_chipSections.find(chipName);
    ChipSection *section = it != m_chipSections.end() ? &it.value() : createChipSection(chipName);

    const QString structure = chipStructureFingerprint(categories);
    const bool sensorsChanged = section->structureFingerprint != structure;
    // Rebuild only when sensors or column counts change; a new card width while
    // resizing just resizes the existing widgets.
    const bool rebuild = sensorsChanged || section->columns != columns;
    if (rebuild) {
        rebuildChipSection(*section, categories, columns);
        section->structureFingerprint = structure;
        section->columns = columns;
    }
    if (rebuild || section->cardWidth != cardWidth) {
        applyCardWidth(*section, cardWidth);
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
    const QStringList order = displayOrder();
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
    section.card->setDraggable(true);
    connect(section.card, &CollapsibleSection::dragRequested, this, [this, chipName] { startChipDrag(chipName); });

    m_chipExpanded.insert(chipName, section.card->isExpanded());
    m_chipSections.insert(chipName, section);
    return &m_chipSections[chipName];
}

void SensorsPanel::rebuildChipSection(ChipSection &section, const CategoryGroups &categories,
                                      const QVector<int> &columns) {
    // Replace only this chip's subtree; keep the outer chip card/header instance alive.
    while (QLayoutItem *item = section.categoryRow->takeAt(0)) {
        if (item->widget() != nullptr) {
            delete item->widget();
        }
        delete item;
    }
    section.widgets.clear();
    section.categoryContainers.clear();

    int categoryIndex = 0;
    for (auto categoryIt = categories.cbegin(); categoryIt != categories.cend(); ++categoryIt, ++categoryIndex) {
        const SensorCategory categoryName = categoryIt.key();
        const QVector<SensorReading> &categoryReadings = categoryIt.value();
        const int columnsPerCategory = columns.value(categoryIndex, 1);

        auto *categoryContainer = new QWidget(section.card->content());
        section.categoryContainers.append(categoryContainer);
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

QVector<int> SensorsPanel::sensorCounts(const CategoryGroups &categories) {
    QVector<int> counts;
    counts.reserve(categories.size());
    for (const QVector<SensorReading> &readings: categories)
        counts.append(static_cast<int>(readings.size()));
    return counts;
}

QVector<int> SensorsPanel::columnsForCategories(const QVector<int> &sensorCounts, const int availableWidth) {
    QVector<int> columns(sensorCounts.size(), 1);
    const int pitch = AppTheme::kCardMinWidth + AppTheme::kUnifiedHorizontalSpacing;
    int used = categoriesWidth(columns);
    const auto rows = [](const int count, const int cols) { return (count + cols - 1) / cols; };

    while (used + pitch <= availableWidth) {
        int best = -1;
        int bestRows = 0;
        for (int i = 0; i < sensorCounts.size(); ++i) {
            const int count = std::max(1, sensorCounts.at(i));
            const bool savesRow = rows(count, columns.at(i) + 1) < rows(count, columns.at(i));
            if (savesRow && columns.at(i) < AppTheme::kMaxColumnsPerCategory && rows(count, columns.at(i)) > bestRows) {
                best = i;
                bestRows = rows(count, columns.at(i));
            }
        }
        if (best < 0)
            break;
        ++columns[best];
        used += pitch;
    }
    return columns;
}

int SensorsPanel::categoriesWidth(const QVector<int> &columns) {
    int width = 0;
    for (const int cols: columns)
        width += widthForColumns(cols);
    return width + std::max<int>(0, static_cast<int>(columns.size()) - 1) * AppTheme::kUnifiedHorizontalSpacing;
}

int SensorsPanel::chipChromeWidth() const {
    const int panelHorizontalMargins = m_layout->contentsMargins().left() + m_layout->contentsMargins().right();
    return panelHorizontalMargins + AppTheme::kChipCardFrameWidthTotal + kChipContentHorizontalMargins;
}

int SensorsPanel::widthForColumns(const int columns, const int cardWidth) {
    return (columns * cardWidth) + ((columns - 1) * AppTheme::kUnifiedHorizontalSpacing);
}

int SensorsPanel::cardWidthFor(const QVector<int> &columns, const int availableWidth) {
    int totalColumns = 0;
    for (const int cols: columns)
        totalColumns += cols;
    const int spare = availableWidth - categoriesWidth(columns);
    if (totalColumns == 0 || spare <= 0)
        return AppTheme::kCardMinWidth;
    return std::min(AppTheme::kCardMinWidth + spare / totalColumns, AppTheme::kCardMaxWidth);
}

void SensorsPanel::applyCardWidth(ChipSection &section, const int cardWidth) {
    for (int i = 0; i < section.categoryContainers.size(); ++i) {
        section.categoryContainers.at(i)->setFixedWidth(widthForColumns(section.columns.value(i, 1), cardWidth));
    }
    for (SensorValueWidget *card: std::as_const(section.widgets)) {
        card->setFixedWidth(cardWidth);
    }
    section.cardWidth = cardWidth;
}

void SensorsPanel::startChipDrag(const QString &chip) {
    const auto it = m_chipSections.constFind(chip);
    if (it == m_chipSections.cend())
        return;

    // Semi-transparent snapshot of the card follows the cursor.
    const QPixmap snapshot = it->card->grab();
    QPixmap pixmap(snapshot.size());
    pixmap.setDevicePixelRatio(snapshot.devicePixelRatio());
    pixmap.fill(Qt::transparent);
    {
        QPainter painter(&pixmap);
        painter.setOpacity(kDragPixmapOpacity);
        painter.drawPixmap(0, 0, snapshot);
    }

    auto *mime = new QMimeData;
    mime->setData(kChipMimeType, chip.toUtf8());
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->setPixmap(pixmap);
    drag->setHotSpot(it->card->mapFromGlobal(QCursor::pos()));
    drag->exec(Qt::MoveAction);
    m_dropIndicator->hide();
}

int SensorsPanel::dropIndexAt(const int y) const {
    for (int i = 0; i < m_chipOrder.size(); ++i) {
        if (y < m_chipSections.value(m_chipOrder.at(i)).card->geometry().center().y())
            return i;
    }
    return static_cast<int>(m_chipOrder.size());
}

void SensorsPanel::showDropIndicator(const int index) {
    if (m_chipOrder.isEmpty())
        return;
    // Centered in the gap before the card at index, or below the last card.
    const int gap = m_layout->spacing();
    const QRect card = index < m_chipOrder.size()
                           ? m_chipSections.value(m_chipOrder.at(index)).card->geometry()
                           : m_chipSections.value(m_chipOrder.last()).card->geometry();
    const int y = index < m_chipOrder.size() ? card.top() - (gap + kDropIndicatorHeight) / 2
                                             : card.bottom() + (gap - kDropIndicatorHeight) / 2;
    m_dropIndicator->setGeometry(card.left(), y, card.width(), kDropIndicatorHeight);
    m_dropIndicator->raise();
    m_dropIndicator->show();
}

void SensorsPanel::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasFormat(kChipMimeType))
        event->acceptProposedAction();
}

void SensorsPanel::dragMoveEvent(QDragMoveEvent *event) {
    if (!event->mimeData()->hasFormat(kChipMimeType))
        return;
    showDropIndicator(dropIndexAt(event->position().toPoint().y()));
    event->acceptProposedAction();
}

void SensorsPanel::dragLeaveEvent(QDragLeaveEvent *event) {
    Q_UNUSED(event);
    m_dropIndicator->hide();
}

void SensorsPanel::dropEvent(QDropEvent *event) {
    m_dropIndicator->hide();
    if (!event->mimeData()->hasFormat(kChipMimeType))
        return;
    const QString chip = QString::fromUtf8(event->mimeData()->data(kChipMimeType));
    moveChip(chip, dropIndexAt(event->position().toPoint().y()));
    event->acceptProposedAction();
}
