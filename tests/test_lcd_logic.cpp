// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/reading_pipeline.h"
#include "ui/widgets/lcd_display_widget.h"
#include "ui/widgets/sensor_value_widget.h"

#include <QtTest/QtTest>

// Verifies LCD bar graph geometry, the sensor card tooltip and when the LCD repaints.
class LcdLogicTest final : public QObject {
    Q_OBJECT

private slots:
    void barGraph_lights_proportional_segments();
    void detailsToolTip_lists_chip_and_limits_in_display_units();
    void detailsToolTip_omits_placeholder_firmware_limits();
    void missing_value_lights_no_bar_and_tooltip_says_so();
    void lcd_repaints_only_when_the_drawn_state_changes();
    void lcd_repaints_on_resize_and_palette_change();
    void tooltip_follows_the_fields_it_shows_not_every_poll();
};

void LcdLogicTest::barGraph_lights_proportional_segments() {
    SensorReading quarter{.value = 25.0, .unit = SensorUnit::Celsius, .minValue = 0.0, .maxValue = 100.0};
    QCOMPARE(LcdDisplayWidget::litBarSegments(quarter, 20), 5);
    QCOMPARE(LcdDisplayWidget::litBarSegments(quarter, 0), 0);

    SensorReading noRange{.value = 25.0, .unit = SensorUnit::Celsius};
    QCOMPARE(LcdDisplayWidget::litBarSegments(noRange, 20), 0);

    const auto segments = LcdDisplayWidget::barGraphSegments(QRectF(4, 30, 142, 4));
    QVERIFY(segments.size() >= 20);
    for (qsizetype i = 1; i < segments.size(); ++i) {
        QVERIFY(segments.at(i).boundingRect().left() > segments.at(i - 1).boundingRect().left());
    }
    QVERIFY(segments.last().boundingRect().right() <= 4 + 142 + 1e-6);
}

void LcdLogicTest::detailsToolTip_lists_chip_and_limits_in_display_units() {
    SensorReading battery{.chip = QStringLiteral("macsmc_battery-isa-0000"), .feature = QStringLiteral("in0"),
                          .value = 11.9, .unit = SensorUnit::Volt, .firmwareMin = 9.888, .firmwareMax = 13.325};
    const QString tip = SensorValueWidget::detailsToolTip(battery);
    QVERIFY(tip.contains(QStringLiteral("<b>in0</b>")));
    QVERIFY(tip.contains(QStringLiteral("macsmc_battery-isa-0000")));
    QVERIFY(tip.contains(QStringLiteral("9.89 V")));
    QVERIFY(tip.contains(QStringLiteral("13.32 V")) || tip.contains(QStringLiteral("13.33 V")));
    QVERIFY(!tip.contains(QStringLiteral("No limits")));

    // Names are escaped: sensors.conf labels are free text.
    SensorReading odd{.chip = QStringLiteral("c"), .feature = QStringLiteral("<CPU & SoC>"),
                      .value = 1.0, .unit = SensorUnit::Watt};
    const QString oddTip = SensorValueWidget::detailsToolTip(odd);
    QVERIFY(oddTip.contains(QStringLiteral("&lt;CPU &amp; SoC&gt;")));
    QVERIFY(oddTip.contains(QStringLiteral("No limits available")));

    // A default bar graph range is not a limit and stays out of the tooltip.
    SensorReading guessed{.chip = QStringLiteral("c"), .feature = QStringLiteral("temp1"), .value = 40.0,
                          .unit = SensorUnit::Celsius, .minValue = 0.0, .maxValue = 100.0};
    const QString guessedTip = SensorValueWidget::detailsToolTip(guessed);
    QVERIFY(guessedTip.contains(QStringLiteral("No limits available")));
    QVERIFY(!guessedTip.contains(QStringLiteral("100.0")));
}

