// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors_backend.h"

#include <QMainWindow>
#include <QHash>
#include <QString>

class QLabel;
class QTimer;
class QScrollArea;
class QVBoxLayout;
class QCloseEvent;
class QResizeEvent;
class QShowEvent;
class QScreen;
class SensorsPanel;
class SettingsPanel;

// Main application surface: polling, persistence and sensor panel layout.
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshReadings();

private:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void setupUi();
    void setStatusMessage(const QString &text);
    void fitInitialWidthWithoutHorizontalScroll();
    void loadSettings();
    void saveSettings() const;
    static QString chipFingerprint(const QVector<SensorReading> &readings);

    SensorsBackend m_backend;
    QScrollArea *m_scrollArea;
    QWidget *m_contentContainer;
    QVBoxLayout *m_contentLayout;
    SensorsPanel *m_sensorsPanel;
    SettingsPanel *m_settingsPanel;
    QLabel *m_statusLabel;
    QTimer *m_timer;
    QHash<QString, bool> m_chipExpanded;
    QString m_loadedChipFingerprint;
    QString m_currentFingerprint;
    QVector<SensorReading> m_lastReadings;
    bool m_initialLayoutApplied = false;
    bool m_hasSavedGeometry = false;
    int m_pollingIntervalSec = 2;
};
