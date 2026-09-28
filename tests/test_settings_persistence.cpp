// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "config/app_config_store.h"
#include "ui/main_window_state_store.h"
#include "config/settings_schema.h"

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>

// Verifies runtime config and main-window state persistence roundtrips via QSettings.
class SettingsPersistenceTest final : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;

private slots:
    void init();
    void runtime_config_roundtrip();
    void runtime_config_temperatureUnit_token_roundtrip();
    void runtime_config_temperatureUnit_invalid_fallbacks_to_celsius();
    void main_window_state_roundtrip();
    void main_window_state_chip_name_with_slash_roundtrip();
    void stores_do_not_write_schema_version();
    void new_settings_get_current_schema_version();
    void v1_settings_are_migrated_and_logged();
    void runtime_config_non_numeric_values_use_defaults();
    void runtime_config_out_of_range_values_are_clamped();
    void newer_schema_version_is_not_downgraded();
    void temperatureUnit_token_parsing_rejects_unknown();
};

void SettingsPersistenceTest::init() {
    QVERIFY2(m_dir.isValid(), "Temporary settings directory must be valid");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.path());
    QCoreApplication::setOrganizationName(QStringLiteral("qsensors-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("persistence"));

    QSettings s;
    s.clear();
}

void SettingsPersistenceTest::runtime_config_roundtrip() {
    RuntimeConfig written;
    written.pollingIntervalSec = 7;
    written.fanDefaultMaxRpm = 6400;
    written.temperatureUnit = TemperatureUnit::Fahrenheit;
    QVERIFY(AppConfigStore::saveRuntimeConfig(written));

    const RuntimeConfig loaded = AppConfigStore::loadRuntimeConfig();
    QCOMPARE(loaded.pollingIntervalSec, 7);
    QCOMPARE(loaded.fanDefaultMaxRpm, 6400);
    QCOMPARE(loaded.temperatureUnit, TemperatureUnit::Fahrenheit);
}

void SettingsPersistenceTest::runtime_config_temperatureUnit_token_roundtrip() {
    RuntimeConfig written;
    written.temperatureUnit = TemperatureUnit::Fahrenheit;
    QVERIFY(AppConfigStore::saveRuntimeConfig(written));

    QSettings s;
    QCOMPARE(s.value(QStringLiteral("runtime/temperature_unit")).toString(), QStringLiteral("F"));

    written.temperatureUnit = TemperatureUnit::Celsius;
    QVERIFY(AppConfigStore::saveRuntimeConfig(written));
    QCOMPARE(s.value(QStringLiteral("runtime/temperature_unit")).toString(), QStringLiteral("C"));
}

void SettingsPersistenceTest::runtime_config_temperatureUnit_invalid_fallbacks_to_celsius() {
    QSettings s;
    s.setValue(QStringLiteral("runtime/temperature_unit"), QStringLiteral("X"));

    const RuntimeConfig loaded = AppConfigStore::loadRuntimeConfig();
    QCOMPARE(loaded.temperatureUnit, TemperatureUnit::Celsius);
}

void SettingsPersistenceTest::main_window_state_roundtrip() {
    const QByteArray geometry("fake-geometry");
    const QString fingerprint = QStringLiteral("chip-a\\nchip-b");
    QHash<QString, bool> expanded;
    expanded.insert(QStringLiteral("chip-a"), true);
    expanded.insert(QStringLiteral("chip-b"), false);

    const QStringList order{QStringLiteral("chip-b"), QStringLiteral("chip-a")};
    QVERIFY(MainWindowStateStore::save({.geometry = geometry, .chipExpanded = expanded, .chipOrder = order,
                                        .sensorFingerprint = fingerprint}));
    const MainWindowState loaded = MainWindowStateStore::load();

    QCOMPARE(loaded.geometry, geometry);
    QCOMPARE(loaded.sensorFingerprint, fingerprint);
    QCOMPARE(loaded.chipExpanded.value(QStringLiteral("chip-a")), true);
    QCOMPARE(loaded.chipExpanded.value(QStringLiteral("chip-b")), false);
    QCOMPARE(loaded.chipOrder, order);
}

void SettingsPersistenceTest::main_window_state_chip_name_with_slash_roundtrip() {
    // Chip names containing '/' must not be interpreted as QSettings group separators.
    QHash<QString, bool> expanded;
    expanded.insert(QStringLiteral("bus/0"), true);
    expanded.insert(QStringLiteral("pci/slot/2"), false);
    expanded.insert(QStringLiteral("normal-chip-isa-0000"), true);

    QVERIFY(MainWindowStateStore::save({.chipExpanded = expanded}));
    const MainWindowState loaded = MainWindowStateStore::load();

    QCOMPARE(loaded.chipExpanded.value(QStringLiteral("bus/0")), true);
    QCOMPARE(loaded.chipExpanded.value(QStringLiteral("pci/slot/2")), false);
    QCOMPARE(loaded.chipExpanded.value(QStringLiteral("normal-chip-isa-0000")), true);
    // No group separator artifacts: exactly the three names come back.
    QCOMPARE(loaded.chipExpanded.size(), 3);
}

void SettingsPersistenceTest::stores_do_not_write_schema_version() {
    // Migration happens once at startup; reading settings must not write anything.
    (void) AppConfigStore::loadRuntimeConfig();
    (void) MainWindowStateStore::load();

    QSettings s;
    QVERIFY(s.allKeys().isEmpty());
}

void SettingsPersistenceTest::new_settings_get_current_schema_version() {
    QSettings s;
    SettingsSchema::ensureUpToDate(s);
    QCOMPARE(SettingsSchema::storedVersion(s), SettingsSchema::kCurrentVersion);
}

void SettingsPersistenceTest::v1_settings_are_migrated_and_logged() {
    QSettings s;
    s.setValue(QStringLiteral("meta/schema_version"), 1);
    s.setValue(QStringLiteral("runtime/polling_interval_sec"), 3);

    QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral("migrated settings .* from schema v1 to v2")));
    SettingsSchema::ensureUpToDate(s);

    QCOMPARE(SettingsSchema::storedVersion(s), 2);
    QCOMPARE(s.value(QStringLiteral("runtime/temperature_unit")).toString(), QStringLiteral("C"));
    QCOMPARE(s.value(QStringLiteral("runtime/polling_interval_sec")).toInt(), 3);
}

