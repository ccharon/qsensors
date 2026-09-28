// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "config/runtime_config.h"
#include "sensors/sensor_monitor.h"
#include "sensors/sensor_reading.h"

#include <QMainWindow>
#include <QPalette>
#include <QVector>

class QCloseEvent;
class QResizeEvent;
class QShowEvent;
class SensorsPanel;
class SettingsPanel;
class StatusLine;
class VerticalScrollArea;

/** Main application surface: polling, persistence and sensor panel layout. */
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    /** Builds the UI, restores settings and starts polling the source made by @p sourceFactory. */
    explicit MainWindow(SensorMonitor::SourceFactory sourceFactory, QWidget *parent = nullptr);

protected:
    /** Persists settings and window state before closing. */
    void closeEvent(QCloseEvent *event) override;

    /** Re-evaluates the height limit after the user resized the window. */
    void resizeEvent(QResizeEvent *event) override;

    /** Fits the height to the content on the first show without a saved window size. */
    void showEvent(QShowEvent *event) override;

    /** Re-applies style sheets on light/dark switches; lifts the height limit when maximized. */
    void changeEvent(QEvent *event) override;

    /** Re-evaluates the height limit once the layouts have taken a content change into account. */
    bool event(QEvent *event) override;

private:
    /** Hands new display readings to the sensors panel and updates the status. */
    void showReadings(const QVector<SensorReading> &readings);

    /** Re-applies all style sheets so palette(...) references resolve to the current palette. */
    void applyThemeRefresh();

    /** Builds static widget hierarchy and signal wiring. */
    void setupUi();
    /** Applies a changed runtime config to polling, persists it and updates the status. */
    void applyRuntimeConfig();

    /** Sets the permanent status bar text. */
    void setStatusMessage(const QString &text);

    /** Permanent status: reading count and refresh interval, or a hint when no sensors exist. */
    void updateReadingsStatus();

    /** Shows @p text for a while instead of the permanent status. */
    void showNotice(const QString &text);

    /** Saves the runtime config right away and reports a failed write. */
    void persistRuntimeConfig();

    /** Loads runtime config, geometry, chip order, expand state and chip fingerprint. */
    void loadSettings();

    /** Persists runtime config, geometry, chip order, expand state and chip fingerprint. */
    void saveSettings() const;

    /** Window height at which the whole content fits without scrolling. */
    [[nodiscard]] int contentWindowHeight() const;

    /** Available height of the window's screen. */
    [[nodiscard]] int availableScreenHeight() const;

    /** Keeps the window from being dragged taller than its content (see WindowSizing). */
    void updateHeightLimit();

    SensorMonitor *m_monitor;
    VerticalScrollArea *m_scrollArea;
    SensorsPanel *m_sensorsPanel;
    SettingsPanel *m_settingsPanel;
    StatusLine *m_statusLine;
    bool m_initialLayoutApplied = false;
    bool m_hasSavedGeometry = false;
    bool m_fitHeightToContent = false; // pending first height fit without saved geometry
    RuntimeConfig m_runtimeConfig;
    // Palette the style sheets were last resolved against; avoids redundant refreshes.
    QPalette m_styledPalette;
};
