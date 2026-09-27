// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "lcd_display_widget.h"
#include "sensor_value_widget.h"

#include <QtTest/QtTest>

// Verifies LCD bar graph geometry and the sensor card tooltip independent of painting.
class LcdLogicTest final : public QObject {
    Q_OBJECT

private slots:
    void barGraph_lights_proportional_segments();
    void detailsToolTip_lists_chip_and_limits_in_display_units();
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
                          .value = 11.9, .unit = SensorUnit::Volt, .minValue = 9.888, .maxValue = 13.325};
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
}

QTEST_APPLESS_MAIN(LcdLogicTest)
#include "test_lcd_logic.moc"
