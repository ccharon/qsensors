// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/main_window.h"

#include "config/app_config_store.h"
#include "config/settings_schema.h"
#include "ui/main_window_state_store.h"
#include "ui/panels/sensors_panel.h"
#include "ui/panels/settings_panel.h"
#include "ui/theme/app_theme.h"
#include "ui/widgets/status_line.h"
#include "ui/widgets/vertical_scroll_area.h"
#include "ui/window_sizing.h"

#include <QApplication>
#include <QResizeEvent>
#include <QSettings>
#include <QScreen>
#include <QStatusBar>
#include <QVBoxLayout>
#include <utility>

namespace {
    constexpr int kNoticeTimeoutMs = 10000;
}

MainWindow::MainWindow(SensorMonitor::SourceFactory sourceFactory, QWidget *parent)
    : QMainWindow(parent),
      m_monitor(new SensorMonitor(std::move(sourceFactory), this)),
      m_scrollArea(nullptr),
      m_sensorsPanel(nullptr),
      m_settingsPanel(nullptr),
      m_statusLine(nullptr) {
    setupUi();
    m_styledPalette = QApplication::palette();
    loadSettings();

    m_settingsPanel->setPollingInterval(m_runtimeConfig.pollingIntervalSec);
    m_settingsPanel->setFanDefaultMaxRpm(m_runtimeConfig.fanDefaultMaxRpm);
    m_settingsPanel->setTemperatureUnit(m_runtimeConfig.temperatureUnit);

    connect(m_settingsPanel, &SettingsPanel::pollingIntervalChanged, this, [this](int value) {
        m_runtimeConfig.pollingIntervalSec = value;
        applyRuntimeConfig();
    });

    connect(m_settingsPanel, &SettingsPanel::fanDefaultMaxRpmChanged, this, [this](int value) {
        m_runtimeConfig.fanDefaultMaxRpm = value;
        applyRuntimeConfig();
    });

    connect(m_settingsPanel, &SettingsPanel::temperatureUnitChanged, this, [this](const TemperatureUnit unit) {
        m_runtimeConfig.temperatureUnit = unit;
        applyRuntimeConfig();
    });

    connect(m_monitor, &SensorMonitor::readingsChanged, this, &MainWindow::showReadings);
    // Reads once before returning, so the first readings are shown before the window.
    if (!m_monitor->start(m_runtimeConfig)) {
        setStatusMessage(tr("libsensors init failed: %1").arg(m_monitor->lastError()));
    }
}

void MainWindow::showReadings(const QVector<SensorReading> &readings) {
    m_sensorsPanel->setReadings(readings);
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
    m_statusLine->showNotice(text, kNoticeTimeoutMs);
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

    m_scrollArea = new VerticalScrollArea(central);

    auto *content = new QWidget(m_scrollArea);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(AppTheme::kNarrowGap);

    m_sensorsPanel = new SensorsPanel(content);
    m_settingsPanel = new SettingsPanel(content);
    connect(m_sensorsPanel, &SensorsPanel::layoutStateReset, this, [this] {
        showNotice(tr("Sensor layout changed: UI config reset"));
    });

    auto *settingsHost = new QWidget(content);
    auto *settingsHostLayout = new QVBoxLayout(settingsHost);
    settingsHostLayout->setContentsMargins(AppTheme::kSectionInset, 0, AppTheme::kSectionInset,
                                           AppTheme::kSectionInset + AppTheme::kNarrowGap);
    settingsHostLayout->setSpacing(0);
    settingsHostLayout->addWidget(m_settingsPanel, 0, Qt::AlignBottom);

    // Settings section should keep its natural height and never consume spare vertical space.
    m_settingsPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    settingsHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    // Sensor panels take the spare height, so the settings sit at the bottom.
    contentLayout->addWidget(m_sensorsPanel, 1);
    contentLayout->addWidget(settingsHost, 0, Qt::AlignBottom);

    m_scrollArea->setWidget(content);
    layout->addWidget(m_scrollArea);
    setCentralWidget(central);

    // Own status widget instead of QStatusBar::showMessage(), which leaves normal
    // widgets visible when the message is set before the window is shown.
    m_statusLine = new StatusLine(this);
    statusBar()->addWidget(m_statusLine, 1);
}

