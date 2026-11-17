// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "config/app_config_store.h"
#include "ui/main_window.h"
#include "ui/theme/app_theme.h"
#include "ui/widgets/collapsible_section.h"
#include "ui/widgets/sensor_value_widget.h"
#include "ui/widgets/status_line.h"

#include "config/settings_keys.h"
#include "ui/main_window_state_store.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLayout>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QToolButton>
#include <QWaitCondition>
#include <QtTest/QtTest>

#include <atomic>
#include <chrono>
#include <memory>

// Verifies the window sizing with a fake sensor source: widths, card sizes and the height limit.
class MainWindowTest final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void first_start_fits_height_to_content();
    void minimum_width_shows_every_card_completely();
    void cards_share_one_size_and_survive_resizing();
    void height_limited_to_content_but_never_forced_smaller();
    void width_limited_to_full_layout();
    void source_error_stays_in_status_after_setting_change();
    void stalled_read_shown_in_status_until_reads_resume();
    void long_status_text_does_not_change_window_width_limits();
    void expand_state_is_saved_when_the_user_changes_it_not_on_startup();
    void failed_layout_write_is_reported();
    void spin_box_steps_are_saved_once_after_the_last_step();
    void pending_setting_change_is_saved_when_the_window_closes();
    void sensor_title_with_markup_is_shown_literally();
    void sensor_card_height_follows_font_changes();
    void sensor_card_tooltip_follows_every_displayed_field();
    void newer_schema_settings_survive_a_session_and_are_reported();
    void source_that_stops_working_is_shown_in_status();
    void source_that_recovers_is_shown_again();

private:
    QTemporaryDir m_dir;
};

namespace {
    /** Lets a test hold reads of the fake source until it opens the gate again. */
    struct ReadGate {
        QMutex mutex;
        QWaitCondition opened;
        bool closed = false;

        void close() {
            QMutexLocker lock(&mutex);
            closed = true;
        }

        void open() {
            QMutexLocker lock(&mutex);
            closed = false;
            opened.wakeAll();
        }

        void pass() {
            QMutexLocker lock(&mutex);
            while (closed)
                opened.wait(&mutex);
        }
    };

    /** Opens the gate when leaving the scope, so no worker stays blocked after a failed check. */
    struct GateGuard {
        std::shared_ptr<ReadGate> gate;
        ~GateGuard() { gate->open(); }
    };

    class FakeSource final : public SensorSource {
    public:
        explicit FakeSource(std::shared_ptr<ReadGate> gate = {}) : m_gate(std::move(gate)) {}

        [[nodiscard]] bool isInitialized() const override { return true; }
        [[nodiscard]] QString lastError() const override { return {}; }

        [[nodiscard]] QVector<SensorReading> readAll() override {
            if (m_gate)
                m_gate->pass();
            QVector<SensorReading> readings;
            const auto add = [&readings](const QString &chip, const SensorCategory category, const int number,
                                         const SensorUnit unit, const double value) {
                readings.append({.chip = chip, .category = category, .feature = QStringLiteral("s%1").arg(number),
                                 .featureNumber = number, .subfeatureNumber = number, .value = value, .unit = unit});
            };
            for (int i = 0; i < 4; ++i)
                add(QStringLiteral("chip-a"), SensorCategory::Temperatures, i, SensorUnit::Celsius, 40.0 + i);
            add(QStringLiteral("chip-a"), SensorCategory::Fans, 10, SensorUnit::Rpm, 1200.0);
            for (int i = 0; i < 3; ++i)
                add(QStringLiteral("chip-b"), SensorCategory::Voltages, i, SensorUnit::Volt, 1.0 + i);
            return readings;
        }

    private:
        std::shared_ptr<ReadGate> m_gate;
    };

    class BrokenSource final : public SensorSource {
    public:
        explicit BrokenSource(QString error = QStringLiteral("sensors_init failed (4)")) : m_error(std::move(error)) {}

        [[nodiscard]] bool isInitialized() const override { return false; }
        [[nodiscard]] QString lastError() const override { return m_error; }
        [[nodiscard]] QVector<SensorReading> readAll() override { return {}; }

    private:
        QString m_error;
    };

