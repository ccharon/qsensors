// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "status_line.h"

#include <QTimer>

StatusLine::StatusLine(QWidget *parent) : QLabel(parent), m_noticeTimer(new QTimer(this)) {
    m_noticeTimer->setSingleShot(true);
    connect(m_noticeTimer, &QTimer::timeout, this, [this] { display(m_permanentText, false); });
}

void StatusLine::setPermanentText(const QString &text) {
    m_permanentText = text;
    if (!isShowingNotice()) {
        display(text, false);
    }
}

void StatusLine::showNotice(const QString &text, const int timeoutMs) {
    display(text, true);
    m_noticeTimer->start(timeoutMs);
}

bool StatusLine::isShowingNotice() const {
    return m_noticeTimer->isActive();
}

void StatusLine::display(const QString &text, const bool notice) {
    QFont f = font();
    f.setBold(notice);
    setFont(f);
    setText(text);
}
