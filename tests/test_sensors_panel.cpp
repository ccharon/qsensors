// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/panels/card_grid_plan.h"
#include "ui/panels/sensors_panel.h"
#include "ui/widgets/collapsible_section.h"
#include "sensors/sensor_identity.h"
#include "ui/widgets/lcd_display_widget.h"
#include "ui/widgets/sensor_value_widget.h"
#include "ui/theme/app_theme.h"

#include <QApplication>
#include <algorithm>
#include <QDropEvent>
#include <QLayout>
#include <QMimeData>
#include <QPointer>
#include <QSignalSpy>
#include <QToolButton>
#include <QtTest/QtTest>

// Verifies that SensorsPanel keeps widgets across value updates and owns the expand state.
class SensorsPanelTest final : public QObject {
    Q_OBJECT

private slots:
    void value_and_unit_updates_keep_widgets();
    void value_update_repaints_only_changed_lcds();
    void resize_moves_cards_without_rebuild();
    void expand_state_roundtrip_and_apply();
    void structure_signal_on_new_category_not_on_values();
    void restored_state_dropped_only_for_missing_chips();
    void restored_state_kept_when_all_chips_present();
    void empty_first_read_keeps_restored_state();
    void fingerprint_and_count_follow_readings();
    void default_order_is_alphabetical();
    void move_chip_reorders_cards();
    void preferred_order_puts_new_chips_last_and_keeps_absent_ones();
    void header_click_still_toggles_when_draggable();
    void user_toggle_and_drop_are_announced_but_applying_state_is_not();
    void drag_request_carries_the_pointer_position_in_the_section();
    void columns_go_to_category_with_most_rows();
    void columns_only_added_when_they_save_a_row();
    void card_width_spreads_spare_width_up_to_maximum();
    void cards_fill_width_with_one_shared_width();
    void collapsed_chips_still_count_for_minimum_width();
    void maximum_width_is_where_all_cards_reach_maximum();
    void unreadable_sensor_is_a_value_update();
    void chip_with_only_unreadable_sensors_keeps_its_cards();
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

    // Counts paint events per watched widget.
    class PaintCounter final : public QObject {
    public:
        void watch(QWidget *widget) {
            widget->installEventFilter(this);
            m_counts.insert(widget, 0);
        }
        [[nodiscard]] int count(QWidget *widget) const { return m_counts.value(widget); }
        [[nodiscard]] int total() const {
            int sum = 0;
            for (const int c: m_counts)
                sum += c;
            return sum;
        }
        void reset() {
            for (int &c: m_counts)
                c = 0;
        }

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override {
            if (event->type() == QEvent::Paint)
                ++m_counts[static_cast<QWidget *>(watched)];
            return QObject::eventFilter(watched, event);
        }

    private:
        QHash<QWidget *, int> m_counts;
    };