    /**
     * Reads once, then stops working like LibsensorsSource after a failed re-init; a retry
     * brings it back once @p recoverable is set.
     */
    class FailingSource final : public SensorSource {
    public:
        explicit FailingSource(std::shared_ptr<std::atomic_bool> recoverable = {}) : m_recoverable(std::move(recoverable)) {}

        [[nodiscard]] bool isInitialized() const override { return !m_failed; }
        [[nodiscard]] QString lastError() const override { return QStringLiteral("re-init failed (4)"); }

        [[nodiscard]] QVector<SensorReading> readAll() override {
            if (m_reads++ == 1) {
                m_failed = true;
                return {};
            }
            return {{.chip = QStringLiteral("chip-a"), .category = SensorCategory::Temperatures,
                     .feature = QStringLiteral("s0"), .featureNumber = 0, .subfeatureNumber = 0, .value = 40.0,
                     .unit = SensorUnit::Celsius}};
        }

        [[nodiscard]] bool recover(bool /*retryDue*/) override {
            if (m_recoverable && *m_recoverable)
                m_failed = false;
            return !m_failed;
        }

    private:
        std::shared_ptr<std::atomic_bool> m_recoverable;
        int m_reads = 0;
        bool m_failed = false;
    };

    // Runs what the last change posted (layout requests, resizes) without a timed wait.
    void settleEvents() {
        for (int i = 0; i < 3; ++i) {
            QCoreApplication::sendPostedEvents();
            QCoreApplication::processEvents();
        }
    }

    std::unique_ptr<MainWindow> showWindow() {
        auto window = std::make_unique<MainWindow>([] { return std::make_unique<FakeSource>(); });
        window->show();
        [[maybe_unused]] const bool exposed = QTest::qWaitForWindowExposed(window.get());
        settleEvents();
        return window;
    }

    void resizeWindow(MainWindow &window, const int width, const int height) {
        window.resize(width, height);
        settleEvents();
    }

    QScrollArea *scrollArea(const MainWindow &window) {
        return window.findChild<QScrollArea *>();
    }

    QList<SensorValueWidget *> visibleCards(const MainWindow &window) {
        QList<SensorValueWidget *> cards;
        for (SensorValueWidget *card: window.findChildren<SensorValueWidget *>())
            if (card->isVisible())
                cards.append(card);
        return cards;
    }

    /** True when the whole content is visible without scrolling. */
    bool contentFits(const MainWindow &window) {
        const QScrollArea *area = scrollArea(window);
        return !area->verticalScrollBar()->isVisible() && area->widget()->height() == area->viewport()->height();
    }

    CollapsibleSection *section(const MainWindow &window, const QString &chip) {
        for (CollapsibleSection *s: window.findChildren<CollapsibleSection *>())
            if (s->findChild<QToolButton *>()->text() == chip)
                return s;
        return nullptr;
    }
}

