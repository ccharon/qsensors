// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "runtime_config.h"

#include <QFrame>

class QSpinBox;
class QComboBox;

/** Collapsible settings section rendered at the bottom of the main structure. */
class SettingsPanel final : public QFrame {
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    /** Applies persisted polling interval without re-emitting change signals. */
    void setPollingInterval(int seconds);
    /** Applies persisted fan fallback max rpm without re-emitting change signals. */
    void setFanDefaultMaxRpm(int rpm);
    /** Applies persisted temperature unit without re-emitting change signals. */
    void setTemperatureUnit(TemperatureUnit unit);

    /** Minimum width required so settings content is fully visible. */
    [[nodiscard]] int minimumRequiredWidth() const;

signals:
    void pollingIntervalChanged(int seconds);
    void fanDefaultMaxRpmChanged(int rpm);
    void temperatureUnitChanged(TemperatureUnit unit);

private:
    QSpinBox *m_pollingSpin;
    QSpinBox *m_fanMaxRpmSpin;
    QComboBox *m_temperatureUnitCombo;
};