    // Delivers pending update requests, so scheduled paints happen now.
    void flushPaints() {
        for (int i = 0; i < 3; ++i) {
            QCoreApplication::sendPostedEvents();
            QCoreApplication::processEvents();
        }
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

void SensorsPanelTest::value_update_repaints_only_changed_lcds() {
    SensorsPanel panel;
    panel.setReadings(sampleReadings(40.0));
    layOut(panel, 900);
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    flushPaints();

    PaintCounter counter;
    LcdDisplayWidget *tempLcd = nullptr;
    for (SensorValueWidget *card: cards(panel)) {
        auto *lcd = card->findChild<LcdDisplayWidget *>();
        QVERIFY(lcd);
        counter.watch(lcd);
        if (card->toolTip().contains(QStringLiteral("temp1")))
            tempLcd = lcd;
    }
    QVERIFY(tempLcd);

    // The same readings again paint nothing.
    panel.setReadings(sampleReadings(40.0));
    flushPaints();
    QCOMPARE(counter.total(), 0);

    // One changed value repaints only its own LCD.
    panel.setReadings(sampleReadings(55.0));
    flushPaints();
    QCOMPARE(counter.count(tempLcd), 1);
    QCOMPARE(counter.total(), 1);
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

void SensorsPanelTest::restored_state_dropped_only_for_missing_chips() {
    SensorsPanel panel;
    QSignalSpy spy(&panel, &SensorsPanel::expandStateDropped);
    panel.restoreChipExpandedState({{QStringLiteral("chip-a"), false},
                                    {QStringLiteral("gone-collapsed"), false},
                                    {QStringLiteral("gone-expanded"), true}});
    panel.setReadings(sampleReadings(40.0));

    // Present chips keep their state; only the missing ones are dropped.
    const QHash<QString, bool> state = panel.chipExpandedState();
    QCOMPARE(state.value(QStringLiteral("chip-a"), true), false);
    QVERIFY(!headerFor(panel, QStringLiteral("chip-a"))->isChecked());
    QVERIFY(!state.contains(QStringLiteral("gone-collapsed")));
    QVERIFY(!state.contains(QStringLiteral("gone-expanded")));
    // Only a dropped collapsed state is worth a notice; an expanded one is the default anyway.
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toStringList(), QStringList{QStringLiteral("gone-collapsed")});

    // The check runs once; later polls do not drop again.
    panel.setChipExpandedState({{QStringLiteral("gone-collapsed"), false}});
    panel.setReadings(sampleReadings(41.0));
    QCOMPARE(spy.count(), 1);
    QVERIFY(panel.chipExpandedState().contains(QStringLiteral("gone-collapsed")));
}

void SensorsPanelTest::restored_state_kept_when_all_chips_present() {
    SensorsPanel panel;
    QSignalSpy spy(&panel, &SensorsPanel::expandStateDropped);
    panel.restoreChipExpandedState({{QStringLiteral("chip-a"), false}});
    // A new chip (chip-b) alone does not drop anything.
    panel.setReadings(sampleReadings(40.0));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(panel.chipExpandedState().value(QStringLiteral("chip-a")), false);
    QCOMPARE(panel.chipExpandedState().value(QStringLiteral("chip-b")), true);
}

void SensorsPanelTest::empty_first_read_keeps_restored_state() {
    SensorsPanel panel;
    QSignalSpy spy(&panel, &SensorsPanel::expandStateDropped);
    panel.restoreChipExpandedState({{QStringLiteral("chip-a"), false}, {QStringLiteral("chip-b"), false}});
    panel.setReadings({});
    QCOMPARE(spy.count(), 0);
    QCOMPARE(panel.chipExpandedState().size(), 2);

    // The first read with chips decides.
    panel.setReadings(sampleReadings(40.0));
    QCOMPARE(spy.count(), 0);
    QVERIFY(!headerFor(panel, QStringLiteral("chip-a"))->isChecked());
    QVERIFY(!headerFor(panel, QStringLiteral("chip-b"))->isChecked());
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

void SensorsPanelTest::user_toggle_and_drop_are_announced_but_applying_state_is_not() {
    SensorsPanel panel;
    QSignalSpy expandedSpy(&panel, &SensorsPanel::chipExpandedChanged);
    QSignalSpy orderSpy(&panel, &SensorsPanel::chipOrderChanged);

    // Restoring before and after the cards exist, and reordering by code, are not user changes.
    panel.restoreChipExpandedState({{QStringLiteral("chip-a"), false}});
    panel.setChipOrder({QStringLiteral("chip-b"), QStringLiteral("chip-a")});
    panel.setReadings(sampleReadings(40.0));
    panel.setChipExpandedState({{QStringLiteral("chip-a"), true}});
    panel.moveChip(QStringLiteral("chip-a"), 0);
    panel.setReadings(sampleReadings(41.0));
    QCOMPARE(expandedSpy.count(), 0);
    QCOMPARE(orderSpy.count(), 0);

    QTest::mouseClick(headerFor(panel, QStringLiteral("chip-a")), Qt::LeftButton);
    QCOMPARE(expandedSpy.count(), 1);
    QCOMPARE(panel.chipExpandedState().value(QStringLiteral("chip-a")), false);

    // A drop that moves a chip announces the new order; one that keeps it does not.
    const QStringList before = cardOrder(panel);
    const auto drop = [&panel](const QString &chip, const int y) {
        auto *mime = new QMimeData;
        mime->setData(QStringLiteral("application/x-qsensors-chip"), chip.toUtf8());
        // The window routes the events to the widget under the pointer, as for a real drag.
        QWindow *window = panel.windowHandle();
        const QPointF pos(10, y);
        QDragEnterEvent enter(pos.toPoint(), Qt::MoveAction, mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &enter);
        QDropEvent event(pos, Qt::MoveAction, mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &event);
        delete mime;
    };
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));
    drop(before.first(), 0);
    QCOMPARE(orderSpy.count(), 0);
    drop(before.first(), panel.height());
    QCOMPARE(orderSpy.count(), 1);
    QCOMPARE(cardOrder(panel), (QStringList{before.last(), before.first()}));
}

void SensorsPanelTest::drag_request_carries_the_pointer_position_in_the_section() {
    CollapsibleSection section(QStringLiteral("chip"), true);
    section.setDraggable(true);
    section.resize(300, 100);
    section.show();
    QVERIFY(QTest::qWaitForWindowExposed(&section));
    QToolButton *header = section.findChild<QToolButton *>();
    QSignalSpy spy(&section, &CollapsibleSection::dragRequested);

    const QPoint press(20, header->height() / 2);
    const QPoint moved = press + QPoint(QApplication::startDragDistance() + 10, 0);
    QTest::mousePress(header, Qt::LeftButton, Qt::NoModifier, press);
    QTest::mouseMove(header, moved);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toPoint(), header->mapTo(&section, moved));
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

void SensorsPanelTest::unreadable_sensor_is_a_value_update() {
    SensorsPanel panel;
    QSignalSpy spy(&panel, &SensorsPanel::structureChanged);
    panel.setReadings(sampleReadings(40.0));
    QCOMPARE(spy.count(), 1);
    layOut(panel, 900);
    QList<QPointer<SensorValueWidget>> before;
    for (SensorValueWidget *card: cards(panel))
        before.append(card);
    const QString fingerprint = panel.chipFingerprint();
    const QSize minimum = panel.minimumSizeHint();
    const auto tempCard = [&panel] {
        for (SensorValueWidget *card: cards(panel))
            if (card->toolTip().contains(QStringLiteral("temp1")))
                return card;
        return static_cast<SensorValueWidget *>(nullptr);
    };
    SensorValueWidget *temp = tempCard();
    QVERIFY(temp);
    const QRect geometry = temp->geometry();

    QVector<SensorReading> unreadable = sampleReadings(40.0);
    unreadable[0].value.reset();
    panel.setReadings(unreadable);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(panel.chipFingerprint(), fingerprint);
    QCOMPARE(panel.readingCount(), 3);
    QCOMPARE(panel.minimumSizeHint(), minimum);
    QCOMPARE(tempCard(), temp);
    QVERIFY(temp->toolTip().contains(QStringLiteral("cannot be read")));
    layOut(panel, 900);
    QCOMPARE(temp->geometry(), geometry);

    panel.setReadings(sampleReadings(42.0));
    QCOMPARE(spy.count(), 1);
    QVERIFY(!temp->toolTip().contains(QStringLiteral("cannot be read")));
    for (const QPointer<SensorValueWidget> &card: before)
        QVERIFY(!card.isNull());
    QCOMPARE(cards(panel).size(), 3);
}

void SensorsPanelTest::chip_with_only_unreadable_sensors_keeps_its_cards() {
    // chip-b's only sensor cannot be read from the first poll on.
    QVector<SensorReading> readings = sampleReadings(40.0);
    readings[2].value.reset();
    SensorsPanel panel;
    QSignalSpy spy(&panel, &SensorsPanel::structureChanged);
    panel.setReadings(readings);
    QCOMPARE(spy.count(), 1);
    QVERIFY(headerFor(panel, QStringLiteral("chip-b")));
    QCOMPARE(cards(panel).size(), 3);
    QCOMPARE(panel.chipFingerprint(), SensorIdentity::chipFingerprint(sampleReadings(40.0)));

    QList<QPointer<SensorValueWidget>> before;
    for (SensorValueWidget *card: cards(panel))
        before.append(card);
    panel.setReadings(sampleReadings(41.0));
    readings[0].value.reset();
    readings[1].value.reset();
    panel.setReadings(readings);
    QCOMPARE(spy.count(), 1);
    for (const QPointer<SensorValueWidget> &card: before)
        QVERIFY(!card.isNull());
    QCOMPARE(cards(panel).size(), 3);
}

QTEST_MAIN(SensorsPanelTest)
#include "test_sensors_panel.moc"
