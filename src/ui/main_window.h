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
class QTimer;
class StatusLine;
class VerticalScrollArea;

/** Main application surface: polling, persistence and sensor panel layout. */
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    /**
     * Builds the UI, restores settings and starts polling the source made by @p sourceFactory,
     * waiting at most @p startupTimeout for the first readings (see SensorMonitor::start()).
     */
    explicit MainWindow(SensorMonitor::SourceFactory sourceFactory, QWidget *parent = nullptr,
                        std::chrono::milliseconds startupTimeout = SensorMonitor::kDefaultStartupTimeout);

    /** Writes a runtime config change still waiting for its debounce interval. */
    ~MainWindow() override;

protected:
    /** Persists settings and window state before closing. */
    void closeEvent(QCloseEvent *event) override;

    /** Re-evaluates the size limits after the user resized the window. */
    void resizeEvent(QResizeEvent *event) override;

    /** Fits the size to the content on the first show without a saved window size (see applyInitialLayout()). */
    void showEvent(QShowEvent *event) override;

    /** Re-applies style sheets on light/dark switches; lifts the size limits when maximized. */
    void changeEvent(QEvent *event) override;

    /** Re-evaluates the size limits once the layouts have taken a content change into account. */
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

    /** Permanent status: the source error, a stalled read, the reading count and refresh interval, or a hint when no sensors exist. */
    void updateReadingsStatus();

    /** Shows @p text for a while instead of the permanent status. */
    void showNotice(const QString &text);

    /** Saves the runtime config and reports a failed write; also what the debounce timer runs. */
    void persistRuntimeConfig();

    /** Saves chip order, expand state right away and reports a failed write. */
    void persistChipLayout();

    /** Loads runtime config, geometry, chip order and expand state. */
    void loadSettings();

    /** Persists runtime config, geometry, chip order and expand state. */
    void saveSettings() const;

    /** Window height at which the whole content fits without scrolling. */
    [[nodiscard]] int contentWindowHeight() const;

    /** Window width at which all cards have their maximum width and columns (see CardGridPlan). */
    [[nodiscard]] int contentWindowWidth() const;

    /** Available size of the window's screen. */
    [[nodiscard]] QSize availableScreenSize() const;

    /** Keeps the window from being dragged larger than its content needs (see WindowSizing). */
    void updateSizeLimits();

    /**
     * Once the window is shown and has readings: without saved geometry, fits the window
     * to the content. Runs at the first show, or when late first readings arrive, unless
     * the window was resized, maximized or made full-screen while waiting for them.
     */
    void applyInitialLayout();

    SensorMonitor *m_monitor;
    QString m_sourceError; // set when the sensor source could not be initialized
    VerticalScrollArea *m_scrollArea;
    SensorsPanel *m_sensorsPanel;
    SettingsPanel *m_settingsPanel;
    StatusLine *m_statusLine;
    QTimer *m_saveTimer; // debounces writing the runtime config after spin box steps
    bool m_initialLayoutApplied = false;
    bool m_hasSavedGeometry = false;
    bool m_fitToContent = false; // pending first size fit without saved geometry
    QSize m_shownSize;           // size at the first show; another size later means the user resized
    RuntimeConfig m_runtimeConfig;
    bool m_settingsReadOnly = false; // stored schema is newer than this build understands: never write
    // Palette the style sheets were last resolved against; avoids redundant refreshes.
    QPalette m_styledPalette;
};
