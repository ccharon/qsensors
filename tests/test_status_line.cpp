// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/widgets/status_line.h"

#include <QtTest/QtTest>

// Verifies that notices hide the permanent status until they expire, even before the widget is shown.
class StatusLineTest final : public QObject {
    Q_OBJECT

private slots:
    void permanent_text_is_shown();
    void notice_hides_permanent_updates_until_expired();
    void notice_before_show_is_not_overlapped();
    void notices_are_shown_one_after_another();
    void permanent_text_waits_for_all_queued_notices();
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

void StatusLineTest::notices_are_shown_one_after_another() {
    StatusLine line;
    line.showNotice(QStringLiteral("first"), 50);
    line.showNotice(QStringLiteral("second"), 50);
    line.showNotice(QStringLiteral("third"), 50);
    QCOMPARE(line.text(), QStringLiteral("first"));

    QTRY_COMPARE_WITH_TIMEOUT(line.text(), QStringLiteral("second"), 1000);
    QVERIFY(line.font().bold());
    QVERIFY(line.isShowingNotice());
    QTRY_COMPARE_WITH_TIMEOUT(line.text(), QStringLiteral("third"), 1000);
}

void StatusLineTest::permanent_text_waits_for_all_queued_notices() {
    StatusLine line;
    line.setPermanentText(QStringLiteral("Readings: 3"));
    line.showNotice(QStringLiteral("first"), 40);
    line.showNotice(QStringLiteral("second"), 40);
    line.setPermanentText(QStringLiteral("Readings: 4"));
    QCOMPARE(line.text(), QStringLiteral("first"));

    QTRY_COMPARE_WITH_TIMEOUT(line.text(), QStringLiteral("second"), 1000);
    QCOMPARE(line.text(), QStringLiteral("second"));
    QTRY_VERIFY_WITH_TIMEOUT(!line.isShowingNotice(), 1000);
    QCOMPARE(line.text(), QStringLiteral("Readings: 4"));
    QVERIFY(!line.font().bold());
}

QTEST_MAIN(StatusLineTest)
#include "test_status_line.moc"