void LcdLogicTest::detailsToolTip_omits_placeholder_firmware_limits() {
    const auto tipFor = [](const SensorCategory category, const double value, const SensorUnit unit,
                           const std::optional<double> min, const std::optional<double> max,
                           const TemperatureUnit temperatureUnit = TemperatureUnit::Celsius) {
        SensorReading raw;
        raw.chip = QStringLiteral("c");
        raw.category = category;
        raw.feature = QStringLiteral("f");
        raw.value = value;
        raw.unit = unit;
        raw.firmwareMin = min;
        raw.firmwareMax = max;
        RuntimeConfig config;
        config.temperatureUnit = temperatureUnit;
        ReadingPipeline pipeline;
        return SensorValueWidget::detailsToolTip(pipeline.process({raw}, config)[0]);
    };
    constexpr auto kTemp = SensorCategory::Temperatures;
    constexpr auto kC = SensorUnit::Celsius;

    // NVMe secondary sensor: 0 K and 65535 K are placeholders.
    for (const TemperatureUnit unit: {TemperatureUnit::Celsius, TemperatureUnit::Fahrenheit}) {
        const QString tip = tipFor(kTemp, 40.0, kC, -273.15, 65261.85, unit);
        QVERIFY(tip.contains(QStringLiteral("No limits available")));
        QVERIFY(!tip.contains(QStringLiteral("Min:")));
        QVERIFY(!tip.contains(QStringLiteral("Max:")));
    }

    // Unconfigured voltage input and a current with a zero maximum.
    QVERIFY(tipFor(SensorCategory::Voltages, 1.2, SensorUnit::Volt, 0.0, 0.0)
                .contains(QStringLiteral("No limits available")));
    QVERIFY(tipFor(SensorCategory::Currents, 12.0, SensorUnit::Ampere, std::nullopt, 0.0)
                .contains(QStringLiteral("No limits available")));

    // A freezer sensor keeps its limits, in the chosen unit.
    const QString celsiusTip = tipFor(kTemp, -18.0, kC, -40.0, -10.0);
    QVERIFY(celsiusTip.contains(QStringLiteral("Min: -40.0 °C")));
    QVERIFY(celsiusTip.contains(QStringLiteral("Max: -10.0 °C")));
    const QString fahrenheitTip = tipFor(kTemp, -18.0, kC, -40.0, -10.0, TemperatureUnit::Fahrenheit);
    QVERIFY(fahrenheitTip.contains(QStringLiteral("Min: -40.0 °F")));
    QVERIFY(fahrenheitTip.contains(QStringLiteral("Max: 14.0 °F")));
}

void LcdLogicTest::missing_value_lights_no_bar_and_tooltip_says_so() {
    SensorReading fan;
    fan.chip = QStringLiteral("c");
    fan.feature = QStringLiteral("fan1");
    fan.unit = SensorUnit::Rpm;
    fan.minValue = fan.firmwareMin = 500.0;
    fan.maxValue = 5000.0;
    QCOMPARE(LcdDisplayWidget::litBarSegments(fan, 20), 0);

    const QString tip = SensorValueWidget::detailsToolTip(fan);
    QVERIFY(tip.contains(QStringLiteral("The sensor cannot be read at the moment")));
    // Limits are still reported without a value.
    QVERIFY(tip.contains(QStringLiteral("Min: 500 RPM")));

    fan.value = 1200.0;
    QVERIFY(!SensorValueWidget::detailsToolTip(fan).contains(QStringLiteral("cannot be read")));
}

namespace {
    /** Counts the paint events a widget receives. */
    class PaintCounter final : public QObject {
    public:
        explicit PaintCounter(QWidget *widget) { widget->installEventFilter(this); }
        int count = 0;

    protected:
        bool eventFilter(QObject *, QEvent *event) override {
            if (event->type() == QEvent::Paint)
                ++count;
            return false;
        }
    };

    SensorReading volts(const double value) {
        return {.chip = QStringLiteral("chip"), .feature = QStringLiteral("in0"), .value = value,
                .unit = SensorUnit::Volt, .minValue = 0.0, .maxValue = 20.0, .firmwareMin = 0.0, .firmwareMax = 12.0};
    }

    // Lets pending paints run, then forgets them.
    void settle(PaintCounter &counter) {
        QTest::qWait(30);
        counter.count = 0;
    }

    // True when a paint follows setReading() within a generous wait.
    bool repaintsAfter(LcdDisplayWidget &lcd, PaintCounter &counter, const SensorReading &reading) {
        counter.count = 0;
        lcd.setReading(reading);
        QTest::qWait(30);
        return counter.count > 0;
    }
}

