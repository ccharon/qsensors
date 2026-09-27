// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "main_window.h"

#include "app_config_store.h"
#include "theme/app_theme.h"
#include "main_window_state_store.h"
#include "settings_schema.h"
#include "settings_panel.h"
#include "sensors_panel.h"

#include <QApplication>
#include <QLabel>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QScreen>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <chrono>

namespace {
    constexpr int kNoticeTimeoutMs = 10000;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_scrollArea(nullptr),
      m_contentContainer(nullptr),
      m_sensorsPanel(nullptr),
      m_settingsPanel(nullptr),
      m_statusLabel(nullptr),
      m_timer(new QTimer(this)) {
    setupUi();
    m_styledPalette = QApplication::palette();
    loadSettings();

    m_settingsPanel->setPollingInterval(m_runtimeConfig.pollingIntervalSec);
    m_settingsPanel->setFanDefaultMaxRpm(m_runtimeConfig.fanDefaultMaxRpm);
    m_settingsPanel->setTemperatureUnit(m_runtimeConfig.temperatureUnit);

    connect(m_settingsPanel, &SettingsPanel::pollingIntervalChanged, this, [this](int value) {
        m_runtimeConfig.pollingIntervalSec = value;
        applyRuntimeConfig();
        persistRuntimeConfig();
        updateReadingsStatus();
    });

    connect(m_settingsPanel, &SettingsPanel::fanDefaultMaxRpmChanged, this, [this](int value) {
        m_runtimeConfig.fanDefaultMaxRpm = value;
        persistRuntimeConfig();
        refreshReadings();
    });

    connect(m_settingsPanel, &SettingsPanel::temperatureUnitChanged, this, [this](const TemperatureUnit unit) {
        m_runtimeConfig.temperatureUnit = unit;
        persistRuntimeConfig();
        refreshReadings();
    });

    if (!m_backend.isInitialized()) {
        setStatusMessage(tr("libsensors init failed: %1").arg(m_backend.lastError()));
        return;
    }

    connect(m_timer, &QTimer::timeout, this, &MainWindow::refreshReadings);
    applyRuntimeConfig();
    m_timer->start();
    // Intentional first immediate sample for fast startup feedback.
    refreshReadings();
}

void MainWindow::refreshReadings() {
    m_sensorsPanel->setReadings(m_backend.readAll(m_runtimeConfig.fanDefaultMaxRpm, m_runtimeConfig.temperatureUnit),
                                viewportWidth());
    updateReadingsStatus();
}

void MainWindow::updateReadingsStatus() {
    const int count = m_sensorsPanel->readingCount();
    if (count == 0) {
        setStatusMessage(tr("No sensors found. Run sensors-detect and check the lm-sensors configuration."));
        return;
    }
    setStatusMessage(tr("Readings: %1 | Refresh: %2s").arg(count).arg(m_runtimeConfig.pollingIntervalSec));
}

void MainWindow::showNotice(const QString &text) {
    // Temporary message: the readings label stays in place underneath.
    statusBar()->showMessage(text, kNoticeTimeoutMs);
}

void MainWindow::persistRuntimeConfig() {
    if (!AppConfigStore::saveRuntimeConfig(m_runtimeConfig)) {
        showNotice(tr("Settings could not be saved"));
    }
}

void MainWindow::setupUi() {
    setWindowTitle(QStringLiteral("%1 %2").arg(QApplication::applicationName(), QApplication::applicationVersion()));
    resize(AppTheme::kInitialWindowWidth, AppTheme::kInitialWindowHeight);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea = new QScrollArea(central);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    m_contentContainer = new QWidget(m_scrollArea);
    auto *contentLayout = new QVBoxLayout(m_contentContainer);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(AppTheme::kNarrowGap);
    contentLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    m_sensorsPanel = new SensorsPanel(m_contentContainer);
    m_settingsPanel = new SettingsPanel(m_contentContainer);
    connect(m_sensorsPanel, &SensorsPanel::structureChanged, this, &MainWindow::updateMinimumWindowWidthConstraint);
    connect(m_sensorsPanel, &SensorsPanel::layoutStateReset, this, [this] {
        showNotice(tr("Sensor layout changed: UI config reset"));
    });

    auto *settingsHost = new QWidget(m_contentContainer);
    auto *settingsHostLayout = new QVBoxLayout(settingsHost);
    settingsHostLayout->setContentsMargins(AppTheme::kSectionInset, AppTheme::kSectionInset + AppTheme::kNarrowGap, AppTheme::kSectionInset, 0);
    settingsHostLayout->setSpacing(0);
    settingsHostLayout->addWidget(m_settingsPanel, 0, Qt::AlignTop);

    // Settings section should keep its natural height and never consume spare vertical space.
    m_settingsPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    settingsHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // Settings first, then sensor panels.
    contentLayout->addWidget(settingsHost, 0, Qt::AlignTop);
    contentLayout->addWidget(m_sensorsPanel, 1);

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
    m_sensorsPanel->relayout(viewportWidth());
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    if (!m_initialLayoutApplied && m_sensorsPanel->readingCount() > 0) {
        m_sensorsPanel->relayout(viewportWidth());
        ensureNoHorizontalOverflow(m_hasSavedGeometry ? AppTheme::kRestoredWidthFitPadding : AppTheme::kInitialWidthFitPadding);
        updateMinimumWindowWidthConstraint();
        m_initialLayoutApplied = true;
    }
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    // ApplicationPaletteChange is not forwarded to changeEvent(); a system light/dark
    // switch arrives here as the resulting PaletteChange (or ThemeChange).
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ThemeChange) {
        applyThemeRefresh();
    }
}

