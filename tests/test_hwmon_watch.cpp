// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/hwmon_watch.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

// Verifies the decision that triggers a libsensors re-init, on a fake hwmon class directory.
class HwmonWatchTest final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void unchanged_directory_is_no_change();
    void added_device_is_a_change_once();
    void removed_device_is_a_change();
    void reused_index_of_another_device_is_a_change();
    void missing_directory_is_stable();

private:
    /** Creates the device directory @p device and the class entry @p name linking to it. */
    void plug(const QString &name, const QString &device);
    void unplug(const QString &name);

    QTemporaryDir m_root;
    QString m_classDir;
};

void HwmonWatchTest::init() {
    // A fresh class directory per test function; device directories may be shared.
    static int testIndex = 0;
    QVERIFY(m_root.isValid());
    m_classDir = m_root.filePath(QStringLiteral("class-%1").arg(++testIndex));
    QVERIFY(QDir().mkpath(m_classDir));
}

void HwmonWatchTest::plug(const QString &name, const QString &device) {
    const QString devicePath = m_root.filePath(QStringLiteral("devices/%1/%2").arg(device, name));
    QVERIFY(QDir().mkpath(devicePath));
    QVERIFY(QFile::link(devicePath, QDir(m_classDir).filePath(name)));
}

void HwmonWatchTest::unplug(const QString &name) {
    QVERIFY(QFile::remove(QDir(m_classDir).filePath(name)));
}

void HwmonWatchTest::unchanged_directory_is_no_change() {
    plug(QStringLiteral("hwmon0"), QStringLiteral("coretemp"));
    HwmonWatch watch(m_classDir);
    watch.reset();
    QVERIFY(!watch.changed());
    QVERIFY(!watch.changed());
}

void HwmonWatchTest::added_device_is_a_change_once() {
    plug(QStringLiteral("hwmon0"), QStringLiteral("coretemp"));
    HwmonWatch watch(m_classDir);
    watch.reset();

    plug(QStringLiteral("hwmon1"), QStringLiteral("usb-pump"));
    QVERIFY(watch.changed());
    QVERIFY(!watch.changed()); // the new state is the snapshot now
}

void HwmonWatchTest::removed_device_is_a_change() {
    plug(QStringLiteral("hwmon0"), QStringLiteral("coretemp"));
    plug(QStringLiteral("hwmon1"), QStringLiteral("usb-pump"));
    HwmonWatch watch(m_classDir);
    watch.reset();

    unplug(QStringLiteral("hwmon1"));
    QVERIFY(watch.changed());
    QVERIFY(!watch.changed());
}

void HwmonWatchTest::reused_index_of_another_device_is_a_change() {
    plug(QStringLiteral("hwmon0"), QStringLiteral("coretemp"));
    plug(QStringLiteral("hwmon1"), QStringLiteral("usb-pump"));
    HwmonWatch watch(m_classDir);
    watch.reset();

    // Between two polls the pump is unplugged and a drive gets its index.
    unplug(QStringLiteral("hwmon1"));
    plug(QStringLiteral("hwmon1"), QStringLiteral("drivetemp"));
    QVERIFY(watch.changed());
}

void HwmonWatchTest::missing_directory_is_stable() {
    HwmonWatch watch(m_root.filePath(QStringLiteral("does-not-exist")));
    watch.reset();
    QVERIFY(!watch.changed());
}

QTEST_APPLESS_MAIN(HwmonWatchTest)
#include "test_hwmon_watch.moc"
