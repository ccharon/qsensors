// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QFrame>

class QSpinBox;

/** Collapsible settings section rendered at the bottom of the main structure. */
class SettingsPanel final : public QFrame {
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    /** Applies persisted polling interval without re-emitting change signals. */
    void setPollingInterval(int seconds);

signals:
    void pollingIntervalChanged(int seconds);

private:
    QSpinBox *m_spin;
};
