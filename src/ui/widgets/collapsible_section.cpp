// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "collapsible_section.h"
#include "theme/app_theme.h"

#include <QToolButton>
#include <QVBoxLayout>

CollapsibleSection::CollapsibleSection(const QString &title, const bool expanded, QWidget *parent)
    : QFrame(parent), m_header(new QToolButton(this)), m_content(new QWidget(this)),
      m_contentLayout(new QVBoxLayout(m_content)) {
    setObjectName(QStringLiteral("sectionCard"));
    setStyleSheet(AppTheme::sectionCardStyle());

    m_header->setText(title);
    m_header->setCheckable(true);
    m_header->setChecked(expanded);
    m_header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_header->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    m_header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_header->setStyleSheet(AppTheme::sectionHeaderStyle());

    m_contentLayout->setContentsMargins(AppTheme::kSectionInset, AppTheme::kSectionInset,
                                        AppTheme::kSectionInset, AppTheme::kSectionInset);
    m_contentLayout->setSpacing(AppTheme::kSectionInset);
    m_content->setVisible(expanded);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_header);
    layout->addWidget(m_content);

    connect(m_header, &QToolButton::toggled, this, [this](const bool isOpen) {
        m_header->setArrowType(isOpen ? Qt::DownArrow : Qt::RightArrow);
        m_content->setVisible(isOpen);
        emit expandedChanged(isOpen);
    });
}

QWidget *CollapsibleSection::content() const {
    return m_content;
}

QVBoxLayout *CollapsibleSection::contentLayout() const {
    return m_contentLayout;
}

bool CollapsibleSection::isExpanded() const {
    return m_header->isChecked();
}

void CollapsibleSection::setExpanded(const bool expanded) {
    m_header->setChecked(expanded);
}
