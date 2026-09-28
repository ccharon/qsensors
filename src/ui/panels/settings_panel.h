// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "ui/widgets/collapsible_section.h"
#include "config/runtime_config.h"

class QFormLayout;
class QSpinBox;
class QComboBox;

/** Collapsed-by-default section with polling, fan fallback and temperature unit controls. */
class SettingsPanel final : public CollapsibleSection {
    Q_OBJECT

public:
    /** Creates the section collapsed; values are set by the owner via the setters. */
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
    /** User committed a new polling interval in seconds. */
    void pollingIntervalChanged(int seconds);
    /** User committed a new fallback maximum for fans without firmware limits. */
    void fanDefaultMaxRpmChanged(int rpm);
    /** User selected a different temperature unit. */
    void temperatureUnitChanged(TemperatureUnit unit);

private:
    void buildPollingRow(QFormLayout *form, QWidget *parent);
    void buildFanRpmRow(QFormLayout *form, QWidget *parent);
    void buildTemperatureUnitRow(QFormLayout *form, QWidget *parent);

    QSpinBox *m_pollingSpin;
    QSpinBox *m_fanMaxRpmSpin;
    QComboBox *m_temperatureUnitCombo;
};