void MainWindowTest::init() {
    QVERIFY(m_dir.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.path());
    QCoreApplication::setOrganizationName(QStringLiteral("qsensors-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("main-window"));
    QSettings().clear();
}

void MainWindowTest::first_start_fits_height_to_content() {
    const auto window = showWindow();
    QVERIFY(contentFits(*window));
    QCOMPARE(window->maximumHeight(), window->height());
    // The full layout is narrower than the default width, so the width is fitted too.
    QVERIFY(window->width() < AppTheme::kInitialWindowWidth);
    QCOMPARE(window->width(), window->maximumWidth());
}

void MainWindowTest::minimum_width_shows_every_card_completely() {
    const auto window = showWindow();
    resizeWindow(*window, 50, 400);
    QCOMPARE(window->width(), window->minimumWidth());

    const QWidget *viewport = scrollArea(*window)->viewport();
    QVERIFY(scrollArea(*window)->widget()->width() <= viewport->width());
    for (const SensorValueWidget *card: visibleCards(*window)) {
        const int right = card->mapTo(viewport, QPoint(card->width(), 0)).x();
        QVERIFY2(right <= viewport->width(), qPrintable(card->toolTip()));
    }
}

void MainWindowTest::cards_share_one_size_and_survive_resizing() {
    const auto window = showWindow();
    QList<QPointer<SensorValueWidget>> cards;
    for (SensorValueWidget *card: visibleCards(*window))
        cards.append(card);
    QCOMPARE(cards.size(), 8);

    QSet<int> columnCounts;
    for (int width = window->minimumWidth(); width < 800; width += 23) {
        resizeWindow(*window, width, 500);
        QSet<QSize> sizes;
        QSet<int> chipAColumns;
        for (const SensorValueWidget *card: visibleCards(*window)) {
            sizes.insert(card->size());
            if (card->toolTip().contains(QStringLiteral("chip-a")))
                chipAColumns.insert(card->x());
        }
        QCOMPARE(sizes.size(), 1);
        columnCounts.insert(static_cast<int>(chipAColumns.size()));
    }
    // The widths covered several column layouts, and no card was rebuilt for them.
    QVERIFY(columnCounts.size() > 1);
    for (const QPointer<SensorValueWidget> &card: cards)
        QVERIFY(!card.isNull());
}

void MainWindowTest::height_limited_to_content_but_never_forced_smaller() {
    const auto window = showWindow();
    // Narrower means more rows: the first drag is clamped to the old limit and raises
    // it, the next one reaches the taller content.
    resizeWindow(*window, 600, 5000);
    resizeWindow(*window, 600, 5000);
    QVERIFY(contentFits(*window));
    const int fitted = window->height();

    // Collapsing shortens the content; the window keeps its height but cannot grow.
    section(*window, QStringLiteral("chip-a"))->setExpanded(false);
    settleEvents();
    QCOMPARE(window->height(), fitted);
    resizeWindow(*window, 600, 5000);
    QCOMPARE(window->height(), fitted);

    // Once smaller, the new limit is the shorter content.
    resizeWindow(*window, 600, 100);
    resizeWindow(*window, 600, 5000);
    QVERIFY(window->height() < fitted);
    QVERIFY(contentFits(*window));
}

void MainWindowTest::width_limited_to_full_layout() {
    const auto window = showWindow();
    // A low window shows the scrollbar, so the limit must include room for it.
    resizeWindow(*window, 5000, 150);
    QVERIFY(scrollArea(*window)->verticalScrollBar()->isVisible());
    const int limit = window->width();
    QCOMPARE(limit, window->maximumWidth());
    for (const SensorValueWidget *card: visibleCards(*window))
        QCOMPARE(card->width(), AppTheme::kCardMaxWidth);

    // One pixel less and the cards have to shrink: the limit adds no empty space.
    resizeWindow(*window, limit - 1, 150);
    QVERIFY(visibleCards(*window).first()->width() < AppTheme::kCardMaxWidth);

    // Collapsed chips still count, so collapsing does not change the limit.
    section(*window, QStringLiteral("chip-a"))->setExpanded(false);
    settleEvents();
    QCOMPARE(window->maximumWidth(), limit);

    // Maximized windows fill the screen; the limit returns afterwards.
    window->showMaximized();
    QTRY_COMPARE(window->maximumWidth(), QWIDGETSIZE_MAX);
    window->showNormal();
    QTRY_COMPARE(window->maximumWidth(), limit);
}

void MainWindowTest::source_error_stays_in_status_after_setting_change() {
    MainWindow window([] { return std::make_unique<BrokenSource>(); });
    const StatusLine *status = window.findChild<StatusLine *>();
    QVERIFY(status->text().contains(QStringLiteral("sensors_init failed (4)")));

    // Any settings change updates the status; the error must not be replaced by "No sensors found".
    for (QSpinBox *spin: window.findChildren<QSpinBox *>())
        spin->setValue(spin->value() == spin->maximum() ? spin->minimum() : spin->maximum());
    QVERIFY2(status->text().contains(QStringLiteral("sensors_init failed (4)")), qPrintable(status->text()));
}

void MainWindowTest::stalled_read_shown_in_status_until_reads_resume() {
    RuntimeConfig config;
    config.pollingIntervalSec = RuntimeConfigLimits::kMinPollingIntervalSec;
    QVERIFY(AppConfigStore::saveRuntimeConfig(config));
    const auto gate = std::make_shared<ReadGate>();
    MainWindow window([gate] { return std::make_unique<FakeSource>(gate); });
    // Declared after the window, so the gate opens before the window waits for the worker.
    const GateGuard guard{gate};
    window.findChild<SensorMonitor *>()->setStallTimeout(std::chrono::milliseconds(100));
    const StatusLine *status = window.findChild<StatusLine *>();
    QVERIFY2(status->text().startsWith(QStringLiteral("Readings: 8")), qPrintable(status->text()));

    gate->close();
    QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("not responding")), 3000);
    QVERIFY2(!status->text().contains(QStringLiteral("Readings")), qPrintable(status->text()));

    gate->open();
    QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Readings: 8")), 3000);
}

