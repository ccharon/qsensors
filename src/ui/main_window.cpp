// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "main_window.h"

#include "app_config_store.h"
#include "theme/app_theme.h"
#include "main_window_state_store.h"
#include "settings_panel.h"
#include "sensors_panel.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLabel>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QStringList>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_scrollArea(nullptr),
      m_contentContainer(nullptr),
      m_contentLayout(nullptr),
      m_sensorsPanel(nullptr),
      m_settingsPanel(nullptr),
      m_statusLabel(nullptr),
      m_timer(new QTimer(this)) {
    setupUi();
    loadSettings();

    m_settingsPanel->setPollingInterval(m_runtimeConfig.pollingIntervalSec);
    m_settingsPanel->setFanDefaultMaxRpm(m_runtimeConfig.fanDefaultMaxRpm);
    connect(m_settingsPanel, &SettingsPanel::pollingIntervalChanged, this, [this](int value) {
        m_runtimeConfig.pollingIntervalSec = value;
        // Apply immediately so the next tick uses the new cadence without restart.
        applyRuntimeConfig();
        setStatusMessage(
            tr("Readings: %1 | Refresh: %2s").arg(m_lastReadings.size()).arg(m_runtimeConfig.pollingIntervalSec)
        );
    });
    connect(m_settingsPanel, &SettingsPanel::fanDefaultMaxRpmChanged, this, [this](int value) {
        m_runtimeConfig.fanDefaultMaxRpm = value;
        // This only affects fallback limits when firmware does not expose FAN_MAX.
        applyRuntimeConfig();
    });

    if (!m_backend.isInitialized()) {
        setStatusMessage(tr("libsensors init failed: %1").arg(m_backend.lastError()));
        return;
    }
    applyRuntimeConfig();

    connect(m_timer, &QTimer::timeout, this, &MainWindow::refreshReadings);
    m_timer->start(m_runtimeConfig.pollingIntervalSec * 1000);
    // Intentional first immediate sample for fast startup feedback.
    refreshReadings();
}

void MainWindow::refreshReadings() {
    m_lastReadings = m_backend.readAll();
    const QString currentFingerprint = chipFingerprint(m_lastReadings);

    // Startup-loaded layout state applies only if chip composition still matches.
    if (!m_loadedChipFingerprint.isEmpty() && m_loadedChipFingerprint != currentFingerprint) {
        m_chipExpanded.clear();
        m_loadedChipFingerprint.clear();
        setStatusMessage(tr("Sensor layout changed: UI config reset"));
    }

    m_currentFingerprint = currentFingerprint;
    m_sensorsPanel->setChipExpandedState(m_chipExpanded);
    m_sensorsPanel->setReadings(
        m_lastReadings,
        m_scrollArea != nullptr && m_scrollArea->viewport() != nullptr ? m_scrollArea->viewport()->width() : width()
    );

    setStatusMessage(tr("Readings: %1 | Refresh: %2s").arg(m_lastReadings.size()).arg(m_runtimeConfig.pollingIntervalSec));
}

void MainWindow::setupUi() {
    setWindowTitle(QStringLiteral("%1 %2").arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion()));
    resize(900, 520);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    m_scrollArea = new QScrollArea(central);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_contentContainer = new QWidget(m_scrollArea);
    m_contentLayout = new QVBoxLayout(m_contentContainer);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(AppTheme::kNarrowGap);
    m_contentLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    m_sensorsPanel = new SensorsPanel(m_contentContainer);
    m_settingsPanel = new SettingsPanel(m_contentContainer);
    connect(m_sensorsPanel, &SensorsPanel::chipExpandedStateChanged, this, [this](const QHash<QString, bool> &state) {
        m_chipExpanded = state;
    });

    auto *settingsHost = new QWidget(m_contentContainer);
    auto *settingsHostLayout = new QVBoxLayout(settingsHost);
    settingsHostLayout->setContentsMargins(AppTheme::kSectionInset, 0, AppTheme::kSectionInset, 0);
    settingsHostLayout->setSpacing(0);
    settingsHostLayout->addWidget(m_settingsPanel, 0, Qt::AlignTop);

    // Settings section should keep its natural height and never consume spare vertical space.
    m_settingsPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    settingsHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // Settings first, then sensor panels.
    m_contentLayout->addWidget(settingsHost, 0, Qt::AlignTop);
    m_contentLayout->addWidget(m_sensorsPanel, 1);

    m_scrollArea->setWidget(m_contentContainer);
    layout->addWidget(m_scrollArea);
    setCentralWidget(central);

    m_statusLabel = new QLabel(this);
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::setStatusMessage(const QString &text) {
    m_statusLabel->setText(text);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (!m_lastReadings.isEmpty()) {
        m_sensorsPanel->setChipExpandedState(m_chipExpanded);
        m_sensorsPanel->relayout(m_scrollArea != nullptr && m_scrollArea->viewport() != nullptr
                                     ? m_scrollArea->viewport()->width()
                                     : width());
    }
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    if (!m_initialLayoutApplied && !m_lastReadings.isEmpty()) {
        m_sensorsPanel->setChipExpandedState(m_chipExpanded);
        m_sensorsPanel->relayout(m_scrollArea != nullptr && m_scrollArea->viewport() != nullptr
                                     ? m_scrollArea->viewport()->width()
                                     : width());
        if (!m_hasSavedGeometry) {
            fitInitialWidthWithoutHorizontalScroll();
        }
        m_initialLayoutApplied = true;
    }
}

void MainWindow::loadSettings() {
    // Runtime config and UI layout state are intentionally persisted independently.
    m_runtimeConfig = AppConfigStore::loadRuntimeConfig();
    const MainWindowState state = MainWindowStateStore::load();
    if (state.hasGeometry) {
        m_hasSavedGeometry = true;
        restoreGeometry(state.geometry);
    } else {
        m_hasSavedGeometry = false;
    }
    m_chipExpanded = state.chipExpanded;
    m_loadedChipFingerprint = state.sensorFingerprint;
}

void MainWindow::saveSettings() const {
    AppConfigStore::saveRuntimeConfig(m_runtimeConfig);
    MainWindowStateStore::save(
        saveGeometry(),
        m_currentFingerprint,
        m_chipExpanded
    );
}

void MainWindow::applyRuntimeConfig() {
    // Centralized fan-out point for runtime-tunable behavior.
    m_timer->setInterval(m_runtimeConfig.pollingIntervalSec * 1000);
    m_backend.applyConfig(m_runtimeConfig);
}

void MainWindow::fitInitialWidthWithoutHorizontalScroll() {
    if (m_scrollArea == nullptr || m_scrollArea->horizontalScrollBar() == nullptr) {
        return;
    }

    auto *hBar = m_scrollArea->horizontalScrollBar();
    if (hBar->maximum() <= 0) {
        return;
    }

    QScreen *screen = windowHandle() != nullptr ? windowHandle()->screen() : QGuiApplication::primaryScreen();
    if (screen == nullptr) {
        return;
    }

    const int maxWidth = screen->availableGeometry().width();
    const int extra = hBar->maximum() + 24;
    const int targetWidth = std::min(maxWidth, width() + extra);
    if (targetWidth > width()) {
        resize(targetWidth, height());
    }
}

QString MainWindow::chipFingerprint(const QVector<SensorReading> &readings) {
    QStringList chips;
    chips.reserve(readings.size());
    for (const SensorReading &r: readings) {
        chips.push_back(r.chip);
    }
    chips.removeDuplicates();
    chips.sort();
    return chips.join(QStringLiteral("\n"));
}
