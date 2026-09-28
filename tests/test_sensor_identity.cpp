// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/sensor_identity.h"

#include <QtTest/QtTest>

// Verifies SensorIdentity::chipFingerprint and SensorIdentity::sensorKey.
class SensorIdentityTest final : public QObject {
    Q_OBJECT

private slots:
    void chipFingerprint_deduplicates_chips();
    void chipFingerprint_is_order_independent();
    void chipFingerprint_empty_readings();
    void sensorKey_same_reading_same_key();
    void sensorKey_different_subfeature_different_key();
    void sensorKey_different_category_different_key();
    void sensorKey_different_chip_different_key();
    void sensorKey_ignores_unit_and_label();
};

void SensorIdentityTest::chipFingerprint_deduplicates_chips() {
    QVector<SensorReading> readings;
    readings.push_back({.chip = QStringLiteral("coretemp-isa-0000")});
    readings.push_back({.chip = QStringLiteral("coretemp-isa-0000")});
    readings.push_back({.chip = QStringLiteral("nct6798d-isa-0290")});

    const QString fp = SensorIdentity::chipFingerprint(readings);
    const QStringList lines = fp.split(QStringLiteral("\n"));
    QCOMPARE(lines.size(), 2);
    QVERIFY(lines.contains(QStringLiteral("coretemp-isa-0000")));
    QVERIFY(lines.contains(QStringLiteral("nct6798d-isa-0290")));
}

void SensorIdentityTest::chipFingerprint_is_order_independent() {
    QVector<SensorReading> order1;
    order1.push_back({.chip = QStringLiteral("chip-a")});
    order1.push_back({.chip = QStringLiteral("chip-b")});

    QVector<SensorReading> order2;
    order2.push_back({.chip = QStringLiteral("chip-b")});
    order2.push_back({.chip = QStringLiteral("chip-a")});

    QCOMPARE(SensorIdentity::chipFingerprint(order1), SensorIdentity::chipFingerprint(order2));
}

void SensorIdentityTest::chipFingerprint_empty_readings() {
    QCOMPARE(SensorIdentity::chipFingerprint({}), QString());
}

void SensorIdentityTest::sensorKey_same_reading_same_key() {
    const SensorReading r{
        .chip = QStringLiteral("coretemp-isa-0000"),
        .category = SensorCategory::Temperatures,
        .feature = QStringLiteral("Core 0"),
        .featureNumber = 1,
        .subfeatureNumber = 2,
        .unit = SensorUnit::Celsius,
    };
    QCOMPARE(SensorIdentity::sensorKey(r), SensorIdentity::sensorKey(r));
}

void SensorIdentityTest::sensorKey_different_subfeature_different_key() {
    SensorReading r{
        .chip = QStringLiteral("chip"),
        .category = SensorCategory::Temperatures,
        .feature = QStringLiteral("Core 0"),
        .featureNumber = 1,
        .subfeatureNumber = 2,
        .unit = SensorUnit::Celsius,
    };
    const QString key1 = SensorIdentity::sensorKey(r);
    r.subfeatureNumber = 3;
    const QString key2 = SensorIdentity::sensorKey(r);
    QVERIFY(key1 != key2);
}

void SensorIdentityTest::sensorKey_different_category_different_key() {
    SensorReading r{
        .chip = QStringLiteral("chip"),
        .category = SensorCategory::Temperatures,
        .feature = QStringLiteral("temp1"),
        .featureNumber = 0,
        .subfeatureNumber = 0,
        .unit = SensorUnit::Celsius,
    };
    const QString key1 = SensorIdentity::sensorKey(r);
    r.category = SensorCategory::Voltages;
    const QString key2 = SensorIdentity::sensorKey(r);
    QVERIFY(key1 != key2);
}

void SensorIdentityTest::sensorKey_different_chip_different_key() {
    SensorReading r{
        .chip = QStringLiteral("chip-a"),
        .category = SensorCategory::Fans,
        .feature = QStringLiteral("fan1"),
        .featureNumber = 0,
        .subfeatureNumber = 0,
        .unit = SensorUnit::Rpm,
    };
    const QString key1 = SensorIdentity::sensorKey(r);
    r.chip = QStringLiteral("chip-b");
    const QString key2 = SensorIdentity::sensorKey(r);
    QVERIFY(key1 != key2);
}

void SensorIdentityTest::sensorKey_ignores_unit_and_label() {
    SensorReading r{
        .chip = QStringLiteral("macsmc_battery-isa-0000"),
        .category = SensorCategory::Currents,
        .feature = QStringLiteral("curr1"),
        .featureNumber = 3,
        .subfeatureNumber = 7,
        .unit = SensorUnit::Ampere,
    };
    const QString key = SensorIdentity::sensorKey(r);
    r.unit = SensorUnit::Milliampere;
    r.feature = QStringLiteral("Battery Current");
    QCOMPARE(SensorIdentity::sensorKey(r), key);
}

QTEST_APPLESS_MAIN(SensorIdentityTest)
#include "test_sensor_identity.moc"