void MainWindowTest::long_status_text_does_not_change_window_width_limits() {
    // QMainWindow does not take the status bar's minimum size hint into account.
    const auto shortError = [] { return std::make_unique<BrokenSource>(QStringLiteral("x")); };
    const auto longError = [] {
        return std::make_unique<BrokenSource>(QStringLiteral("this error text is much longer than a narrow window ").repeated(6));
    };
    MainWindow shortWindow(shortError);
    MainWindow longWindow(longError);
    shortWindow.show();
    longWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&shortWindow));
    QVERIFY(QTest::qWaitForWindowExposed(&longWindow));
    settleEvents();

    QCOMPARE(longWindow.minimumSizeHint().width(), shortWindow.minimumSizeHint().width());
    QCOMPARE(longWindow.minimumWidth(), shortWindow.minimumWidth());
    resizeWindow(longWindow, 50, 400);
    QCOMPARE(longWindow.width(), shortWindow.minimumSizeHint().width());
    // Guard: the status bar itself asks for far more width than the window; the window must ignore it.
    QVERIFY(longWindow.findChild<QStatusBar *>()->minimumSizeHint().width() > longWindow.width());
}

void MainWindowTest::expand_state_is_saved_when_the_user_changes_it_not_on_startup() {
    {
        const auto window = showWindow();
        // Starting, restoring and polling write no layout state.
        QVERIFY(!QSettings().contains(SettingsKeys::kChipExpandedGroup));
        QVERIFY(!QSettings().contains(SettingsKeys::kChipOrder));

        section(*window, QStringLiteral("chip-a"))->setExpanded(false);
        // Written without closing the window, and without the window geometry.
        const MainWindowState state = MainWindowStateStore::load();
        QCOMPARE(state.chipExpanded.value(QStringLiteral("chip-a"), true), false);
        QCOMPARE(state.chipOrder, (QStringList{QStringLiteral("chip-a"), QStringLiteral("chip-b")}));
        QVERIFY(!QSettings().contains(QStringLiteral("sensors/fingerprint")));
        QVERIFY(state.geometry.isEmpty());
        window->close();
    }
    QVERIFY(!MainWindowStateStore::load().geometry.isEmpty());

    // The saved state is restored without being written again.
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.path());
    const QString file = QSettings().fileName();
    // Age the file instead of sleeping past the file system's timestamp resolution.
    {
        QFile settingsFile(file);
        QVERIFY(settingsFile.open(QIODevice::ReadWrite));
        QVERIFY(settingsFile.setFileTime(QDateTime::currentDateTime().addSecs(-60), QFileDevice::FileModificationTime));
    }
    const QDateTime before = QFileInfo(file).lastModified();
    const auto window = showWindow();
    QVERIFY(!section(*window, QStringLiteral("chip-a"))->isExpanded());
    QCOMPARE(QFileInfo(file).lastModified(), before);
}

void MainWindowTest::failed_layout_write_is_reported() {
    const auto window = showWindow();
    const StatusLine *status = window->findChild<StatusLine *>();
    QVERIFY(!status->isShowingNotice());

    QFile file(QSettings().fileName());
    QVERIFY(file.exists() || QSettings().status() == QSettings::NoError);
    // A directory in place of the settings file cannot be written.
    QSettings().sync();
    QVERIFY(QFile::remove(file.fileName()));
    QVERIFY(QDir().mkdir(file.fileName()));
    section(*window, QStringLiteral("chip-a"))->setExpanded(false);
    QVERIFY(status->isShowingNotice());
    QVERIFY(QDir().rmdir(file.fileName()));
}

