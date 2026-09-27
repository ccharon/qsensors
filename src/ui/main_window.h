// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "sensors_backend.h"
#include "runtime_config.h"

#include <QMainWindow>
#include <QPalette>
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

/** Main application surface: polling, persistence and sensor panel layout. */
class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    /** Builds the UI, restores settings and starts polling when libsensors is available. */
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    /** Polls the backend and hands the readings to the sensors panel. */
    void refreshReadings();

protected:
    /** Persists settings and window state before closing. */
    void closeEvent(QCloseEvent *event) override;

    /** Reflows sensor cards to current viewport width while preserving expand state. */
    void resizeEvent(QResizeEvent *event) override;

    /** Applies initial relayout and optional width fit once after first data is shown. */
    void showEvent(QShowEvent *event) override;

    /** Re-applies style sheets when the system switches light/dark mode at runtime. */
    void changeEvent(QEvent *event) override;

private:
    /** Re-applies all style sheets so palette(...) references resolve to the current palette. */
    void applyThemeRefresh();

    /** Builds static widget hierarchy and signal wiring. */
    void setupUi();
    /** Applies the polling interval from the runtime config to the timer. */
    void applyRuntimeConfig();

    /** Updates status bar text in one place. */
    void setStatusMessage(const QString &text);

    /** Permanent status: reading count and refresh interval, or a hint when no sensors exist. */
    void updateReadingsStatus();

    /** Shows @p text temporarily on top of the permanent status. */
    void showNotice(const QString &text);

    /** Saves the runtime config right away and reports a failed write. */
    void persistRuntimeConfig();

    /** Keeps top-level minimum width aligned to widest currently required content. */
    void updateMinimumWindowWidthConstraint();

    /** Expands window width minimally until horizontal overflow is gone. */
    void ensureNoHorizontalOverflow(int extraPadding);

    /** Loads runtime config, geometry, expand state and chip fingerprint. */
    void loadSettings();

    /** Persists runtime config, geometry, expand state and chip fingerprint. */
    void saveSettings() const;

    /** Width available to the sensor panel inside the scroll area. */
    [[nodiscard]] int viewportWidth() const;

    SensorsBackend m_backend;
    QScrollArea *m_scrollArea;
    QWidget *m_contentContainer;
    QVBoxLayout *m_contentLayout;
    SensorsPanel *m_sensorsPanel;
    SettingsPanel *m_settingsPanel;
    QLabel *m_statusLabel;
    QTimer *m_timer;
    bool m_initialLayoutApplied = false;
    bool m_hasSavedGeometry = false;
    RuntimeConfig m_runtimeConfig;
    // Palette the style sheets were last resolved against; avoids redundant refreshes.
    QPalette m_styledPalette;
};
