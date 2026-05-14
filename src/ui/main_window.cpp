// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "main_window.h"

#include "settings_panel.h"
#include "sensors_panel.h"

#include <QCloseEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QShowEvent>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>
#include <QStringList>

namespace {
constexpr int kDefaultPollingIntervalSec = 2;
}

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

    m_settingsPanel->setPollingInterval(m_pollingIntervalSec);
    connect(m_settingsPanel, &SettingsPanel::pollingIntervalChanged, this, [this](int value) {
        m_pollingIntervalSec = value;
        m_timer->setInterval(m_pollingIntervalSec * 1000);
        setStatusMessage(tr("Readings: %1 | Refresh: %2s").arg(m_lastReadings.size()).arg(m_pollingIntervalSec));
    });

    if (!m_backend.isInitialized()) {
        setStatusMessage(tr("libsensors init failed: %1").arg(m_backend.lastError()));
        return;
    }

    connect(m_timer, &QTimer::timeout, this, &MainWindow::refreshReadings);
    m_timer->start(m_pollingIntervalSec * 1000);
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

    setStatusMessage(tr("Readings: %1 | Refresh: %2s").arg(m_lastReadings.size()).arg(m_pollingIntervalSec));
}

void MainWindow::setupUi() {
    setWindowTitle(tr("qsensors"));
    resize(900, 520);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    m_scrollArea = new QScrollArea(central);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_contentContainer = new QWidget(m_scrollArea);
    m_contentLayout = new QVBoxLayout(m_contentContainer);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    // SensorsPanel has an internal top margin of 8px; keep outer spacing at 2px
    // so the visual gap to the first sensor card matches the 10px card-to-card gap.
    m_contentLayout->setSpacing(2);

    m_sensorsPanel = new SensorsPanel(m_contentContainer);
    m_settingsPanel = new SettingsPanel(m_contentContainer);
    connect(m_sensorsPanel, &SensorsPanel::chipExpandedStateChanged, this, [this](const QHash<QString, bool> &state) {
        m_chipExpanded = state;
    });

    // Keep settings width aligned with sensor cards by using the same side insets.
    auto *settingsHost = new QWidget(m_contentContainer);
    auto *settingsHostLayout = new QVBoxLayout(settingsHost);
    settingsHostLayout->setContentsMargins(8, 0, 8, 0);
    settingsHostLayout->setSpacing(0);
    settingsHostLayout->addWidget(m_settingsPanel);

    // Settings first, then sensor panels.
    m_contentLayout->addWidget(settingsHost);
    m_contentLayout->addWidget(m_sensorsPanel);

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
        m_sensorsPanel->relayout(m_scrollArea != nullptr && m_scrollArea->viewport() != nullptr ? m_scrollArea->viewport()->width() : width());
    }
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    if (!m_initialLayoutApplied && !m_lastReadings.isEmpty()) {
        m_sensorsPanel->setChipExpandedState(m_chipExpanded);
        m_sensorsPanel->relayout(m_scrollArea != nullptr && m_scrollArea->viewport() != nullptr ? m_scrollArea->viewport()->width() : width());
        if (!m_hasSavedGeometry) {
            fitInitialWidthWithoutHorizontalScroll();
        }
        m_initialLayoutApplied = true;
    }
}

void MainWindow::loadSettings() {
    QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("ui/geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        m_hasSavedGeometry = true;
        restoreGeometry(geometry);
    } else {
        m_hasSavedGeometry = false;
    }

    settings.beginGroup(QStringLiteral("ui/chips"));
    const QStringList keys = settings.childKeys();
    for (const QString &key : keys) {
        m_chipExpanded.insert(key, settings.value(key, true).toBool());
    }
    settings.endGroup();

    m_pollingIntervalSec = settings.value(QStringLiteral("ui/polling_interval_sec"), kDefaultPollingIntervalSec).toInt();
    if (m_pollingIntervalSec < 1 || m_pollingIntervalSec > 10) {
        m_pollingIntervalSec = kDefaultPollingIntervalSec;
    }

    m_loadedChipFingerprint = settings.value(QStringLiteral("sensors/fingerprint")).toString();
}

void MainWindow::saveSettings() const {
    QSettings settings;
    settings.setValue(QStringLiteral("ui/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("ui/polling_interval_sec"), m_pollingIntervalSec);
    settings.setValue(QStringLiteral("sensors/fingerprint"), m_currentFingerprint);
    settings.beginGroup(QStringLiteral("ui/chips"));
    settings.remove(QString());
    for (auto it = m_chipExpanded.constBegin(); it != m_chipExpanded.constEnd(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
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
    for (const SensorReading &r : readings) {
        chips.push_back(r.chip);
    }
    chips.removeDuplicates();
    chips.sort();
    return chips.join(QStringLiteral("\n"));
}