void LcdLogicTest::lcd_repaints_only_when_the_drawn_state_changes() {
    LcdDisplayWidget lcd(volts(5.0));
    lcd.resize(160, 40);
    PaintCounter counter(&lcd);
    lcd.show();
    QVERIFY(QTest::qWaitForWindowExposed(&lcd));
    settle(counter);

    // The same reading again, and a change below the displayed digits and bar resolution: nothing to draw.
    QVERIFY(!repaintsAfter(lcd, counter, volts(5.0)));
    QVERIFY(!repaintsAfter(lcd, counter, volts(5.001)));

    // Changed digits.
    QVERIFY(repaintsAfter(lcd, counter, volts(5.5)));
    QVERIFY(!repaintsAfter(lcd, counter, volts(5.5)));

    // Changed unit with the same digits.
    SensorReading amps = volts(5.5);
    amps.unit = SensorUnit::Ampere;
    QVERIFY(repaintsAfter(lcd, counter, amps));

    // Same digits, but the value crosses the firmware maximum: only the alert state differs.
    QVERIFY(repaintsAfter(lcd, counter, volts(12.0)));
    QVERIFY(repaintsAfter(lcd, counter, volts(12.001)));
    QVERIFY(repaintsAfter(lcd, counter, volts(12.0)));

    // Same digits, but another number of lit bar segments: no repaint request is lost.
    SensorReading wide = volts(12.0);
    wide.maxValue = 24.0;
    QVERIFY(repaintsAfter(lcd, counter, wide));

    // A value that cannot be read differs from one that can.
    SensorReading unreadable = wide;
    unreadable.value.reset();
    QVERIFY(repaintsAfter(lcd, counter, unreadable));
    QVERIFY(!repaintsAfter(lcd, counter, unreadable));
}

void LcdLogicTest::lcd_repaints_on_resize_and_palette_change() {
    LcdDisplayWidget lcd(volts(5.0));
    lcd.resize(160, 40);
    PaintCounter counter(&lcd);
    lcd.show();
    QVERIFY(QTest::qWaitForWindowExposed(&lcd));
    settle(counter);

    lcd.resize(260, 40);
    QTest::qWait(30);
    QVERIFY(counter.count > 0);
    // After the resize the unchanged reading is still not repainted.
    QVERIFY(!repaintsAfter(lcd, counter, volts(5.0)));

    QPalette dark = lcd.palette();
    dark.setColor(QPalette::Base, Qt::black);
    counter.count = 0;
    lcd.setPalette(dark);
    QTest::qWait(30);
    QVERIFY(counter.count > 0);
}

void LcdLogicTest::tooltip_follows_the_fields_it_shows_not_every_poll() {
    SensorReading reading = volts(5.0);
    SensorValueWidget card(reading);
    QCOMPARE(card.toolTip(), SensorValueWidget::detailsToolTip(reading));

    reading.value = 6.0;
    card.setReading(reading);
    QCOMPARE(card.toolTip(), SensorValueWidget::detailsToolTip(reading));

    reading.firmwareMax = 13.0;
    card.setReading(reading);
    QVERIFY(card.toolTip().contains(QStringLiteral("13")));
    QCOMPARE(card.toolTip(), SensorValueWidget::detailsToolTip(reading));

    reading.value.reset();
    card.setReading(reading);
    QVERIFY(card.toolTip().contains(QStringLiteral("cannot be read")));

    reading.scale = SensorScale{.prefix = SiPrefix::Milli, .decimals = 1};
    card.setReading(reading);
    QCOMPARE(card.toolTip(), SensorValueWidget::detailsToolTip(reading));

    reading.unit = SensorUnit::Ampere;
    card.setReading(reading);
    QCOMPARE(card.toolTip(), SensorValueWidget::detailsToolTip(reading));

    reading.feature = QStringLiteral("renamed");
    card.setReading(reading);
    QVERIFY(card.toolTip().contains(QStringLiteral("renamed")));
}

QTEST_MAIN(LcdLogicTest)
#include "test_lcd_logic.moc"
