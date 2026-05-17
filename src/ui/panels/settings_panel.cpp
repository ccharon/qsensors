// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "settings_panel.h"

#include "theme/app_theme.h"

#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

SettingsPanel::SettingsPanel(QWidget *parent) : QFrame(parent), m_pollingSpin(nullptr), m_fanMaxRpmSpin(nullptr) {
    setObjectName(QStringLiteral("settingsCard"));
    setStyleSheet(AppTheme::settingsCardStyle());

    auto *settingsLayout = new QVBoxLayout(this);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(0);

    auto *header = new QToolButton(this);
    header->setText(tr("Settings"));
    header->setCheckable(true);
    header->setChecked(false);
    header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    header->setArrowType(Qt::RightArrow);
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    header->setStyleSheet(AppTheme::sectionHeaderStyle());

    auto *content = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(
        AppTheme::kSectionInset, AppTheme::kSectionInset,
        AppTheme::kSectionInset, AppTheme::kSectionInset
    );
    contentLayout->setSpacing(AppTheme::kSectionInset);
    content->setVisible(false);

    // Header controls local collapse state; intentionally not persisted.
    connect(header, &QToolButton::toggled, this, [header, content](bool expanded) {
        header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        content->setVisible(expanded);
    });

    settingsLayout->addWidget(header);

    auto *formLayout = new QFormLayout();
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setHorizontalSpacing(AppTheme::kSectionInset);
    formLayout->setVerticalSpacing(AppTheme::kSectionInset);
    formLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    auto *pollingLabel = new QLabel(tr("Polling Interval (s):"), content);
    m_pollingSpin = new QSpinBox(content);
    m_pollingSpin->setRange(1, 10);
    m_pollingSpin->setKeyboardTracking(false);
    m_pollingSpin->setAccelerated(true);
    m_pollingSpin->setStyleSheet(AppTheme::spinBoxStyle());
    connect(m_pollingSpin, &QSpinBox::valueChanged, this, &SettingsPanel::pollingIntervalChanged);

    auto *fanRpmLabel = new QLabel(tr("Fan Max RPM:"), content);
    m_fanMaxRpmSpin = new QSpinBox(content);
    m_fanMaxRpmSpin->setRange(1000, 50000);
    m_fanMaxRpmSpin->setSingleStep(100);
    m_fanMaxRpmSpin->setKeyboardTracking(false);
    m_fanMaxRpmSpin->setAccelerated(true);
    m_fanMaxRpmSpin->setStyleSheet(AppTheme::spinBoxStyle());
    connect(m_fanMaxRpmSpin, &QSpinBox::valueChanged, this, &SettingsPanel::fanDefaultMaxRpmChanged);

    formLayout->addRow(pollingLabel, m_pollingSpin);
    formLayout->addRow(fanRpmLabel, m_fanMaxRpmSpin);

    contentLayout->addLayout(formLayout);
    contentLayout->addStretch(1);

    settingsLayout->addWidget(content);
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

int SettingsPanel::minimumRequiredWidth() const {
    return minimumSizeHint().width();
}
