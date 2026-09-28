// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/widgets/collapsible_section.h"
#include "ui/theme/app_theme.h"

#include <QApplication>
#include <QMouseEvent>
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

void CollapsibleSection::setDraggable(const bool draggable) {
    if (draggable == m_draggable)
        return;
    m_draggable = draggable;
    if (draggable) {
        m_header->installEventFilter(this);
        m_header->setCursor(Qt::OpenHandCursor);
        m_header->setToolTip(tr("Click to collapse or expand, drag to reorder"));
    } else {
        m_header->removeEventFilter(this);
        m_header->unsetCursor();
        m_header->setToolTip(QString());
    }
}

bool CollapsibleSection::eventFilter(QObject *watched, QEvent *event) {
    if (watched != m_header)
        return QFrame::eventFilter(watched, event);

    if (event->type() == QEvent::MouseButtonPress) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        m_swallowRelease = false;
        if (mouse->button() == Qt::LeftButton)
            m_pressPos = mouse->position().toPoint();
    } else if (event->type() == QEvent::MouseButtonRelease && m_swallowRelease) {
        // Some platforms deliver the release after a drag; it must not toggle the section.
        m_swallowRelease = false;
        return true;
    } else if (event->type() == QEvent::MouseMove) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if ((mouse->buttons() & Qt::LeftButton)
            && (mouse->position().toPoint() - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            // The drag's event loop swallows the release, so the header must not stay pressed.
            m_header->setDown(false);
            m_swallowRelease = true;
            emit dragRequested();
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}
