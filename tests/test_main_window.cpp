// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/main_window.h"
#include "ui/theme/app_theme.h"
#include "ui/widgets/collapsible_section.h"
#include "ui/widgets/sensor_value_widget.h"

#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest/QtTest>

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

private:
    QTemporaryDir m_dir;
};

namespace {
    class FakeSource final : public SensorSource {
    public:
        [[nodiscard]] bool isInitialized() const override { return true; }
        [[nodiscard]] QString lastError() const override { return {}; }

        [[nodiscard]] QVector<SensorReading> readAll() override {
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
    };

    std::unique_ptr<MainWindow> showWindow() {
        auto window = std::make_unique<MainWindow>([] { return std::make_unique<FakeSource>(); });
        window->show();
        [[maybe_unused]] const bool exposed = QTest::qWaitForWindowExposed(window.get());
        QTest::qWait(50);
        return window;
    }

    void resizeWindow(MainWindow &window, const int width, const int height) {
        window.resize(width, height);
        QTest::qWait(50);
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
    QTest::qWait(50);
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
    QTest::qWait(50);
    QCOMPARE(window->maximumWidth(), limit);

    // Maximized windows fill the screen; the limit returns afterwards.
    window->showMaximized();
    QTest::qWait(50);
    QCOMPARE(window->maximumWidth(), QWIDGETSIZE_MAX);
    window->showNormal();
    QTest::qWait(50);
    QCOMPARE(window->maximumWidth(), limit);
}

QTEST_MAIN(MainWindowTest)
#include "test_main_window.moc"
