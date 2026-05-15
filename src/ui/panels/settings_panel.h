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
    /** Applies persisted fan fallback max rpm without re-emitting change signals. */
    void setFanDefaultMaxRpm(int rpm);

signals:
    void pollingIntervalChanged(int seconds);
    void fanDefaultMaxRpmChanged(int rpm);

private:
    QSpinBox *m_pollingSpin;
    QSpinBox *m_fanMaxRpmSpin;
};
