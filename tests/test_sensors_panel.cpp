// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors_panel.h"
#include "collapsible_section.h"
#include "sensor_identity.h"
#include "sensor_value_widget.h"

#include <QApplication>
#include <algorithm>
#include <QLayout>
#include <QPointer>
#include <QSignalSpy>
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
    void structure_signal_on_new_category_not_on_values();
    void restored_state_dropped_for_other_chip_set();
    void restored_state_kept_for_same_chip_set();
    void fingerprint_and_count_follow_readings();
    void default_order_is_alphabetical();
    void move_chip_reorders_cards();
    void preferred_order_puts_new_chips_last_and_keeps_absent_ones();
    void header_click_still_toggles_when_draggable();
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

    // Chip names of the cards from top to bottom, as laid out.
    QStringList cardOrder(SensorsPanel &panel) {
        panel.resize(900, 900);
        panel.layout()->activate();
        QList<CollapsibleSection *> sections = panel.findChildren<CollapsibleSection *>();
        std::sort(sections.begin(), sections.end(),
                  [](const QWidget *a, const QWidget *b) { return a->y() < b->y(); });
        QStringList names;
        for (const CollapsibleSection *section: sections)
            names << section->findChild<QToolButton *>()->text();
        return names;
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

void SensorsPanelTest::structure_signal_on_new_category_not_on_values() {
    SensorsPanel panel;
    QSignalSpy spy(&panel, &SensorsPanel::structureChanged);
    panel.setReadings(sampleReadings(40.0), 800);
    QCOMPARE(spy.count(), 1);

    panel.setReadings(sampleReadings(45.0), 800);
    QCOMPARE(spy.count(), 1);

    // Same chip set, but chip-b grows to three categories (more than chip-a's two).
    QVector<SensorReading> readings = sampleReadings(45.0);
    readings.append({.chip = QStringLiteral("chip-b"), .category = SensorCategory::Voltages,
                     .feature = QStringLiteral("in0"), .featureNumber = 1, .subfeatureNumber = 2,
                     .value = 12.0, .unit = SensorUnit::Volt, .minValue = 11.0, .maxValue = 13.0});
    readings.append({.chip = QStringLiteral("chip-b"), .category = SensorCategory::Power,
                     .feature = QStringLiteral("power1"), .featureNumber = 2, .subfeatureNumber = 3,
                     .value = 5.0, .unit = SensorUnit::Watt, .maxValue = 60.0});
    const int widthBefore = panel.minimumRequiredWidth();
    panel.setReadings(readings, 800);
    QCOMPARE(spy.count(), 2);
    QVERIFY(panel.minimumRequiredWidth() > widthBefore);
}

void SensorsPanelTest::restored_state_dropped_for_other_chip_set() {
    SensorsPanel panel;
    QSignalSpy spy(&panel, &SensorsPanel::layoutStateReset);
    panel.restoreChipExpandedState({{QStringLiteral("chip-a"), false}}, QStringLiteral("old-chip"));
    panel.setReadings(sampleReadings(40.0), 800);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.chipExpandedState().value(QStringLiteral("chip-a")), true);

    // The check runs once; later polls do not reset again.
    panel.setReadings(sampleReadings(41.0), 800);
    QCOMPARE(spy.count(), 1);
}

void SensorsPanelTest::restored_state_kept_for_same_chip_set() {
    const QVector<SensorReading> readings = sampleReadings(40.0);
    SensorsPanel panel;
    QSignalSpy spy(&panel, &SensorsPanel::layoutStateReset);
    panel.restoreChipExpandedState({{QStringLiteral("chip-a"), false}}, SensorIdentity::chipFingerprint(readings));
    panel.setReadings(readings, 800);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(panel.chipExpandedState().value(QStringLiteral("chip-a")), false);
}

void SensorsPanelTest::fingerprint_and_count_follow_readings() {
    const QVector<SensorReading> readings = sampleReadings(40.0);
    SensorsPanel panel;
    QCOMPARE(panel.readingCount(), 0);
    QVERIFY(panel.chipFingerprint().isEmpty());

    panel.setReadings(readings, 800);
    QCOMPARE(panel.readingCount(), 3);
    QCOMPARE(panel.chipFingerprint(), SensorIdentity::chipFingerprint(readings));
}

void SensorsPanelTest::default_order_is_alphabetical() {
    SensorsPanel panel;
    panel.setReadings(sampleReadings(40.0), 800);
    QCOMPARE(panel.chipOrder(), (QStringList{QStringLiteral("chip-a"), QStringLiteral("chip-b")}));
    QCOMPARE(cardOrder(panel), (QStringList{QStringLiteral("chip-a"), QStringLiteral("chip-b")}));
}

void SensorsPanelTest::move_chip_reorders_cards() {
    SensorsPanel panel;
    panel.setReadings(sampleReadings(40.0), 800);

    panel.moveChip(QStringLiteral("chip-b"), 0);
    QCOMPARE(panel.chipOrder(), (QStringList{QStringLiteral("chip-b"), QStringLiteral("chip-a")}));
    QCOMPARE(cardOrder(panel), (QStringList{QStringLiteral("chip-b"), QStringLiteral("chip-a")}));

    // Target is the insert position before the move: 2 means "after the last card".
    panel.moveChip(QStringLiteral("chip-b"), 2);
    QCOMPARE(cardOrder(panel), (QStringList{QStringLiteral("chip-a"), QStringLiteral("chip-b")}));

    // The order survives value updates.
    panel.moveChip(QStringLiteral("chip-b"), 0);
    panel.setReadings(sampleReadings(45.0), 800);
    QCOMPARE(cardOrder(panel), (QStringList{QStringLiteral("chip-b"), QStringLiteral("chip-a")}));

    panel.moveChip(QStringLiteral("unknown"), 0);
    QCOMPARE(cardOrder(panel), (QStringList{QStringLiteral("chip-b"), QStringLiteral("chip-a")}));
}

void SensorsPanelTest::preferred_order_puts_new_chips_last_and_keeps_absent_ones() {
    QVector<SensorReading> readings = sampleReadings(40.0);
    readings.append({.chip = QStringLiteral("chip-c"), .category = SensorCategory::Fans, .feature = QStringLiteral("fan1"),
                     .featureNumber = 0, .subfeatureNumber = 1, .value = 900.0, .unit = SensorUnit::Rpm});
    SensorsPanel panel;
    panel.setChipOrder({QStringLiteral("gone"), QStringLiteral("chip-b")});
    panel.setReadings(readings, 800);

    QCOMPARE(cardOrder(panel),
             (QStringList{QStringLiteral("chip-b"), QStringLiteral("chip-a"), QStringLiteral("chip-c")}));
    // A chip that is currently missing keeps its saved preference.
    QCOMPARE(panel.chipOrder(), (QStringList{QStringLiteral("chip-b"), QStringLiteral("chip-a"),
                                             QStringLiteral("chip-c"), QStringLiteral("gone")}));
}

void SensorsPanelTest::header_click_still_toggles_when_draggable() {
    SensorsPanel panel;
    panel.setReadings(sampleReadings(40.0), 800);
    QToolButton *header = headerFor(panel, QStringLiteral("chip-a"));
    QCOMPARE(header->cursor().shape(), Qt::OpenHandCursor);
    QVERIFY(header->isChecked());

    QTest::mouseClick(header, Qt::LeftButton);
    QVERIFY(!header->isChecked());
    QCOMPARE(panel.chipExpandedState().value(QStringLiteral("chip-a")), false);
}

QTEST_MAIN(SensorsPanelTest)
#include "test_sensors_panel.moc"
