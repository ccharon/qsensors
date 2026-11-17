// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/sensor_identity.h"

#include <QtTest/QtTest>

// Verifies SensorIdentity::sensorKey.
class SensorIdentityTest final : public QObject {
    Q_OBJECT

private slots:
    void sensorKey_same_reading_same_key();
    void sensorKey_different_subfeature_different_key();
    void sensorKey_different_category_different_key();
    void sensorKey_different_chip_different_key();
    void sensorKey_ignores_unit_and_label();
    void uniqueChipNames_keeps_unique_names();
    void uniqueChipNames_numbers_repeated_names_by_stable_key();
    void uniqueChipNames_is_stable_across_listing_order();
    void uniqueChipNames_avoids_existing_names();
    void uniqueChipNames_separates_sensor_keys();
};

namespace {
    /** Stable keys by index, recording which indexes were asked for. */
    struct KeyLookup {
        QStringList keys;
        QList<qsizetype> *asked;

        QString operator()(const qsizetype i) const {
            asked->append(i);
            return keys.at(i);
        }
    };
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
    r.scale = SensorScale{.prefix = SiPrefix::Milli, .decimals = 0};
    r.feature = QStringLiteral("Battery Current");
    QCOMPARE(SensorIdentity::sensorKey(r), key);
}

void SensorIdentityTest::uniqueChipNames_keeps_unique_names() {
    const QStringList names{QStringLiteral("coretemp-isa-0000"), QStringLiteral("nvme-pci-0100")};
    QList<qsizetype> asked;
    QCOMPARE(SensorIdentity::uniqueChipNames(names, KeyLookup{{}, &asked}), names);
    QVERIFY(asked.isEmpty()); // no filesystem lookups for unique names
}

void SensorIdentityTest::uniqueChipNames_numbers_repeated_names_by_stable_key() {
    const QStringList names{QStringLiteral("acpi_fan-acpi-0"), QStringLiteral("coretemp-isa-0000"),
                            QStringLiteral("acpi_fan-acpi-0"), QStringLiteral("acpi_fan-acpi-0")};
    const QStringList keys{QStringLiteral("/sys/devices/PNP0C0B:02"), QString(),
                           QStringLiteral("/sys/devices/PNP0C0B:00"), QStringLiteral("/sys/devices/PNP0C0B:01")};
    QList<qsizetype> asked;
    const QStringList unique = SensorIdentity::uniqueChipNames(names, KeyLookup{keys, &asked});
    QCOMPARE(unique, (QStringList{QStringLiteral("acpi_fan-acpi-0 #3"), QStringLiteral("coretemp-isa-0000"),
                                  QStringLiteral("acpi_fan-acpi-0"), QStringLiteral("acpi_fan-acpi-0 #2")}));
    QVERIFY(!asked.contains(1));
}

void SensorIdentityTest::uniqueChipNames_is_stable_across_listing_order() {
    // libsensors may list the same chips in another order after a re-init or a restart.
    const QStringList names{QStringLiteral("acpi_fan-acpi-0"), QStringLiteral("acpi_fan-acpi-0")};
    QList<qsizetype> asked;
    const QStringList first = SensorIdentity::uniqueChipNames(names, KeyLookup{{QStringLiteral("a"), QStringLiteral("b")}, &asked});
    const QStringList swapped = SensorIdentity::uniqueChipNames(names, KeyLookup{{QStringLiteral("b"), QStringLiteral("a")}, &asked});
    QCOMPARE(first, (QStringList{QStringLiteral("acpi_fan-acpi-0"), QStringLiteral("acpi_fan-acpi-0 #2")}));
    QCOMPARE(swapped, (QStringList{QStringLiteral("acpi_fan-acpi-0 #2"), QStringLiteral("acpi_fan-acpi-0")}));
}

void SensorIdentityTest::uniqueChipNames_avoids_existing_names() {
    const QStringList names{QStringLiteral("x"), QStringLiteral("x #2"), QStringLiteral("x")};
    QList<qsizetype> asked;
    const QStringList unique = SensorIdentity::uniqueChipNames(names, KeyLookup{{QStringLiteral("a"), {}, QStringLiteral("b")}, &asked});
    QCOMPARE(unique, (QStringList{QStringLiteral("x"), QStringLiteral("x #2"), QStringLiteral("x #3")}));
}

void SensorIdentityTest::uniqueChipNames_separates_sensor_keys() {
    // Two ACPI fans with the same printed name and numbering get separate cards.
    const QStringList unique = SensorIdentity::uniqueChipNames(
        {QStringLiteral("acpi_fan-acpi-0"), QStringLiteral("acpi_fan-acpi-0")},
        [](const qsizetype i) { return QString::number(i); });
    SensorReading a{.chip = unique.at(0), .category = SensorCategory::Fans, .featureNumber = 0, .subfeatureNumber = 0};
    SensorReading b = a;
    b.chip = unique.at(1);
    QVERIFY(SensorIdentity::sensorKey(a) != SensorIdentity::sensorKey(b));
}

QTEST_APPLESS_MAIN(SensorIdentityTest)
#include "test_sensor_identity.moc"
