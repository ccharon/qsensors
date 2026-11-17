// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/widgets/status_line.h"

#include <QTimer>

StatusLine::StatusLine(QWidget *parent) : QLabel(parent), m_noticeTimer(new QTimer(this)) {
    // Shows text from libsensors (e.g. init errors); never interpreted as rich text.
    setTextFormat(Qt::PlainText);
    m_noticeTimer->setSingleShot(true);
    connect(m_noticeTimer, &QTimer::timeout, this, [this] {
        if (m_queue.isEmpty()) {
            display(m_permanentText, false);
        } else {
            const Notice next = m_queue.dequeue();
            display(next.text, true);
            m_noticeTimer->start(next.timeoutMs);
        }
    });
}

void StatusLine::setPermanentText(const QString &text) {
    m_permanentText = text;
    if (!isShowingNotice()) {
        display(text, false);
    }
}

void StatusLine::showNotice(const QString &text, const int timeoutMs) {
    if (isShowingNotice()) {
        m_queue.enqueue({text, timeoutMs});
        return;
    }
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
