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
    void save_layout_keeps_stored_geometry();
    void stores_do_not_write_schema_version();
    void new_settings_get_current_schema_version();
    void v1_settings_are_migrated_and_logged();
    void runtime_config_non_numeric_values_use_defaults();
    void runtime_config_out_of_range_values_are_clamped();
    void newer_schema_version_is_not_downgraded();
    void v2_settings_are_migrated_to_v3_and_the_fingerprint_removed();
    void saving_the_layout_does_not_write_the_fingerprint();
    void schema_version_above_current_is_newer();
    void temperatureUnit_token_parsing_rejects_unknown();
    void runtime_config_value_too_large_for_int_is_clamped();
    void runtime_config_temperatureUnit_non_string_falls_back_to_celsius();
    void garbled_schema_version_is_reported_and_migrated();
    void unversioned_settings_with_keys_are_migrated_and_logged();
    void main_window_state_special_chip_names_roundtrip();
    void main_window_state_corrupt_geometry_loads_as_bytes();
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
    QHash<QString, bool> expanded;
    expanded.insert(QStringLiteral("chip-a"), true);
    expanded.insert(QStringLiteral("chip-b"), false);

    const QStringList order{QStringLiteral("chip-b"), QStringLiteral("chip-a")};
    QVERIFY(MainWindowStateStore::save({.geometry = geometry, .chipExpanded = expanded, .chipOrder = order}));
    const MainWindowState loaded = MainWindowStateStore::load();

    QCOMPARE(loaded.geometry, geometry);
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

void SettingsPersistenceTest::save_layout_keeps_stored_geometry() {
    QVERIFY(MainWindowStateStore::save({.geometry = QByteArray("fake-geometry"), .chipExpanded = {}, .chipOrder = {}}));

    const QStringList order{QStringLiteral("chip-b"), QStringLiteral("chip-a")};
    QVERIFY(MainWindowStateStore::saveLayout({.geometry = QByteArray("ignored"),
                                              .chipExpanded = {{QStringLiteral("chip-a"), false}},
                                              .chipOrder = order}));
    const MainWindowState loaded = MainWindowStateStore::load();
    QCOMPARE(loaded.geometry, QByteArray("fake-geometry"));
    QCOMPARE(loaded.chipOrder, order);
    QCOMPARE(loaded.chipExpanded.value(QStringLiteral("chip-a"), true), false);
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
    QVERIFY(SettingsSchema::ensureUpToDate(s));
    QCOMPARE(SettingsSchema::storedVersion(s), SettingsSchema::kCurrentVersion);
}

void SettingsPersistenceTest::v1_settings_are_migrated_and_logged() {
    QSettings s;
    s.setValue(QStringLiteral("meta/schema_version"), 1);
    s.setValue(QStringLiteral("runtime/polling_interval_sec"), 3);

    QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral("migrated settings .* from schema v1 to v3")));
    QVERIFY(SettingsSchema::ensureUpToDate(s));

    QCOMPARE(SettingsSchema::storedVersion(s), SettingsSchema::kCurrentVersion);
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
    s.setValue(QStringLiteral("runtime/polling_interval_sec"), 30);
    s.sync();

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("newer than supported")));
    QVERIFY(SettingsSchema::ensureUpToDate(s));
    // Loading clamps in memory only and writes nothing; the main window then never saves
    // (see MainWindowTest::newer_schema_settings_survive_a_session_and_are_reported).
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("out of range")));
    QCOMPARE(AppConfigStore::loadRuntimeConfig().pollingIntervalSec, RuntimeConfigLimits::kMaxPollingIntervalSec);

    const QSettings reread;
    QCOMPARE(SettingsSchema::storedVersion(reread), SettingsSchema::kCurrentVersion + 1);
    QCOMPARE(reread.value(QStringLiteral("runtime/polling_interval_sec")).toInt(), 30);
}

void SettingsPersistenceTest::v2_settings_are_migrated_to_v3_and_the_fingerprint_removed() {
    const QByteArray geometry("fake-geometry");
    QSettings s;
    s.setValue(QStringLiteral("meta/schema_version"), 2);
    s.setValue(QStringLiteral("sensors/fingerprint"), QStringLiteral("chip-a\nchip-b"));
    s.setValue(QStringLiteral("runtime/polling_interval_sec"), 3);
    s.setValue(QStringLiteral("runtime/fan_default_max_rpm"), 4000);
    s.setValue(QStringLiteral("runtime/temperature_unit"), QStringLiteral("F"));
    s.setValue(QStringLiteral("ui/geometry"), geometry);
    s.setValue(QStringLiteral("ui/chip_order"), QStringList{QStringLiteral("chip-b"), QStringLiteral("chip-a")});
    s.setValue(QStringLiteral("ui/chips/chip-a"), false);
    s.sync();

    QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral("removed the unused settings key sensors/fingerprint")));
    QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral("migrated settings .* from schema v2 to v3")));
    QVERIFY(SettingsSchema::ensureUpToDate(s));

    const QSettings reread;
    QCOMPARE(SettingsSchema::storedVersion(reread), 3);
    QVERIFY(!reread.contains(QStringLiteral("sensors/fingerprint")));
    QCOMPARE(reread.value(QStringLiteral("runtime/polling_interval_sec")).toInt(), 3);
    QCOMPARE(reread.value(QStringLiteral("runtime/fan_default_max_rpm")).toInt(), 4000);
    QCOMPARE(reread.value(QStringLiteral("runtime/temperature_unit")).toString(), QStringLiteral("F"));
    const MainWindowState state = MainWindowStateStore::load();
    QCOMPARE(state.geometry, geometry);
    QCOMPARE(state.chipOrder, (QStringList{QStringLiteral("chip-b"), QStringLiteral("chip-a")}));
    QCOMPARE(state.chipExpanded.value(QStringLiteral("chip-a"), true), false);
    QCOMPARE(state.chipExpanded.size(), 1);
}

