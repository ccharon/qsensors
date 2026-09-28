// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/widgets/vertical_scroll_area.h"

#include <QEvent>
#include <QScrollBar>

VerticalScrollArea::VerticalScrollArea(QWidget *parent) : QScrollArea(parent) {
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

QSize VerticalScrollArea::minimumSizeHint() const {
    QSize size = QScrollArea::minimumSizeHint();
    if (const QWidget *content = widget()) {
        // Room for the scrollbar is always reserved, so it can appear without cutting off content.
        size.setWidth(content->minimumSizeHint().width() + 2 * frameWidth()
                      + verticalScrollBar()->sizeHint().width());
    }
    return size;
}

QSize VerticalScrollArea::sizeHint() const {
    QSize size = QScrollArea::sizeHint();
    if (const QWidget *content = widget()) {
        const int width = viewport()->width();
        const int height = content->hasHeightForWidth() ? content->heightForWidth(width) : content->sizeHint().height();
        size.setHeight(height + 2 * frameWidth());
    }
    return size;
}

bool VerticalScrollArea::eventFilter(QObject *watched, QEvent *event) {
    // The content is not managed by a layout of ours; forward its size changes.
    if (watched == widget() && event->type() == QEvent::LayoutRequest) {
        updateGeometry();
    }
    return QScrollArea::eventFilter(watched, event);
}
