// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors/sensor_reading.h"
#include "ui/panels/card_grid_plan.h"

#include <QHash>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class CategoryRowLayout;
class CollapsibleSection;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QVBoxLayout;
class SensorValueWidget;

/**
 * Chip-grouped sensor cards; owns the per-chip expand state and chip order. The
 * columns follow the panel width through each chip's CategoryRowLayout, so a resize
 * never rebuilds widgets; only a change of the sensors does.
 */
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

    /** Shows @p readings; rebuilds only sections whose sensors changed. */
    void setReadings(const QVector<SensorReading> &readings);

    /**
     * Minimum width at which every chip still gets one column per category. Chip
     * headers do not count; a long chip name is cut off rather than widening the window.
     */
    [[nodiscard]] QSize minimumSizeHint() const override;

    /**
     * Widest useful panel width (see CardGridPlan::maximumWidth()), collapsed chips
     * included; QWIDGETSIZE_MAX without chips. Not a widget constraint: a wider panel
     * (e.g. maximized) still stretches its chip frames.
     */
    [[nodiscard]] int maximumUsefulWidth() const;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

signals:
    /** Chips or sensors changed, as opposed to new values of the same sensors. */
    void structureChanged();

    /** A restored expand state did not match the current chips and was dropped. */
    void layoutStateReset();

private:
    using CategoryGroups = QMap<SensorCategory, QVector<SensorReading>>;
    using ChipGroups = QMap<QString, CategoryGroups>;

    /** One persistent UI section per chip, reused across refresh cycles. */
    struct ChipSection {
        CollapsibleSection *card = nullptr;
        CategoryRowLayout *grid = nullptr;
        /** Sensor keys currently rendered; a change requires a rebuild. */
        QString structureFingerprint;
        /** Cards by sensor key, for value-only updates. */
        QHash<QString, SensorValueWidget *> widgets;
    };

    /** Reconciles all sections with m_groups; returns true if the structure changed. */
    bool render();

    /** Drops the pending restored state when it belongs to another chip set. */
    void checkRestoredState();

    /** Deletes sections of chips that are no longer present; returns true if any was removed. */
    bool removeStaleChipSections();

    /** Creates, rebuilds or updates one section; returns true if its sensors changed. */
    bool reconcileChipSection(const QString &chipName, const CategoryGroups &categories);

    /** Creates and wires one reusable chip section container. */
    [[nodiscard]] ChipSection *createChipSection(const QString &chipName);

    /** Rebuilds one chip section's category titles and cards. */
    void rebuildChipSection(ChipSection &section, const CategoryGroups &categories);

    /** Present chips in display order: preferred order first, then the rest alphabetically. */
    [[nodiscard]] QStringList displayOrder() const;

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

    /** Width around the categories of a chip: panel margins, card frame and chip content margins. */
    [[nodiscard]] int chipChromeWidth() const;

    QVBoxLayout *m_layout;
    ChipGroups m_groups;
    CardGridPlan m_plan;
    QHash<QString, ChipSection> m_chipSections;
    QStringList m_chipOrder; // order currently in the layout
    QStringList m_preferredOrder;
    QWidget *m_dropIndicator;
    QHash<QString, bool> m_chipExpanded;
    // Set by restoreChipExpandedState() until the first readings confirm or reject it.
    QString m_restoredFingerprint;
};
