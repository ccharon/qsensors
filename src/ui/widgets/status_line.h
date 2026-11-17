// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QLabel>
#include <QQueue>

class QTimer;

/** Status bar text that shows a permanent status or, for a while, a bold notice on top of it. */
class StatusLine final : public QLabel {
    Q_OBJECT

public:
    /** Creates an empty status line. */
    explicit StatusLine(QWidget *parent = nullptr);

    /** Sets the permanent status; while a notice is shown it appears after the notice expires. */
    void setPermanentText(const QString &text);

    /** Shows @p text in bold for @p timeoutMs, then returns to the permanent status. A notice
     * arriving while another is shown waits and is shown after it, in order. */
    void showNotice(const QString &text, int timeoutMs);

    /** True while a notice hides the permanent status. */
    [[nodiscard]] bool isShowingNotice() const;

private:
    /** Shows @p text with the notice or normal font weight. */
    void display(const QString &text, bool notice);

    struct Notice {
        QString text;
        int timeoutMs;
    };

    QTimer *m_noticeTimer;
    QQueue<Notice> m_queue;
    QString m_permanentText;
};
