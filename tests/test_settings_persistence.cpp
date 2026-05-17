// SPDX-License-Identifier: GPL-2.0-or-later

#include "app_config_store.h"
#include "main_window_state_store.h"

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
    void schema_version_is_written();
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
    AppConfigStore::saveRuntimeConfig(written);

    const RuntimeConfig loaded = AppConfigStore::loadRuntimeConfig();
    QCOMPARE(loaded.pollingIntervalSec, 7);
    QCOMPARE(loaded.fanDefaultMaxRpm, 6400);
    QCOMPARE(loaded.temperatureUnit, TemperatureUnit::Fahrenheit);
}

void SettingsPersistenceTest::runtime_config_temperatureUnit_token_roundtrip() {
    RuntimeConfig written;
    written.temperatureUnit = TemperatureUnit::Fahrenheit;
    AppConfigStore::saveRuntimeConfig(written);

    QSettings s;
    QCOMPARE(s.value(QStringLiteral("runtime/temperature_unit")).toString(), QStringLiteral("F"));

    written.temperatureUnit = TemperatureUnit::Celsius;
    AppConfigStore::saveRuntimeConfig(written);
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

    MainWindowStateStore::save(geometry, fingerprint, expanded);
    const MainWindowState loaded = MainWindowStateStore::load();

    QVERIFY(loaded.hasGeometry);
    QCOMPARE(loaded.geometry, geometry);
    QCOMPARE(loaded.sensorFingerprint, fingerprint);
    QCOMPARE(loaded.chipExpanded.value(QStringLiteral("chip-a")), true);
    QCOMPARE(loaded.chipExpanded.value(QStringLiteral("chip-b")), false);
}

void SettingsPersistenceTest::schema_version_is_written() {
    // Trigger both stores to ensure schema plumbing is exercised regardless of call-site order.
    (void) AppConfigStore::loadRuntimeConfig();
    (void) MainWindowStateStore::load();

    QSettings s;
    QCOMPARE(s.value(QStringLiteral("meta/schema_version")).toInt(), 2);
}

QTEST_APPLESS_MAIN(SettingsPersistenceTest)
#include "test_settings_persistence.moc"
