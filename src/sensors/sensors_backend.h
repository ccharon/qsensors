// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "runtime_config.h"

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

enum class SensorUnit {
    Celsius,
    Volt,
    Rpm,
    Ampere,
    Watt,
    Unknown
};

[[nodiscard]] inline QString sensorUnitSymbol(const SensorUnit unit) {
    switch (unit) {
        case SensorUnit::Celsius: return QStringLiteral("°C");
        case SensorUnit::Volt: return QStringLiteral("V");
        case SensorUnit::Rpm: return QStringLiteral("RPM");
        case SensorUnit::Ampere: return QStringLiteral("A");
        case SensorUnit::Watt: return QStringLiteral("W");
        case SensorUnit::Unknown: return QString();
    }
    return QString();
}

/** Normalized sensor sample used by the UI layer. */
struct SensorReading {
    QString chip;
    SensorCategory category = SensorCategory::Other;
    QString feature;
    double value = 0.0;
    SensorUnit unit = SensorUnit::Unknown;
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

    /** Applies runtime configuration relevant to backend range defaults. */
    void applyConfig(const RuntimeConfig &config);

private:
    bool m_initialized;
    QString m_lastError;
    int m_defaultFanMaxRpm = 5000;
};
