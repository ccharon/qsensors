// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors_backend.h"

#include <QHash>
#include <QString>
#include <QWidget>

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

signals:
    void chipExpandedStateChanged(const QHash<QString, bool> &state);

private:
    /** Deletes all currently rendered chip/category widgets from the root layout. */
    void clearContent();

    /** Rebuilds full chip/category/widget UI based on current readings. */
    void renderReadings(int viewportWidth);

    /** Applies value updates to already rendered widgets without rebuilding layout. */
    void updateVisibleReadings();

    /** Stable key for structure-change detection (chip/category/feature/unit). */
    static QString structureFingerprint(const QVector<SensorReading> &readings);

    /** Unique widget lookup key for one sensor reading instance. */
    static QString sensorKey(const SensorReading &reading);

    QVBoxLayout *m_layout;
    QVector<SensorReading> m_readings;
    QString m_lastStructureFingerprint;
    QHash<QString, SensorValueWidget *> m_sensorWidgets;
    QHash<QString, bool> m_chipExpanded;
};
