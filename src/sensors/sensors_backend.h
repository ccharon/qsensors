// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QObject>
#include <QString>
#include <QVector>

namespace SensorDomain {
    Q_NAMESPACE

    enum class SensorCategory {
        Voltages = 0,
        Temperatures = 1,
        Fans = 2,
        Currents = 10,
        Power = 11,
        Other = 12,
    };

    Q_ENUM_NS(SensorCategory)
}

using SensorCategory = SensorDomain::SensorCategory;

/** Normalized sensor sample used by the UI layer. */
struct SensorReading {
    QString chip;
    SensorCategory category = SensorCategory::Other;
    QString feature;
    double value = 0.0;
    QString unit;
    bool hasRange = false;
    bool hasMin = false;
    bool hasMax = false;
    double minValue = 0.0;
    double maxValue = 0.0;
};

/** Thin wrapper around libsensors discovery/readout. */
class SensorsBackend {
public:
    SensorsBackend();

    ~SensorsBackend();

    SensorsBackend(const SensorsBackend &) = delete;

    SensorsBackend &operator=(const SensorsBackend &) = delete;

    /** True when libsensors init succeeded and reads are valid. */
    [[nodiscard]] bool isInitialized() const;

    /** Human-readable backend init error. */
    [[nodiscard]] QString lastError() const;

    /** Snapshot of all supported sensor input values. */
    [[nodiscard]] QVector<SensorReading> readAll() const;

private:
    bool m_initialized;
    QString m_lastError;
};
