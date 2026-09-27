// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "status_line.h"

#include <QtTest/QtTest>

// Verifies that notices hide the permanent status until they expire, even before the widget is shown.
class StatusLineTest final : public QObject {
    Q_OBJECT

private slots:
    void permanent_text_is_shown();
    void notice_hides_permanent_updates_until_expired();
    void notice_before_show_is_not_overlapped();
};

void StatusLineTest::permanent_text_is_shown() {
    StatusLine line;
    line.setPermanentText(QStringLiteral("Readings: 3"));
    QCOMPARE(line.text(), QStringLiteral("Readings: 3"));
    QVERIFY(!line.font().bold());
}

void StatusLineTest::notice_hides_permanent_updates_until_expired() {
    StatusLine line;
    line.setPermanentText(QStringLiteral("Readings: 3"));
    line.showNotice(QStringLiteral("Settings could not be saved"), 50);
    QVERIFY(line.font().bold());

    // A poll during the notice must not replace it.
    line.setPermanentText(QStringLiteral("Readings: 4"));
    QCOMPARE(line.text(), QStringLiteral("Settings could not be saved"));

    QTRY_VERIFY_WITH_TIMEOUT(!line.isShowingNotice(), 1000);
    QCOMPARE(line.text(), QStringLiteral("Readings: 4"));
    QVERIFY(!line.font().bold());
}

void StatusLineTest::notice_before_show_is_not_overlapped() {
    StatusLine line;
    line.showNotice(QStringLiteral("Sensor layout changed"), 1000);
    line.setPermanentText(QStringLiteral("Readings: 3"));
    line.show();
    QCOMPARE(line.text(), QStringLiteral("Sensor layout changed"));
}

QTEST_MAIN(StatusLineTest)
#include "test_status_line.moc"
