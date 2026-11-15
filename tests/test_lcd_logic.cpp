// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/reading_pipeline.h"
#include "ui/widgets/lcd_display_widget.h"
#include "ui/widgets/sensor_value_widget.h"

#include <QtTest/QtTest>

// Verifies LCD bar graph geometry and the sensor card tooltip independent of painting.
class LcdLogicTest final : public QObject {
    Q_OBJECT

private slots:
    void barGraph_lights_proportional_segments();
    void detailsToolTip_lists_chip_and_limits_in_display_units();
    void detailsToolTip_omits_placeholder_firmware_limits();
    void missing_value_lights_no_bar_and_tooltip_says_so();
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

QTEST_APPLESS_MAIN(LcdLogicTest)
#include "test_lcd_logic.moc"
