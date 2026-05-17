// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors_backend.h"

#include <QFrame>
#include <QHash>
#include <QString>
#include <QWidget>

class QHBoxLayout;
class QToolButton;
class QVBoxLayout;
class SensorValueWidget;

/** Dynamic chip/category panel that renders and updates sensor widgets. */
class SensorsPanel final : public QWidget {
    Q_OBJECT

public:
    explicit SensorsPanel(QWidget *parent = nullptr);

    /** Injects externally persisted chip expand/collapse state before rendering. */
    void setChipExpandedState(const QHash<QString, bool> &state);

    /** Returns current expand/collapse state captured from user interaction. */
    [[nodiscard]] QHash<QString, bool> chipExpandedState() const;

    /** Updates panel data and rebuilds widget tree only when sensor structure changed. */
    void setReadings(const QVector<SensorReading> &readings, int viewportWidth);

    /** Forces a layout rebuild for viewport width changes without changing values. */
    void relayout(int viewportWidth);

    /** Minimum width required so each category can still render at least one sensor column. */
    [[nodiscard]] int minimumRequiredWidth() const;

signals:
    void chipExpandedStateChanged(const QHash<QString, bool> &state);

private:
    /** One persistent UI section per chip, reused across refresh cycles. */
    struct ChipSection {
        QFrame *card = nullptr;
        QToolButton *header = nullptr;
        QWidget *content = nullptr;
        QHBoxLayout *categoryRow = nullptr;
        /** Fingerprint of category/feature layout currently rendered in this section. */
        QString structureFingerprint;
        /** Widget map for fast value-only updates without rebuilding chip content. */
        QHash<QString, SensorValueWidget *> widgets;
    };

    /** Reconciles chip sections and rebuilds only changed chip/category trees. */
    void renderReadings(int viewportWidth, bool forceRebuild);

    [[nodiscard]] static QMap<QString, QMap<SensorCategory, QVector<SensorReading> > > groupReadingsByChip(
        const QVector<SensorReading> &readings
    );

    void removeStaleChipSections(const QMap<QString, QMap<SensorCategory, QVector<SensorReading> > > &grouped);

    [[nodiscard]] static int computeStableViewportWidth(int viewportWidth);
    [[nodiscard]] static int widthForColumns(int columns);

    void reconcileChipSection(
        const QString &chipName,
        const QMap<SensorCategory, QVector<SensorReading> > &categories,
        int stableViewportWidth,
        bool forceRebuild,
        QHash<QString, SensorValueWidget *> &reconciledWidgets
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

    /** Unique widget lookup key for one sensor reading instance. */
    [[nodiscard]] static QString sensorKey(const SensorReading &reading);

    QVBoxLayout *m_layout;
    QVector<SensorReading> m_readings;
    QHash<QString, SensorValueWidget *> m_sensorWidgets;
    QHash<QString, bool> m_chipExpanded;
    QHash<QString, ChipSection> m_chipSections;
};