void SettingsPersistenceTest::runtime_config_non_numeric_values_use_defaults() {
    QSettings s;
    s.setValue(QStringLiteral("runtime/polling_interval_sec"), QStringLiteral("fast"));
    s.setValue(QStringLiteral("runtime/fan_default_max_rpm"), QStringLiteral(""));
    s.sync();

    const RuntimeConfig loaded = AppConfigStore::loadRuntimeConfig();
    QCOMPARE(loaded.pollingIntervalSec, RuntimeConfigLimits::kDefaultPollingIntervalSec);
    QCOMPARE(loaded.fanDefaultMaxRpm, RuntimeConfigLimits::kDefaultFanDefaultMaxRpm);
}

void SettingsPersistenceTest::runtime_config_out_of_range_values_are_clamped() {
    QSettings s;
    s.setValue(QStringLiteral("runtime/polling_interval_sec"), 99);
    s.setValue(QStringLiteral("runtime/fan_default_max_rpm"), 10);
    s.sync();

    const RuntimeConfig loaded = AppConfigStore::loadRuntimeConfig();
    QCOMPARE(loaded.pollingIntervalSec, RuntimeConfigLimits::kMaxPollingIntervalSec);
    QCOMPARE(loaded.fanDefaultMaxRpm, RuntimeConfigLimits::kMinFanDefaultMaxRpm);
}

void SettingsPersistenceTest::newer_schema_version_is_not_downgraded() {
    QSettings s;
    s.setValue(QStringLiteral("meta/schema_version"), SettingsSchema::kCurrentVersion + 1);
    s.sync();

    SettingsSchema::ensureUpToDate(s);
    (void) AppConfigStore::loadRuntimeConfig();
    QVERIFY(AppConfigStore::saveRuntimeConfig(RuntimeConfig{}));

    QSettings reread;
    QCOMPARE(SettingsSchema::storedVersion(reread), SettingsSchema::kCurrentVersion + 1);
}

void SettingsPersistenceTest::temperatureUnit_token_parsing_rejects_unknown() {
    QCOMPARE(temperatureUnitFromToken(u" f "), std::optional(TemperatureUnit::Fahrenheit));
    QCOMPARE(temperatureUnitFromToken(u"c"), std::optional(TemperatureUnit::Celsius));
    QVERIFY(!temperatureUnitFromToken(u"X").has_value());
    QVERIFY(!temperatureUnitFromToken(u"").has_value());
}

QTEST_APPLESS_MAIN(SettingsPersistenceTest)
#include "test_settings_persistence.moc"
