// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors/sensor_reading.h"
#include "ui/theme/app_theme.h"

#include <QHash>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class CollapsibleSection;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QHBoxLayout;
class QVBoxLayout;
class SensorValueWidget;

/** Chip-grouped sensor cards; owns the per-chip expand state. */
class SensorsPanel final : public QWidget {
    Q_OBJECT

public:
    /** Creates an empty panel; content appears with the first setReadings(). */
    explicit SensorsPanel(QWidget *parent = nullptr);

    /** Replaces the expand/collapse state per chip; existing sections follow immediately. */
    void setChipExpandedState(const QHash<QString, bool> &state);

    /**
     * Applies a persisted expand state that was saved for @p chipFingerprint. If the
     * first readings show a different chip set, the state is dropped and
     * layoutStateReset() is emitted.
     */
    void restoreChipExpandedState(const QHash<QString, bool> &state, const QString &chipFingerprint);

    /** Expand/collapse state per chip name, for persistence. */
    [[nodiscard]] QHash<QString, bool> chipExpandedState() const;

    /**
     * Preferred chip order, e.g. restored from settings. Listed chips come first in
     * this order; chips not listed follow alphabetically.
     */
    void setChipOrder(const QStringList &order);

    /** Current chip order for persistence; includes preferred chips not present right now. */
    [[nodiscard]] QStringList chipOrder() const;

    /** Moves @p chip to position @p targetIndex among the shown chips (insert position before the move). */
    void moveChip(const QString &chip, int targetIndex);

    /** Fingerprint of the chips currently shown (see SensorIdentity::chipFingerprint). */
    [[nodiscard]] QString chipFingerprint() const;

    /** Number of readings currently shown. */
    [[nodiscard]] int readingCount() const;

    /** Shows @p readings; rebuilds only sections whose sensors or column count changed. */
    void setReadings(const QVector<SensorReading> &readings, int viewportWidth);

    /** Re-evaluates the column layout for a new viewport width; unchanged sections are kept. */
    void relayout(int viewportWidth);

    /** Minimum width required so each category can still render at least one sensor column. */
    [[nodiscard]] int minimumRequiredWidth() const;

    /**
     * Columns per category for @p sensorCounts within @p availableWidth. Every category
     * gets one column; each further column goes to the category with the most rows, as
     * long as it saves a row and fits.
     */
    [[nodiscard]] static QVector<int> columnsForCategories(const QVector<int> &sensorCounts, int availableWidth);

    /** Width of categories laid out with @p columns at minimum card width, including the gaps. */
    [[nodiscard]] static int categoriesWidth(const QVector<int> &columns);

    /**
     * Card width that spreads the width left over by @p columns within @p availableWidth
     * evenly over all cards, between kCardMinWidth and kCardMaxWidth.
     */
    [[nodiscard]] static int cardWidthFor(const QVector<int> &columns, int availableWidth);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

signals:
    /** Chips or categories changed, so minimumRequiredWidth() may have changed. */
    void structureChanged();

    /** A restored expand state did not match the current chips and was dropped. */
    void layoutStateReset();

private:
    using CategoryGroups = QMap<SensorCategory, QVector<SensorReading>>;
    using ChipGroups = QMap<QString, CategoryGroups>;

    /** One persistent UI section per chip, reused across refresh cycles. */
    struct ChipSection {
        CollapsibleSection *card = nullptr;
        QHBoxLayout *categoryRow = nullptr;
        /** Sensor keys currently rendered; a change requires a rebuild. */
        QString structureFingerprint;
        /** Grid columns per category (in category order) the section was built with. */
        QVector<int> columns;
        /** One container per category, in category order; sized by applyCardWidth(). */
        QVector<QWidget *> categoryContainers;
        /** Card width currently applied. */
        int cardWidth = 0;
        /** Cards by sensor key, for value-only updates. */
        QHash<QString, SensorValueWidget *> widgets;
    };

    /** Reconciles all sections with m_groups; returns true if the structure changed. */
    bool render(int viewportWidth);

    /** Drops the pending restored state when it belongs to another chip set. */
    void checkRestoredState();

    /** Deletes sections of chips that are no longer present; returns true if any was removed. */
    bool removeStaleChipSections();

    /** Creates, rebuilds or updates one section with the given layout; returns true if its sensors changed. */
    bool reconcileChipSection(const QString &chipName, const CategoryGroups &categories, const QVector<int> &columns,
                              int cardWidth);

    /** Creates and wires one reusable chip section container. */
    [[nodiscard]] ChipSection *createChipSection(const QString &chipName);

    /** Rebuilds one chip section's category/widget subtree. */
    void rebuildChipSection(ChipSection &section, const CategoryGroups &categories, const QVector<int> &columns);

    /** Present chips in display order: preferred order first, then the rest alphabetically. */
    [[nodiscard]] QStringList displayOrder() const;

    /** Sizes all cards and category containers of @p section for @p cardWidth. */
    static void applyCardWidth(ChipSection &section, int cardWidth);

    /** Puts the chip cards into the layout in displayOrder() when it differs. */
    void applyChipOrder();

    /** Starts dragging the card of @p chip; the drop reorders via moveChip(). */
    void startChipDrag(const QString &chip);

    /** Insert position among the shown chips for a drop at @p y. */
    [[nodiscard]] int dropIndexAt(int y) const;

    /** Shows the insert line before position @p index. */
    void showDropIndicator(int index);

    [[nodiscard]] static ChipGroups groupReadingsByChip(const QVector<SensorReading> &readings);
    [[nodiscard]] static QString chipStructureFingerprint(const CategoryGroups &categories);
    [[nodiscard]] static QVector<int> sensorCounts(const CategoryGroups &categories);
    [[nodiscard]] static int widthForColumns(int columns, int cardWidth = AppTheme::kCardMinWidth);

    /** Width around the categories of a chip: panel margins, card frame and chip content margins. */
    [[nodiscard]] int chipChromeWidth() const;

    QVBoxLayout *m_layout;
    ChipGroups m_groups;
    QHash<QString, ChipSection> m_chipSections;
    QStringList m_chipOrder; // order currently in the layout
    QStringList m_preferredOrder;
    QWidget *m_dropIndicator;
    QHash<QString, bool> m_chipExpanded;
    // Set by restoreChipExpandedState() until the first readings confirm or reject it.
    QString m_restoredFingerprint;
};
