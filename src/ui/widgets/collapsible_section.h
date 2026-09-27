// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QFrame>
#include <QPoint>

class QToolButton;
class QVBoxLayout;

/** Framed card with a clickable header that shows or hides its content area. */
class CollapsibleSection : public QFrame {
    Q_OBJECT

public:
    /** Creates the card with @p title in the header and the content initially @p expanded. */
    CollapsibleSection(const QString &title, bool expanded, QWidget *parent = nullptr);

    /** Parent widget for everything placed inside the collapsible area. */
    [[nodiscard]] QWidget *content() const;

    /** Layout of the collapsible area; margins and spacing follow the theme. */
    [[nodiscard]] QVBoxLayout *contentLayout() const;

    /** True when the content area is shown. */
    [[nodiscard]] bool isExpanded() const;

    /** Shows or hides the content area; emits expandedChanged() on change. */
    void setExpanded(bool expanded);

    /** Lets the header start a drag (dragRequested()); a plain click still toggles. */
    void setDraggable(bool draggable);

signals:
    /** Emitted when the content is shown or hidden, by the user or via setExpanded(). */
    void expandedChanged(bool expanded);

    /** The header was dragged past the start distance; the owner runs the drag. */
    void dragRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QToolButton *m_header;
    QWidget *m_content;
    QVBoxLayout *m_contentLayout;
    bool m_draggable = false;
    bool m_swallowRelease = false;
    QPoint m_pressPos;
};