void MainWindowTest::newer_schema_settings_survive_a_session_and_are_reported() {
    {
        // Written by a newer version: values this build would clamp, under a newer marker.
        QSettings s;
        s.setValue(SettingsKeys::kSchemaVersion, 99);
        s.setValue(SettingsKeys::kPollingIntervalSec, 30);
        s.setValue(SettingsKeys::kFanDefaultMaxRpm, 20000);
        s.setValue(SettingsKeys::kTemperatureUnit, QStringLiteral("K"));
        s.setValue(SettingsKeys::kChipOrder, QStringList{QStringLiteral("zzz-future-chip")});
        s.sync();
    }
    {
        const auto window = showWindow();
        const StatusLine *status = window->findChild<StatusLine *>();
        QVERIFY(status->isShowingNotice());
        QVERIFY2(status->text().contains(QStringLiteral("newer")), qPrintable(status->text()));

        // Changes in this session stay in memory only.
        for (QSpinBox *spin: window->findChildren<QSpinBox *>())
            spin->setValue(spin->value() == spin->maximum() ? spin->minimum() : spin->maximum());
        section(*window, QStringLiteral("chip-a"))->setExpanded(false);
        window->close();
    }
    const QSettings s;
    QCOMPARE(s.value(SettingsKeys::kSchemaVersion).toInt(), 99);
    QCOMPARE(s.value(SettingsKeys::kPollingIntervalSec).toInt(), 30);
    QCOMPARE(s.value(SettingsKeys::kFanDefaultMaxRpm).toInt(), 20000);
    QCOMPARE(s.value(SettingsKeys::kTemperatureUnit).toString(), QStringLiteral("K"));
    QCOMPARE(s.value(SettingsKeys::kChipOrder).toStringList(), QStringList{QStringLiteral("zzz-future-chip")});
    QVERIFY(!s.contains(SettingsKeys::kWindowGeometry));
    QVERIFY(!s.contains(QStringLiteral("sensors/fingerprint")));
}

void MainWindowTest::spin_box_steps_are_saved_once_after_the_last_step() {
    const auto window = showWindow();
    QSpinBox *polling = nullptr;
    for (QSpinBox *spin: window->findChildren<QSpinBox *>()) {
        if (spin->maximum() == RuntimeConfigLimits::kMaxPollingIntervalSec)
            polling = spin;
    }
    QVERIFY(polling != nullptr);
    for (const int value: {4, 5, 6})
        polling->setValue(value);
    // The display follows each step, the settings file only the last one, after the debounce.
    QVERIFY(!QSettings().contains(SettingsKeys::kPollingIntervalSec));
    QTRY_VERIFY_WITH_TIMEOUT(QSettings().contains(SettingsKeys::kPollingIntervalSec), 3000);
    QCOMPARE(AppConfigStore::loadRuntimeConfig().pollingIntervalSec, 6);
}

void MainWindowTest::pending_setting_change_is_saved_when_the_window_closes() {
    {
        const auto window = showWindow();
        for (QSpinBox *spin: window->findChildren<QSpinBox *>())
            spin->setValue(spin->maximum());
        QVERIFY(!QSettings().contains(SettingsKeys::kPollingIntervalSec));
        window->close();
    }
    const RuntimeConfig saved = AppConfigStore::loadRuntimeConfig();
    QCOMPARE(saved.pollingIntervalSec, RuntimeConfigLimits::kMaxPollingIntervalSec);
    QCOMPARE(saved.fanDefaultMaxRpm, RuntimeConfigLimits::kMaxFanDefaultMaxRpm);

    // Without a close event (window destroyed directly) the pending change is flushed as well.
    QSettings().clear();
    {
        const auto window = showWindow();
        for (QSpinBox *spin: window->findChildren<QSpinBox *>())
            spin->setValue(spin->minimum());
        QVERIFY(!QSettings().contains(SettingsKeys::kPollingIntervalSec));
    }
    QCOMPARE(AppConfigStore::loadRuntimeConfig().pollingIntervalSec, RuntimeConfigLimits::kMinPollingIntervalSec);
}

