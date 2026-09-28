// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "config/runtime_config.h"
#include "sensors/sensor_reading.h"

#include <QMainWindow>
#include <QPalette>
#include <QVector>

class QScrollArea;
class QCloseEvent;
class QResizeEvent;
class QShowEvent;
class SensorMonitor;
class SensorsPanel;
class SettingsPanel;
class StatusLine;

/** Main application surface: polling, persistence and sensor panel layout. */
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    /** Builds the UI, restores settings and starts polling when libsensors is available. */
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    /** Persists settings and window state before closing. */
    void closeEvent(QCloseEvent *event) override;

    /** Re-evaluates the height limit after the user resized the window. */
    void resizeEvent(QResizeEvent *event) override;

    /** Applies initial relayout and optional width fit once after first data is shown. */
    void showEvent(QShowEvent *event) override;

    /** Re-applies style sheets on light/dark switches; lifts the height limit when maximized. */
    void changeEvent(QEvent *event) override;

    /** Relayouts on viewport width changes and tracks content height changes. */
    bool eventFilter(QObject *watched, QEvent *event) override;

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

    /** Keeps top-level minimum width aligned to widest currently required content. */
    void updateMinimumWindowWidthConstraint();

    /** Expands window width minimally until horizontal overflow is gone. */
    void ensureNoHorizontalOverflow(int extraPadding);

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

    /** Width available to the sensor panel inside the scroll area. */
    [[nodiscard]] int viewportWidth() const;

    SensorMonitor *m_monitor;
    QScrollArea *m_scrollArea;
    QWidget *m_contentContainer;
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
