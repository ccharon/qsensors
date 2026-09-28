// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/panels/card_grid_plan.h"
#include "ui/panels/sensors_panel.h"
#include "ui/widgets/collapsible_section.h"
#include "sensors/sensor_identity.h"
#include "ui/widgets/sensor_value_widget.h"
#include "ui/theme/app_theme.h"

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
    void resize_moves_cards_without_rebuild();
    void expand_state_roundtrip_and_apply();
    void structure_signal_on_new_category_not_on_values();
    void restored_state_dropped_for_other_chip_set();
    void restored_state_kept_for_same_chip_set();
    void fingerprint_and_count_follow_readings();
    void default_order_is_alphabetical();
    void move_chip_reorders_cards();
    void preferred_order_puts_new_chips_last_and_keeps_absent_ones();
    void header_click_still_toggles_when_draggable();
    void columns_go_to_category_with_most_rows();
    void columns_only_added_when_they_save_a_row();
    void card_width_spreads_spare_width_up_to_maximum();
    void cards_fill_width_with_one_shared_width();
    void collapsed_chips_still_count_for_minimum_width();
    void maximum_width_is_where_all_cards_reach_maximum();
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

    // Shows the panel at @p width and lets all nested layouts run.
    void layOut(SensorsPanel &panel, const int width) {
        panel.resize(width, 900);
        panel.show();
        QCoreApplication::sendPostedEvents();
        panel.layout()->activate();
        QCoreApplication::sendPostedEvents();
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
    panel.setReadings(sampleReadings(40.0));
    const QList<SensorValueWidget *> before = cards(panel);
    QCOMPARE(before.size(), 3);

    panel.setReadings(sampleReadings(55.0, SensorUnit::Milliampere));
    QCOMPARE(cards(panel), before);
}

void SensorsPanelTest::resize_moves_cards_without_rebuild() {
    // Two sensors in one category so the column count matters.
    QVector<SensorReading> readings = sampleReadings(40.0);
    readings.append({.chip = QStringLiteral("chip-a"), .category = SensorCategory::Temperatures,
                     .feature = QStringLiteral("temp2"), .featureNumber = 2, .subfeatureNumber = 3,
                     .value = 41.0, .unit = SensorUnit::Celsius, .maxValue = 100.0});
    SensorsPanel panel;
    panel.setReadings(readings);
    // QPointer detects deletion reliably; raw addresses may be reused by new widgets.
    QList<QPointer<SensorValueWidget>> before;
    for (SensorValueWidget *card: cards(panel))
        before.append(card);

    const auto temperatureColumns = [&panel] {
        QSet<int> xs;
        for (SensorValueWidget *card: cards(panel))
            if (card->toolTip().contains(QStringLiteral("temp")))
                xs.insert(card->x());
        return xs.size();
    };
    layOut(panel, panel.minimumSizeHint().width());
    QCOMPARE(temperatureColumns(), 1);
    layOut(panel, 1600);
    QCOMPARE(temperatureColumns(), 2);

    for (const QPointer<SensorValueWidget> &card: before)
        QVERIFY(!card.isNull());
    QCOMPARE(cards(panel).size(), 4);
}

void SensorsPanelTest::expand_state_roundtrip_and_apply() {
    SensorsPanel panel;
    panel.setChipExpandedState({{QStringLiteral("chip-b"), false}});
    panel.setReadings(sampleReadings(40.0));

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
    panel.setReadings(sampleReadings(40.0));
    QCOMPARE(spy.count(), 1);

    panel.setReadings(sampleReadings(45.0));
    QCOMPARE(spy.count(), 1);

    // Same chip set, but chip-b grows to three categories (more than chip-a's two).
    QVector<SensorReading> readings = sampleReadings(45.0);
    readings.append({.chip = QStringLiteral("chip-b"), .category = SensorCategory::Voltages,
                     .feature = QStringLiteral("in0"), .featureNumber = 1, .subfeatureNumber = 2,
                     .value = 12.0, .unit = SensorUnit::Volt, .minValue = 11.0, .maxValue = 13.0});
    readings.append({.chip = QStringLiteral("chip-b"), .category = SensorCategory::Power,
                     .feature = QStringLiteral("power1"), .featureNumber = 2, .subfeatureNumber = 3,
                     .value = 5.0, .unit = SensorUnit::Watt, .maxValue = 60.0});
    const int widthBefore = panel.minimumSizeHint().width();
    panel.setReadings(readings);
    QCOMPARE(spy.count(), 2);
    QVERIFY(panel.minimumSizeHint().width() > widthBefore);
}