void MainWindowTest::sensor_title_with_markup_is_shown_literally() {
    const SensorReading reading{.chip = QStringLiteral("chip"), .category = SensorCategory::Temperatures,
                                .feature = QStringLiteral("<b>x</b>"), .value = 40.0, .unit = SensorUnit::Celsius};
    SensorValueWidget card(reading);
    card.resize(300, card.height());
    card.show();
    [[maybe_unused]] const bool exposed = QTest::qWaitForWindowExposed(&card);
    const QLabel *title = card.findChild<QLabel *>();
    QVERIFY(title != nullptr);
    QCOMPARE(title->textFormat(), Qt::PlainText);
    QCOMPARE(title->text(), QStringLiteral("<b>x</b>:"));
}

void MainWindowTest::sensor_card_height_follows_font_changes() {
    const SensorReading reading{.chip = QStringLiteral("chip"), .category = SensorCategory::Temperatures,
                                .feature = QStringLiteral("temp1"), .value = 40.0, .unit = SensorUnit::Celsius};
    SensorValueWidget card(reading);
    card.show();
    [[maybe_unused]] const bool exposed = QTest::qWaitForWindowExposed(&card);
    const int before = card.height();
    QCOMPARE(card.minimumHeight(), card.maximumHeight());

    QFont bigger = card.font();
    bigger.setPointSizeF(bigger.pointSizeF() * 2.0);
    card.setFont(bigger);
    QTRY_VERIFY(card.height() > before);
    QCOMPARE(card.height(), card.layout()->sizeHint().height());
    QCOMPARE(card.minimumHeight(), card.maximumHeight());
}

void MainWindowTest::sensor_card_tooltip_follows_every_displayed_field() {
    SensorReading reading{.chip = QStringLiteral("chip"), .category = SensorCategory::Temperatures,
                          .feature = QStringLiteral("temp1"), .value = 40.0, .unit = SensorUnit::Celsius,
                          .firmwareMax = 80.0};
    SensorValueWidget card(reading);
    QCOMPARE(card.toolTip(), SensorValueWidget::detailsToolTip(reading));

    reading.firmwareMax = 90.0;
    card.setReading(reading);
    QCOMPARE(card.toolTip(), SensorValueWidget::detailsToolTip(reading));
    reading.value = std::nullopt;
    card.setReading(reading);
    QCOMPARE(card.toolTip(), SensorValueWidget::detailsToolTip(reading));
    QVERIFY(card.toolTip().contains(QStringLiteral("cannot be read")));
}

void MainWindowTest::source_that_stops_working_is_shown_in_status() {
    RuntimeConfig config;
    config.pollingIntervalSec = RuntimeConfigLimits::kMinPollingIntervalSec;
    QVERIFY(AppConfigStore::saveRuntimeConfig(config));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("stopped working")));
    MainWindow window([] { return std::make_unique<FailingSource>(); });
    const StatusLine *status = window.findChild<StatusLine *>();
    QVERIFY2(status->text().startsWith(QStringLiteral("Readings: 1")), qPrintable(status->text()));

    QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("re-init failed (4)")), 10000);
    QCOMPARE(window.findChildren<SensorValueWidget *>().size(), 0);
}

void MainWindowTest::source_that_recovers_is_shown_again() {
    RuntimeConfig config;
    config.pollingIntervalSec = RuntimeConfigLimits::kMinPollingIntervalSec;
    QVERIFY(AppConfigStore::saveRuntimeConfig(config));
    const auto recoverable = std::make_shared<std::atomic_bool>(false);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("stopped working")));
    MainWindow window([recoverable] { return std::make_unique<FailingSource>(recoverable); });
    window.findChild<SensorMonitor *>()->setRetryDelays(std::chrono::milliseconds(50), std::chrono::milliseconds(50));
    const StatusLine *status = window.findChild<StatusLine *>();
    QVERIFY2(status->text().startsWith(QStringLiteral("Readings: 1")), qPrintable(status->text()));

    // The error stays in the status while the retries fail.
    QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("re-init failed (4)")), 10000);
    QCOMPARE(window.findChildren<SensorValueWidget *>().size(), 0);

    *recoverable = true;
    QTRY_VERIFY_WITH_TIMEOUT(status->text() == QStringLiteral("Sensors are available again"), 10000);
    QCOMPARE(window.findChildren<SensorValueWidget *>().size(), 1);
    QVERIFY(window.findChild<SensorMonitor *>()->lastError().isEmpty());
}

QTEST_MAIN(MainWindowTest)
#include "test_main_window.moc"
