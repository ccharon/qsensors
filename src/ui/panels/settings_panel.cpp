// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "settings_panel.h"

#include "theme/app_theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

SettingsPanel::SettingsPanel(QWidget *parent) : QFrame(parent), m_spin(nullptr) {
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
    auto *contentLayout = new QHBoxLayout(content);
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

    auto *label = new QLabel(tr("Polling Interval (s):"), content);
    m_spin = new QSpinBox(content);
    m_spin->setRange(1, 10);
    m_spin->setKeyboardTracking(false);
    m_spin->setAccelerated(true);
    m_spin->setStyleSheet(AppTheme::spinBoxStyle());

    connect(m_spin, &QSpinBox::valueChanged, this, &SettingsPanel::pollingIntervalChanged);

    contentLayout->addWidget(label);
    contentLayout->addWidget(m_spin, 0, Qt::AlignLeft);
    contentLayout->addStretch(1);

    settingsLayout->addWidget(content);
}

void SettingsPanel::setPollingInterval(const int seconds) {
    // Prevent synthetic value restore from feeding back into polling updates.
    const QSignalBlocker blocker(m_spin);
    m_spin->setValue(seconds);
}