void MainWindow::setStatusMessage(const QString &text) {
    m_statusLine->setPermanentText(text);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    updateSizeLimits();
}

bool MainWindow::event(QEvent *event) {
    const bool handled = QMainWindow::event(event);
    // Posted whenever a size hint below changed, e.g. a collapsed chip or new sensors.
    if (event->type() == QEvent::LayoutRequest) {
        updateSizeLimits();
    }
    return handled;
}

int MainWindow::contentWindowHeight() const {
    return height() - m_scrollArea->height() + m_scrollArea->sizeHint().height();
}

int MainWindow::contentWindowWidth() const {
    const int usefulWidth = m_sensorsPanel->maximumUsefulWidth();
    if (usefulWidth >= QWIDGETSIZE_MAX)
        return QWIDGETSIZE_MAX;
    // The settings may need more than the sensors; they must never be cut off.
    const int contentWidth = std::max(usefulWidth, m_scrollArea->widget()->minimumSizeHint().width());
    return width() - m_scrollArea->width() + m_scrollArea->widthForContent(contentWidth);
}

QSize MainWindow::availableScreenSize() const {
    return screen() != nullptr ? screen()->availableGeometry().size() : size();
}

void MainWindow::updateSizeLimits() {
    if (isMaximized() || isFullScreen()) {
        setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        return;
    }
    const QSize screenSize = availableScreenSize();
    if (m_fitToContent) {
        m_fitToContent = false;
        // Raise the limits first; earlier, smaller limits would clip the resize.
        const QSize fitted(std::min({width(), contentWindowWidth(), screenSize.width()}),
                           std::min(contentWindowHeight(), screenSize.height()));
        setMaximumSize(maximumSize().expandedTo(fitted));
        resize(fitted);
    }
    const QSize limit(
        std::max(WindowSizing::maximumExtent(contentWindowWidth(), width(), screenSize.width()), minimumWidth()),
        WindowSizing::maximumExtent(contentWindowHeight(), height(), screenSize.height()));
    if (limit != maximumSize()) {
        setMaximumSize(limit);
    }
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    if (!m_initialLayoutApplied && m_sensorsPanel->readingCount() > 0) {
        // Without saved geometry the first size fits the content; applied once the
        // layout has computed its size (see updateSizeLimits()).
        m_fitToContent = !m_hasSavedGeometry;
        m_initialLayoutApplied = true;
        QMetaObject::invokeMethod(this, &MainWindow::updateSizeLimits, Qt::QueuedConnection);
    }
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    // ApplicationPaletteChange is not forwarded to changeEvent(); a system light/dark
    // switch arrives here as the resulting PaletteChange (or ThemeChange).
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ThemeChange) {
        applyThemeRefresh();
    } else if (event->type() == QEvent::WindowStateChange) {
        // Maximized and full-screen windows fill the screen regardless of content.
        updateSizeLimits();
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
    m_sensorsPanel->setChipOrder(state.chipOrder);
}

void MainWindow::saveSettings() const {
    // Runs on close, when no UI feedback is possible; the stores log failures.
    (void) AppConfigStore::saveRuntimeConfig(m_runtimeConfig);
    (void) MainWindowStateStore::save({
        .geometry = saveGeometry(),
        .chipExpanded = m_sensorsPanel->chipExpandedState(),
        .chipOrder = m_sensorsPanel->chipOrder(),
        .sensorFingerprint = m_sensorsPanel->chipFingerprint(),
    });
}

void MainWindow::applyRuntimeConfig() {
    persistRuntimeConfig();
    m_monitor->setConfig(m_runtimeConfig);
    updateReadingsStatus();
}