void MainWindow::applyThemeRefresh() {
    const QPalette current = QApplication::palette();
    if (current == m_styledPalette) {
        return;
    }
    m_styledPalette = current;
    AppTheme::refreshStyleSheets(this);
}

void MainWindow::loadSettings() {
    // Runtime config and UI layout state are intentionally persisted independently.
    m_runtimeConfig = AppConfigStore::loadRuntimeConfig();
    if (QSettings settings; SettingsSchema::storedVersion(settings) > SettingsSchema::kCurrentVersion) {
        showNotice(tr("Settings were written by a newer qsensors version and are used as far as possible"));
    }
    const MainWindowState state = MainWindowStateStore::load();

    m_hasSavedGeometry = !state.geometry.isEmpty() && restoreGeometry(state.geometry);

    m_sensorsPanel->restoreChipExpandedState(state.chipExpanded, state.sensorFingerprint);
}

void MainWindow::saveSettings() const {
    // Runs on close, when no UI feedback is possible; the stores log failures.
    (void) AppConfigStore::saveRuntimeConfig(m_runtimeConfig);
    (void) MainWindowStateStore::save(saveGeometry(), m_sensorsPanel->chipFingerprint(),
                                      m_sensorsPanel->chipExpandedState());
}

void MainWindow::applyRuntimeConfig() {
    m_timer->setInterval(std::chrono::seconds(m_runtimeConfig.pollingIntervalSec));
}

void MainWindow::ensureNoHorizontalOverflow(const int extraPadding) {
    // Let pending layout updates settle before deciding if horizontal overflow is real.
    m_scrollArea->ensurePolished();
    m_scrollArea->updateGeometry();
    m_contentContainer->ensurePolished();
    m_contentContainer->updateGeometry();

    auto *hBar = m_scrollArea->horizontalScrollBar();
    if (hBar->maximum() <= 0) {
        return;
    }

    QScreen *screen = windowHandle() != nullptr ? windowHandle()->screen() : QApplication::primaryScreen();
    if (screen == nullptr) {
        return;
    }

    const int maxWidth = screen->availableGeometry().width();
    const int extra = hBar->maximum() + extraPadding;
    const int targetWidth = std::min(maxWidth, width() + extra);
    if (targetWidth > width()) {
        resize(targetWidth, height());
    }
}

void MainWindow::updateMinimumWindowWidthConstraint() {
    const int sensorsMin = m_sensorsPanel->minimumRequiredWidth();
    const int settingsMin = (AppTheme::kSectionInset * 2) + m_settingsPanel->minimumRequiredWidth();
    const int requiredContentWidth = std::max(sensorsMin, settingsMin);

    m_contentContainer->setMinimumWidth(requiredContentWidth);

    const int verticalScrollbarReserve = m_scrollArea->verticalScrollBar()->sizeHint().width();
    const int scrollAreaChrome = (m_scrollArea->frameWidth() * 2) + verticalScrollbarReserve;
    const int requiredCentralWidth = requiredContentWidth + scrollAreaChrome;
    m_scrollArea->setMinimumWidth(requiredCentralWidth);
}

int MainWindow::viewportWidth() const {
    return m_scrollArea->viewport()->width();
}

