// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/window_sizing.h"

#include <QtTest/QtTest>

// Verifies the maximum window height rule: never taller than the content, never forced smaller.
class WindowSizingTest final : public QObject {
    Q_OBJECT

private slots:
    void limit_is_content_height_when_window_is_smaller();
    void taller_window_is_not_forced_smaller();
    void limit_stays_within_screen();
};

void WindowSizingTest::limit_is_content_height_when_window_is_smaller() {
    QCOMPARE(WindowSizing::maximumHeight(700, 500, 1000), 700);
    QCOMPARE(WindowSizing::maximumHeight(700, 700, 1000), 700);
}

void WindowSizingTest::taller_window_is_not_forced_smaller() {
    // Content shrank (chip collapsed or more columns): the window keeps its height.
    QCOMPARE(WindowSizing::maximumHeight(400, 650, 1000), 650);
}

void WindowSizingTest::limit_stays_within_screen() {
    QCOMPARE(WindowSizing::maximumHeight(3000, 500, 1000), 1000);
    // A window taller than the screen (e.g. restored on a smaller screen) is not squeezed.
    QCOMPARE(WindowSizing::maximumHeight(3000, 1200, 1000), 1200);
}

QTEST_APPLESS_MAIN(WindowSizingTest)
#include "test_window_sizing.moc"
