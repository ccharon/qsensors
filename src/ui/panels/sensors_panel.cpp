// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/panels/sensors_panel.h"

#include "ui/panels/category_row_layout.h"

#include "sensors/sensor_identity.h"
#include "ui/theme/app_theme.h"
#include "ui/widgets/sensor_value_widget.h"
#include "ui/widgets/collapsible_section.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QPainter>
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

void SensorsPanel::setReadings(const QVector<SensorReading> &readings) {
    m_groups = groupReadingsByChip(readings);
    checkRestoredState();
    if (render()) {
        emit structureChanged();
    }
}

QSize SensorsPanel::minimumSizeHint() const {
    return {chipChromeWidth() + m_plan.minimumWidth(), QWidget::minimumSizeHint().height()};
}

int SensorsPanel::maximumUsefulWidth() const {
    return m_groups.isEmpty() ? QWIDGETSIZE_MAX : chipChromeWidth() + m_plan.maximumWidth();
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

bool SensorsPanel::render() {
    // Batch updates avoid flicker while chip sections are reconciled/reordered.
    setUpdatesEnabled(false);
    bool structureChanged = removeStaleChipSections();

    // The plan is shared by all chips (one card width), so every chip follows a change.
    QHash<QString, QVector<int>> counts;
    for (auto it = m_groups.cbegin(); it != m_groups.cend(); ++it)
        counts.insert(it.key(), sensorCounts(it.value()));
    if (m_plan.setChips(counts)) {
        for (const ChipSection &section: std::as_const(m_chipSections))
            section.grid->invalidate();
        updateGeometry();
    }

    for (auto it = m_groups.cbegin(); it != m_groups.cend(); ++it) {
        structureChanged |= reconcileChipSection(it.key(), it.value());
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

bool SensorsPanel::reconcileChipSection(const QString &chipName, const CategoryGroups &categories) {
    auto it = m_chipSections.find(chipName);
    ChipSection *section = it != m_chipSections.end() ? &it.value() : createChipSection(chipName);

    const QString structure = chipStructureFingerprint(categories);
    const bool sensorsChanged = section->structureFingerprint != structure;
    if (sensorsChanged) {
        rebuildChipSection(*section, categories);
        section->structureFingerprint = structure;
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
    section.grid = new CategoryRowLayout(&m_plan, chipName);
    section.card->contentLayout()->addLayout(section.grid);

    connect(section.card, &CollapsibleSection::expandedChanged, this, [this, chipName](const bool expanded) {
        m_chipExpanded[chipName] = expanded;
    });
    section.card->setDraggable(true);
    connect(section.card, &CollapsibleSection::dragRequested, this, [this, chipName] { startChipDrag(chipName); });

    m_chipExpanded.insert(chipName, section.card->isExpanded());
    m_chipSections.insert(chipName, section);
    return &m_chipSections[chipName];
}

void SensorsPanel::rebuildChipSection(ChipSection &section, const CategoryGroups &categories) {
    // Replace only this chip's titles and cards; keep the outer chip card/header instance alive.
    section.grid->clear();
    section.widgets.clear();

    QWidget *parent = section.card->content();
    for (auto categoryIt = categories.cbegin(); categoryIt != categories.cend(); ++categoryIt) {
        auto *categoryTitle = new QLabel(translatedCategoryName(categoryIt.key()) + QStringLiteral(":"), parent);
        QFont catFont = categoryTitle->font();
        catFont.setBold(true);
        categoryTitle->setFont(catFont);

        QList<QWidget *> cards;
        for (const SensorReading &reading: categoryIt.value()) {
            auto *sensorWidget = new SensorValueWidget(reading, parent);
            cards.append(sensorWidget);
            section.widgets.insert(SensorIdentity::sensorKey(reading), sensorWidget);
        }
        section.grid->addCategory(categoryTitle, cards);
    }
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

int SensorsPanel::chipChromeWidth() const {
    const int panelHorizontalMargins = m_layout->contentsMargins().left() + m_layout->contentsMargins().right();
    return panelHorizontalMargins + AppTheme::kChipCardFrameWidthTotal + kChipContentHorizontalMargins;
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