void SettingsPersistenceTest::saving_the_layout_does_not_write_the_fingerprint() {
    QVERIFY(MainWindowStateStore::save({.geometry = QByteArray("g"), .chipExpanded = {{QStringLiteral("chip-a"), false}},
                                        .chipOrder = {QStringLiteral("chip-a")}}));
    QVERIFY(MainWindowStateStore::saveLayout({.chipExpanded = {{QStringLiteral("chip-a"), true}}}));
    const QSettings s;
    QVERIFY(!s.contains(QStringLiteral("sensors/fingerprint")));
    for (const QString &key: s.allKeys())
        QVERIFY2(!key.contains(QStringLiteral("fingerprint")), qPrintable(key));
}

void SettingsPersistenceTest::schema_version_above_current_is_newer() {
    QCOMPARE(SettingsSchema::kCurrentVersion, 3);
    QSettings s;
    s.setValue(QStringLiteral("meta/schema_version"), 4);
    s.setValue(QStringLiteral("sensors/fingerprint"), QStringLiteral("kept"));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("newer than supported")));
    QVERIFY(SettingsSchema::ensureUpToDate(s));
    QCOMPARE(SettingsSchema::storedVersion(s), 4);
    QVERIFY(s.contains(QStringLiteral("sensors/fingerprint")));
}

void SettingsPersistenceTest::temperatureUnit_token_parsing_rejects_unknown() {
    QCOMPARE(temperatureUnitFromToken(u" f "), std::optional(TemperatureUnit::Fahrenheit));
    QCOMPARE(temperatureUnitFromToken(u"c"), std::optional(TemperatureUnit::Celsius));
    QVERIFY(!temperatureUnitFromToken(u"X").has_value());
    QVERIFY(!temperatureUnitFromToken(u"").has_value());
}

void SettingsPersistenceTest::runtime_config_value_too_large_for_int_is_clamped() {
    QSettings s;
    s.setValue(QStringLiteral("runtime/polling_interval_sec"), QStringLiteral("99999999999"));
    s.setValue(QStringLiteral("runtime/fan_default_max_rpm"), QStringLiteral("-99999999999"));
    s.sync();

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("out of range")));
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("out of range")));
    const RuntimeConfig loaded = AppConfigStore::loadRuntimeConfig();
    QCOMPARE(loaded.pollingIntervalSec, RuntimeConfigLimits::kMaxPollingIntervalSec);
    QCOMPARE(loaded.fanDefaultMaxRpm, RuntimeConfigLimits::kMinFanDefaultMaxRpm);
}

void SettingsPersistenceTest::runtime_config_temperatureUnit_non_string_falls_back_to_celsius() {
    QSettings s;
    s.setValue(QStringLiteral("runtime/temperature_unit"), 42);
    s.sync();

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("invalid setting")));
    QCOMPARE(AppConfigStore::loadRuntimeConfig().temperatureUnit, TemperatureUnit::Celsius);
}

void SettingsPersistenceTest::garbled_schema_version_is_reported_and_migrated() {
    for (const QString &garbled: {QStringLiteral("abc"), QStringLiteral("-3")}) {
        QSettings s;
        s.clear();
        s.setValue(QStringLiteral("meta/schema_version"), garbled);
        s.setValue(QStringLiteral("runtime/polling_interval_sec"), 3);
        QCOMPARE(SettingsSchema::storedVersion(s), 0);

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unreadable settings schema version")));
        QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral("migrated settings .* from schema v0 to v3")));
        QVERIFY(SettingsSchema::ensureUpToDate(s));
        QCOMPARE(SettingsSchema::storedVersion(s), SettingsSchema::kCurrentVersion);
        QCOMPARE(s.value(QStringLiteral("runtime/polling_interval_sec")).toInt(), 3);
    }
}

void SettingsPersistenceTest::unversioned_settings_with_keys_are_migrated_and_logged() {
    QSettings s;
    s.setValue(QStringLiteral("runtime/fan_default_max_rpm"), 4000);

    QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral("migrated settings .* from schema v0 to v3")));
    QVERIFY(SettingsSchema::ensureUpToDate(s));
    QCOMPARE(SettingsSchema::storedVersion(s), SettingsSchema::kCurrentVersion);
    QCOMPARE(s.value(QStringLiteral("runtime/fan_default_max_rpm")).toInt(), 4000);
    QCOMPARE(s.value(QStringLiteral("runtime/temperature_unit")).toString(), QStringLiteral("C"));
}

void SettingsPersistenceTest::main_window_state_special_chip_names_roundtrip() {
    QHash<QString, bool> expanded;
    expanded.insert(QStringLiteral("100%"), false);
    expanded.insert(QStringLiteral("a%2Fb"), false); // looks percent-encoded already
    expanded.insert(QStringLiteral("a/b"), true);
    expanded.insert(QStringLiteral("sp ace\u00e4"), false);

    QVERIFY(MainWindowStateStore::save({.chipExpanded = expanded}));
    const MainWindowState loaded = MainWindowStateStore::load();
    QCOMPARE(loaded.chipExpanded, expanded);
}

void SettingsPersistenceTest::main_window_state_corrupt_geometry_loads_as_bytes() {
    // The store does not interpret the geometry; restoreGeometry() rejects garbage.
    QSettings s;
    s.setValue(QStringLiteral("ui/geometry"), QStringLiteral("not a geometry blob"));
    s.sync();
    QCOMPARE(MainWindowStateStore::load().geometry, QByteArray("not a geometry blob"));
}

QTEST_APPLESS_MAIN(SettingsPersistenceTest)
#include "test_settings_persistence.moc"
