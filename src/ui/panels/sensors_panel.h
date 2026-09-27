// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensor_reading.h"

#include <QHash>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class CollapsibleSection;
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
        /** Grid columns per category the section was built with. */
        int columnsPerCategory = 0;
        /** Cards by sensor key, for value-only updates. */
        QHash<QString, SensorValueWidget *> widgets;
    };

    /** Reconciles all sections with m_groups; returns true if the structure changed. */
    bool render(int viewportWidth);

    /** Drops the pending restored state when it belongs to another chip set. */
    void checkRestoredState();

    /** Deletes sections of chips that are no longer present; returns true if any was removed. */
    bool removeStaleChipSections();

    /** Creates, rebuilds or updates one section; returns true if its sensors changed. */
    bool reconcileChipSection(const QString &chipName, const CategoryGroups &categories, int viewportWidth);

    /** Creates and wires one reusable chip section container. */
    [[nodiscard]] ChipSection *createChipSection(const QString &chipName);

    /** Rebuilds one chip section's category/widget subtree. */
    void rebuildChipSection(ChipSection &section, const CategoryGroups &categories, int columnsPerCategory);

    /** Puts the chip cards into the layout in m_groups order when it differs. */
    void applyChipOrder();

    [[nodiscard]] static ChipGroups groupReadingsByChip(const QVector<SensorReading> &readings);
    [[nodiscard]] static QString chipStructureFingerprint(const CategoryGroups &categories);
    [[nodiscard]] static int columnsPerCategoryFor(int categoryCount, int viewportWidth);
    [[nodiscard]] static int widthForColumns(int columns);

    QVBoxLayout *m_layout;
    ChipGroups m_groups;
    QHash<QString, ChipSection> m_chipSections;
    QStringList m_chipOrder;
    QHash<QString, bool> m_chipExpanded;
    // Set by restoreChipExpandedState() until the first readings confirm or reject it.
    QString m_restoredFingerprint;
};