void SensorsPanelTest::restored_state_dropped_for_other_chip_set() {
    SensorsPanel panel;
    QSignalSpy spy(&panel, &SensorsPanel::layoutStateReset);
    panel.restoreChipExpandedState({{QStringLiteral("chip-a"), false}}, QStringLiteral("old-chip"));
    panel.setReadings(sampleReadings(40.0));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.chipExpandedState().value(QStringLiteral("chip-a")), true);

    // The check runs once; later polls do not reset again.
    panel.setReadings(sampleReadings(41.0));
    QCOMPARE(spy.count(), 1);
}

void SensorsPanelTest::restored_state_kept_for_same_chip_set() {
    const QVector<SensorReading> readings = sampleReadings(40.0);
    SensorsPanel panel;
    QSignalSpy spy(&panel, &SensorsPanel::layoutStateReset);
    panel.restoreChipExpandedState({{QStringLiteral("chip-a"), false}}, SensorIdentity::chipFingerprint(readings));
    panel.setReadings(readings);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(panel.chipExpandedState().value(QStringLiteral("chip-a")), false);
}

void SensorsPanelTest::fingerprint_and_count_follow_readings() {
    const QVector<SensorReading> readings = sampleReadings(40.0);
    SensorsPanel panel;
    QCOMPARE(panel.readingCount(), 0);
    QVERIFY(panel.chipFingerprint().isEmpty());

    panel.setReadings(readings);
    QCOMPARE(panel.readingCount(), 3);
    QCOMPARE(panel.chipFingerprint(), SensorIdentity::chipFingerprint(readings));
}

void SensorsPanelTest::default_order_is_alphabetical() {
    SensorsPanel panel;
    panel.setReadings(sampleReadings(40.0));
    QCOMPARE(panel.chipOrder(), (QStringList{QStringLiteral("chip-a"), QStringLiteral("chip-b")}));
    QCOMPARE(cardOrder(panel), (QStringList{QStringLiteral("chip-a"), QStringLiteral("chip-b")}));
}

void SensorsPanelTest::move_chip_reorders_cards() {
    SensorsPanel panel;
    panel.setReadings(sampleReadings(40.0));

    panel.moveChip(QStringLiteral("chip-b"), 0);
    QCOMPARE(panel.chipOrder(), (QStringList{QStringLiteral("chip-b"), QStringLiteral("chip-a")}));
    QCOMPARE(cardOrder(panel), (QStringList{QStringLiteral("chip-b"), QStringLiteral("chip-a")}));

    // Target is the insert position before the move: 2 means "after the last card".
    panel.moveChip(QStringLiteral("chip-b"), 2);
    QCOMPARE(cardOrder(panel), (QStringList{QStringLiteral("chip-a"), QStringLiteral("chip-b")}));

    // The order survives value updates.
    panel.moveChip(QStringLiteral("chip-b"), 0);
    panel.setReadings(sampleReadings(45.0));
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
    panel.setReadings(readings);

    QCOMPARE(cardOrder(panel),
             (QStringList{QStringLiteral("chip-b"), QStringLiteral("chip-a"), QStringLiteral("chip-c")}));
    // A chip that is currently missing keeps its saved preference.
    QCOMPARE(panel.chipOrder(), (QStringList{QStringLiteral("chip-b"), QStringLiteral("chip-a"),
                                             QStringLiteral("chip-c"), QStringLiteral("gone")}));
}

void SensorsPanelTest::header_click_still_toggles_when_draggable() {
    SensorsPanel panel;
    panel.setReadings(sampleReadings(40.0));
    QToolButton *header = headerFor(panel, QStringLiteral("chip-a"));
    QCOMPARE(header->cursor().shape(), Qt::OpenHandCursor);
    QVERIFY(header->isChecked());

    QTest::mouseClick(header, Qt::LeftButton);
    QVERIFY(!header->isChecked());
    QCOMPARE(panel.chipExpandedState().value(QStringLiteral("chip-a")), false);
}

void SensorsPanelTest::columns_go_to_category_with_most_rows() {
    const int base = CardGridPlan::categoriesWidth({1, 1, 1, 1});
    const int pitch = AppTheme::kCardMinWidth + AppTheme::kUnifiedHorizontalSpacing;
    const QVector<int> counts{1, 4, 2, 3};

    QCOMPARE(CardGridPlan::columnsForCategories(counts, base), (QVector<int>{1, 1, 1, 1}));
    QCOMPARE(CardGridPlan::columnsForCategories(counts, base + pitch - 1), (QVector<int>{1, 1, 1, 1}));
    QCOMPARE(CardGridPlan::columnsForCategories(counts, base + pitch), (QVector<int>{1, 2, 1, 1}));
    QCOMPARE(CardGridPlan::columnsForCategories(counts, base + 2 * pitch), (QVector<int>{1, 2, 1, 2}));
    // Ties go to the first category with the most rows.
    QCOMPARE(CardGridPlan::columnsForCategories(counts, base + 3 * pitch), (QVector<int>{1, 2, 2, 2}));
    QCOMPARE(CardGridPlan::categoriesWidth({1, 2, 2, 2}), base + 3 * pitch);
}

