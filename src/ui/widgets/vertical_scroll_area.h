// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QScrollArea>

/**
 * Scroll area that only scrolls vertically and resizes its content to its width.
 * Its minimum width is the content's minimum width plus room for the vertical
 * scrollbar, so content is never cut off horizontally; its size hint height is
 * the content height at the current width. Changes of the content propagate to
 * the parent layout like those of any other widget.
 */
class VerticalScrollArea final : public QScrollArea {
    Q_OBJECT

public:
    explicit VerticalScrollArea(QWidget *parent = nullptr);

    [[nodiscard]] QSize minimumSizeHint() const override;
    [[nodiscard]] QSize sizeHint() const override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
};
