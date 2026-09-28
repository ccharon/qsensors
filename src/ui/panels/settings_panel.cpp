// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/panels/settings_panel.h"

#include "config/runtime_config.h"
#include "ui/theme/app_theme.h"

#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QComboBox>
#include <QVBoxLayout>

SettingsPanel::SettingsPanel(QWidget *parent)
    : CollapsibleSection(tr("Settings"), false, parent), m_pollingSpin(nullptr), m_fanMaxRpmSpin(nullptr),
      m_temperatureUnitCombo(nullptr) {
    auto *formLayout = new QFormLayout();
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setHorizontalSpacing(AppTheme::kSectionInset);
    formLayout->setVerticalSpacing(AppTheme::kSectionInset);
    formLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    buildPollingRow(formLayout, content());
    buildFanRpmRow(formLayout, content());
    buildTemperatureUnitRow(formLayout, content());

    contentLayout()->addLayout(formLayout);
    contentLayout()->addStretch(1);
}

void SettingsPanel::setPollingInterval(const int seconds) {
    // Prevent synthetic value restore from feeding back into polling updates.
    const QSignalBlocker blocker(m_pollingSpin);
    m_pollingSpin->setValue(seconds);
}

void SettingsPanel::setFanDefaultMaxRpm(const int rpm) {
    // Prevent synthetic value restore from feeding back into policy updates.
    const QSignalBlocker blocker(m_fanMaxRpmSpin);
    m_fanMaxRpmSpin->setValue(rpm);
}

void SettingsPanel::setTemperatureUnit(const TemperatureUnit unit) {
    const QSignalBlocker blocker(m_temperatureUnitCombo);
    const int index = m_temperatureUnitCombo->findData(temperatureUnitToToken(unit));
    m_temperatureUnitCombo->setCurrentIndex(index >= 0 ? index : 0);
}

int SettingsPanel::minimumRequiredWidth() const {
    return minimumSizeHint().width();
}

namespace {
    // Emits valueChanged only on commit (Enter/focus loss), not per keystroke.
    QSpinBox *createSpinBox(QWidget *parent, const int min, const int max, const int step) {
        auto *spin = new QSpinBox(parent);
        spin->setRange(min, max);
        spin->setSingleStep(step);
        spin->setKeyboardTracking(false);
        spin->setAccelerated(true);
        spin->setStyleSheet(AppTheme::spinBoxStyle());
        return spin;
    }
}

void SettingsPanel::buildPollingRow(QFormLayout *form, QWidget *parent) {
    m_pollingSpin = createSpinBox(parent, RuntimeConfigLimits::kMinPollingIntervalSec,
                                  RuntimeConfigLimits::kMaxPollingIntervalSec, 1);
    connect(m_pollingSpin, &QSpinBox::valueChanged, this, &SettingsPanel::pollingIntervalChanged);
    form->addRow(new QLabel(tr("Polling Interval (s):"), parent), m_pollingSpin);
}

void SettingsPanel::buildFanRpmRow(QFormLayout *form, QWidget *parent) {
    m_fanMaxRpmSpin = createSpinBox(parent, RuntimeConfigLimits::kMinFanDefaultMaxRpm,
                                    RuntimeConfigLimits::kMaxFanDefaultMaxRpm, 100);
    connect(m_fanMaxRpmSpin, &QSpinBox::valueChanged, this, &SettingsPanel::fanDefaultMaxRpmChanged);
    form->addRow(new QLabel(tr("Fan Max RPM:"), parent), m_fanMaxRpmSpin);
}

void SettingsPanel::buildTemperatureUnitRow(QFormLayout *form, QWidget *parent) {
    auto *label = new QLabel(tr("Temperature Unit:"), parent);
    m_temperatureUnitCombo = new QComboBox(parent);
    m_temperatureUnitCombo->addItem(tr("Celsius"), temperatureUnitToToken(TemperatureUnit::Celsius));
    m_temperatureUnitCombo->addItem(tr("Fahrenheit"), temperatureUnitToToken(TemperatureUnit::Fahrenheit));
    m_temperatureUnitCombo->setStyleSheet(AppTheme::comboBoxStyle());
    connect(m_temperatureUnitCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        const QString token = m_temperatureUnitCombo->itemData(index).toString();
        emit temperatureUnitChanged(temperatureUnitFromToken(token).value_or(TemperatureUnit::Celsius));
    });
    form->addRow(label, m_temperatureUnitCombo);
}
