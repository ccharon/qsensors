// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors_backend.h"

#include <QHash>
#include <QString>
#include <QWidget>

class QVBoxLayout;
class SensorValueWidget;

// Dynamic chip/category panel that renders and updates sensor widgets.
class SensorsPanel final : public QWidget {
    Q_OBJECT

public:
    explicit SensorsPanel(QWidget *parent = nullptr);

    void setChipExpandedState(const QHash<QString, bool> &state);
    [[nodiscard]] QHash<QString, bool> chipExpandedState() const;

    void setReadings(const QVector<SensorReading> &readings, int viewportWidth);
    void relayout(int viewportWidth);

signals:
    void chipExpandedStateChanged(const QHash<QString, bool> &state);

private:
    void clearContent();
    void renderReadings(int viewportWidth);
    void updateVisibleReadings();
    static QString structureFingerprint(const QVector<SensorReading> &readings);
    static QString sensorKey(const SensorReading &reading);

    QVBoxLayout *m_layout;
    QVector<SensorReading> m_readings;
    QString m_lastStructureFingerprint;
    QHash<QString, SensorValueWidget *> m_sensorWidgets;
    QHash<QString, bool> m_chipExpanded;
};