void SensorsPanelTest::columns_only_added_when_they_save_a_row() {
    // 5 sensors: 3 columns give 2 rows; a 4th column would not save a row.
    QCOMPARE(CardGridPlan::columnsForCategories({5}, 10000), (QVector<int>{3}));
    QCOMPARE(CardGridPlan::columnsForCategories({1}, 10000), (QVector<int>{1}));
    QCOMPARE(CardGridPlan::columnsForCategories({40}, 10000), (QVector<int>{AppTheme::kMaxColumnsPerCategory}));
}

void SensorsPanelTest::card_width_spreads_spare_width_up_to_maximum() {
    const QVector<int> columns{1, 2};
    const int base = CardGridPlan::categoriesWidth(columns);
    QCOMPARE(CardGridPlan::cardWidthFor(columns, base), AppTheme::kCardMinWidth);
    QCOMPARE(CardGridPlan::cardWidthFor(columns, base - 50), AppTheme::kCardMinWidth);
    // 3 cards share 31 px: each gets 10, the remainder stays unused.
    QCOMPARE(CardGridPlan::cardWidthFor(columns, base + 31), AppTheme::kCardMinWidth + 10);
    QCOMPARE(CardGridPlan::cardWidthFor(columns, base + 10000), AppTheme::kCardMaxWidth);
    QCOMPARE(CardGridPlan::cardWidthFor({}, 1000), AppTheme::kCardMinWidth);
}

void SensorsPanelTest::cards_fill_width_with_one_shared_width() {
    SensorsPanel panel;
    panel.setReadings(sampleReadings(40.0));
    const QList<SensorValueWidget *> before = cards(panel);
    const int chrome = panel.minimumSizeHint().width() - CardGridPlan::categoriesWidth({1, 1});

    const int width = 380;
    layOut(panel, width);
    QCOMPARE(cards(panel), before);

    // chip-b alone could use the maximum; chip-a has less spare width and sets the
    // shared width for all cards.
    const int chipAWidth = CardGridPlan::cardWidthFor({1, 1}, width - chrome);
    QCOMPARE(CardGridPlan::cardWidthFor({1}, width - chrome), AppTheme::kCardMaxWidth);
    QVERIFY(chipAWidth > AppTheme::kCardMinWidth && chipAWidth < AppTheme::kCardMaxWidth);
    for (SensorValueWidget *card: cards(panel)) {
        QCOMPARE(card->width(), chipAWidth);
    }
}

void SensorsPanelTest::collapsed_chips_still_count_for_minimum_width() {
    SensorsPanel panel;
    panel.setReadings(sampleReadings(40.0));
    const int expanded = panel.minimumSizeHint().width();
    // chip-a has two categories and sets the minimum; collapsing it must not narrow the window.
    headerFor(panel, QStringLiteral("chip-a"))->toggle();
    QCOMPARE(panel.minimumSizeHint().width(), expanded);
}

void SensorsPanelTest::maximum_width_is_where_all_cards_reach_maximum() {
    CardGridPlan plan;
    QCOMPARE(plan.maximumWidth(), 0);
    // chip-a ends with {3, 1} columns (a 4th column would not save a row), chip-b with {3}.
    plan.setChips({{QStringLiteral("chip-a"), {5, 1}}, {QStringLiteral("chip-b"), {3}}});
    const int width = plan.maximumWidth();
    QCOMPARE(width, CardGridPlan::categoriesWidth({3, 1}, AppTheme::kCardMaxWidth));

    QCOMPARE(plan.columns(QStringLiteral("chip-a"), width), (QVector<int>{3, 1}));
    QCOMPARE(plan.columns(QStringLiteral("chip-a"), 2 * width), (QVector<int>{3, 1}));
    QCOMPARE(plan.cardWidth(width), AppTheme::kCardMaxWidth);
    QVERIFY(plan.cardWidth(width - 1) < AppTheme::kCardMaxWidth);
}

QTEST_MAIN(SensorsPanelTest)
#include "test_sensors_panel.moc"
