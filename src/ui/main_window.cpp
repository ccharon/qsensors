// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "main_window.h"

#include "app_config_store.h"
#include "theme/app_theme.h"
#include "main_window_state_store.h"
#include "settings_schema.h"
#include "settings_panel.h"
#include "sensors_panel.h"
#include "status_line.h"
#include "window_sizing.h"

#include <QApplication>
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
      m_statusLine(nullptr),
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

    m_scrollArea = new QScrollArea(central);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

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

    m_scrollArea->setWidget(m_contentContainer);
    layout->addWidget(m_scrollArea);
    setCentralWidget(central);

    // The viewport narrows when the scrollbar appears; the cards follow its width.
    m_scrollArea->viewport()->installEventFilter(this);
    // Expanding or collapsing sections changes the content height.
    m_contentContainer->installEventFilter(this);

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
    updateHeightLimit();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_scrollArea->viewport() && event->type() == QEvent::Resize) {
        m_sensorsPanel->relayout(viewportWidth());
    } else if (watched == m_contentContainer && event->type() == QEvent::LayoutRequest) {
        // The layout recomputes its size hint after this event; read it afterwards.
        QMetaObject::invokeMethod(this, &MainWindow::updateHeightLimit, Qt::QueuedConnection);
    }
    return QMainWindow::eventFilter(watched, event);
}

int MainWindow::contentWindowHeight() const {
    return height() - m_scrollArea->viewport()->height() + m_contentContainer->layout()->sizeHint().height();
}

int MainWindow::availableScreenHeight() const {
    return screen() != nullptr ? screen()->availableGeometry().height() : height();
}

void MainWindow::updateHeightLimit() {
    if (isMaximized() || isFullScreen()) {
        setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }
    if (m_fitHeightToContent) {
        m_fitHeightToContent = false;
        // Raise the limit first; an earlier, smaller limit would clip the resize.
        const int fitted = std::min(contentWindowHeight(), availableScreenHeight());
        setMaximumHeight(std::max(fitted, maximumHeight()));
        resize(width(), fitted);
    }
    const int limit = WindowSizing::maximumHeight(contentWindowHeight(), height(), availableScreenHeight());
    if (limit != maximumHeight()) {
        setMaximumHeight(limit);
    }
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    if (!m_initialLayoutApplied && m_sensorsPanel->readingCount() > 0) {
        m_sensorsPanel->relayout(viewportWidth());
        ensureNoHorizontalOverflow(m_hasSavedGeometry ? AppTheme::kRestoredWidthFitPadding : AppTheme::kInitialWidthFitPadding);
        updateMinimumWindowWidthConstraint();
        // Without saved geometry the first height fits the content; applied once the
        // layout has computed its size (see updateHeightLimit()).
        m_fitHeightToContent = !m_hasSavedGeometry;
        m_initialLayoutApplied = true;
        QMetaObject::invokeMethod(this, &MainWindow::updateHeightLimit, Qt::QueuedConnection);
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
        updateHeightLimit();
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

