// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors_panel.h"
#include "sensor_value_widget.h"

#include <QApplication>
#include <QPointer>
#include <QToolButton>
#include <QtTest/QtTest>

// Verifies that SensorsPanel keeps widgets across value updates and owns the expand state.
class SensorsPanelTest final : public QObject {
    Q_OBJECT

private slots:
    void value_and_unit_updates_keep_widgets();
    void relayout_with_same_columns_keeps_widgets();
    void relayout_with_other_columns_rebuilds();
    void expand_state_roundtrip_and_apply();
};

namespace {
    QVector<SensorReading> sampleReadings(const double value, const SensorUnit currentUnit = SensorUnit::Ampere) {
        return {
            {.chip = QStringLiteral("chip-a"), .category = SensorCategory::Temperatures, .feature = QStringLiteral("temp1"),
             .featureNumber = 0, .subfeatureNumber = 1, .value = value, .unit = SensorUnit::Celsius, .maxValue = 100.0},
            {.chip = QStringLiteral("chip-a"), .category = SensorCategory::Currents, .feature = QStringLiteral("curr1"),
             .featureNumber = 1, .subfeatureNumber = 2, .value = 0.4, .unit = currentUnit, .maxValue = 5.0},
            {.chip = QStringLiteral("chip-b"), .category = SensorCategory::Fans, .feature = QStringLiteral("fan1"),
             .featureNumber = 0, .subfeatureNumber = 1, .value = 1200.0, .unit = SensorUnit::Rpm, .minValue = 500.0},
        };
    }

    QList<SensorValueWidget *> cards(const SensorsPanel &panel) {
        return panel.findChildren<SensorValueWidget *>();
    }

    QToolButton *headerFor(const SensorsPanel &panel, const QString &chip) {
        for (QToolButton *button: panel.findChildren<QToolButton *>())
            if (button->text() == chip)
                return button;
        return nullptr;
    }
}

void SensorsPanelTest::value_and_unit_updates_keep_widgets() {
    SensorsPanel panel;
    panel.setReadings(sampleReadings(40.0), 800);
    const QList<SensorValueWidget *> before = cards(panel);
    QCOMPARE(before.size(), 3);

    panel.setReadings(sampleReadings(55.0, SensorUnit::Milliampere), 800);
    QCOMPARE(cards(panel), before);
}

void SensorsPanelTest::relayout_with_same_columns_keeps_widgets() {
    SensorsPanel panel;
    panel.setReadings(sampleReadings(40.0), 800);
    const QList<SensorValueWidget *> before = cards(panel);

    panel.relayout(810);
    QCOMPARE(cards(panel), before);
}

void SensorsPanelTest::relayout_with_other_columns_rebuilds() {
    // Two sensors in one category so the column count matters.
    QVector<SensorReading> readings = sampleReadings(40.0);
    readings.append({.chip = QStringLiteral("chip-a"), .category = SensorCategory::Temperatures,
                     .feature = QStringLiteral("temp2"), .featureNumber = 2, .subfeatureNumber = 3,
                     .value = 41.0, .unit = SensorUnit::Celsius, .maxValue = 100.0});
    SensorsPanel panel;
    panel.setReadings(readings, 300);
    // QPointer detects deletion reliably; raw addresses may be reused by new widgets.
    const QPointer<SensorValueWidget> narrowCard = cards(panel).first();

    panel.relayout(1600);
    QVERIFY(narrowCard.isNull());
    QCOMPARE(cards(panel).size(), 4);
}

void SensorsPanelTest::expand_state_roundtrip_and_apply() {
    SensorsPanel panel;
    panel.setChipExpandedState({{QStringLiteral("chip-b"), false}});
    panel.setReadings(sampleReadings(40.0), 800);

    QHash<QString, bool> state = panel.chipExpandedState();
    QCOMPARE(state.value(QStringLiteral("chip-a")), true);
    QCOMPARE(state.value(QStringLiteral("chip-b")), false);
    QVERIFY(!headerFor(panel, QStringLiteral("chip-b"))->isChecked());

    // Applying a state to existing sections updates headers and the stored state.
    panel.setChipExpandedState({{QStringLiteral("chip-a"), false}});
    QVERIFY(!headerFor(panel, QStringLiteral("chip-a"))->isChecked());
    QVERIFY(headerFor(panel, QStringLiteral("chip-b"))->isChecked());
    state = panel.chipExpandedState();
    QCOMPARE(state.value(QStringLiteral("chip-a")), false);
    QCOMPARE(state.value(QStringLiteral("chip-b")), true);

    // User toggles are reflected as well.
    headerFor(panel, QStringLiteral("chip-a"))->toggle();
    QCOMPARE(panel.chipExpandedState().value(QStringLiteral("chip-a")), true);
}

QTEST_MAIN(SensorsPanelTest)
#include "test_sensors_panel.moc"
