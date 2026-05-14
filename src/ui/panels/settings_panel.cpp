// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "settings_panel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

SettingsPanel::SettingsPanel(QWidget *parent) : QFrame(parent), m_spin(nullptr) {
    setObjectName(QStringLiteral("settingsCard"));
    setStyleSheet(QStringLiteral(
        "#settingsCard {"
        "  border: 1px solid palette(mid);"
        "  background: palette(window);"
        "  color: palette(window-text);"
        "}"
    ));

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
    header->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  border: none;"
        "  text-align: left;"
        "  font-weight: bold;"
        "  padding: 6px 8px;"
        "  background: palette(button);"
        "  color: palette(button-text);"
        "}"
    ));

    auto *content = new QWidget(this);
    auto *contentLayout = new QHBoxLayout(content);
    contentLayout->setContentsMargins(8, 8, 8, 8);
    contentLayout->setSpacing(8);
    content->setVisible(false);

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
    m_spin->setStyleSheet(QStringLiteral(
        "QAbstractSpinBox {"
        "  min-width: 72px;"
        "}"
    ));

    connect(m_spin, &QSpinBox::valueChanged, this, &SettingsPanel::pollingIntervalChanged);

    contentLayout->addWidget(label);
    contentLayout->addWidget(m_spin, 0, Qt::AlignLeft);
    contentLayout->addStretch(1);

    settingsLayout->addWidget(content);
}

void SettingsPanel::setPollingInterval(const int seconds) {
    const QSignalBlocker blocker(m_spin);
    m_spin->setValue(seconds);
}
