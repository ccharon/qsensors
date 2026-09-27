// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensor_reading.h"

#include <QFrame>
#include <QHash>
#include <QMap>
#include <QString>
#include <QVector>
#include <QWidget>

class CollapsibleSection;
class QHBoxLayout;
class QVBoxLayout;
class SensorValueWidget;

/** Dynamic chip/category panel that renders and updates sensor widgets. */
class SensorsPanel final : public QWidget {
    Q_OBJECT

public:
    /** Creates an empty panel; content appears with the first setReadings(). */
    explicit SensorsPanel(QWidget *parent = nullptr);

    /** Replaces the expand/collapse state per chip; existing sections follow immediately. */
    void setChipExpandedState(const QHash<QString, bool> &state);

    /** Expand/collapse state per chip name, for persistence. */
    [[nodiscard]] QHash<QString, bool> chipExpandedState() const;

    /** Updates panel data and rebuilds widget tree only when sensor structure changed. */
    void setReadings(const QVector<SensorReading> &readings, int viewportWidth);

    /** Re-evaluates the column layout for a new viewport width; unchanged sections are kept. */
    void relayout(int viewportWidth);

    /** Minimum width required so each category can still render at least one sensor column. */
    [[nodiscard]] int minimumRequiredWidth() const;

private:
    /** One persistent UI section per chip, reused across refresh cycles. */
    struct ChipSection {
        CollapsibleSection *card = nullptr;
        QHBoxLayout *categoryRow = nullptr;
        /** Fingerprint of category/feature layout currently rendered in this section. */
        QString structureFingerprint;
        /** Grid columns per category the section was built with. */
        int columnsPerCategory = 0;
        /** Widget map for fast value-only updates without rebuilding chip content. */
        QHash<QString, SensorValueWidget *> widgets;
    };

    /** Reconciles chip sections and rebuilds only changed chip/category trees. */
    void renderReadings(int viewportWidth);

    [[nodiscard]] static QMap<QString, QMap<SensorCategory, QVector<SensorReading> > > groupReadingsByChip(
        const QVector<SensorReading> &readings
    );

    void removeStaleChipSections(const QMap<QString, QMap<SensorCategory, QVector<SensorReading> > > &grouped);

    [[nodiscard]] static int computeStableViewportWidth(int viewportWidth);
    [[nodiscard]] static int widthForColumns(int columns);

    void reconcileChipSection(
        const QString &chipName,
        const QMap<SensorCategory, QVector<SensorReading> > &categories,
        int stableViewportWidth
    );

    /** Creates and wires one reusable chip section container. */
    [[nodiscard]] ChipSection *createChipSection(const QString &chipName);

    /** Rebuilds one chip section's category/widget subtree. */
    void rebuildChipSection(ChipSection &section, const QMap<SensorCategory, QVector<SensorReading> > &categories, int columnsPerCategory);

    /** Fingerprint for one chip's structural content. */
    [[nodiscard]] static QString chipStructureFingerprint(const QMap<SensorCategory, QVector<SensorReading> > &categories);

    /** Reorders chip cards in layout to match current chip ordering. */
    void applyChipOrder(const QStringList &orderedChips);

    /** Applies value updates to already rendered widgets without rebuilding layout. */
    void updateVisibleReadings();

    QVBoxLayout *m_layout;
    QVector<SensorReading> m_readings;
    QHash<QString, SensorValueWidget *> m_sensorWidgets;
    QHash<QString, bool> m_chipExpanded;
    QHash<QString, ChipSection> m_chipSections;
    // Cached grouping reused by minimumRequiredWidth(); avoids recomputing per tick.
    QMap<QString, QMap<SensorCategory, QVector<SensorReading>>> m_groupedCache;
};
