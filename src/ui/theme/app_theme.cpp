// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "app_theme.h"

namespace AppTheme {
    namespace {
        QString cardStyle(const char *selector) {
            return QStringLiteral(
                "%1 {"
                "  border: 1px solid palette(mid);"
                "  background: palette(window);"
                "  color: palette(window-text);"
                "}"
            ).arg(QLatin1StringView(selector));
        }
    }

    QString chipCardStyle() {
        return cardStyle("#chipCard");
    }

    QString sectionHeaderStyle() {
        return QStringLiteral(
            "QToolButton {"
            "  border: none;"
            "  text-align: left;"
            "  font-weight: bold;"
            "  padding: 6px 8px;"
            "  background: palette(button);"
            "  color: palette(button-text);"
            "}"
        );
    }

    QString settingsCardStyle() {
        return cardStyle("#settingsCard");
    }

    QString sensorGroupStyle(const QString &borderColor) {
        return QStringLiteral(
            "QGroupBox {"
            "  border: 1px solid %1;"
            "  margin-top: 8px;"
            "  padding-top: 0px;"
            "  padding-bottom: 0px;"
            "  background: palette(window);"
            "  color: palette(window-text);"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  left: 4px;"
            "  top: -2px;"
            "  padding: 0 2px;"
            "  font-size: 11px;"
            "  background: palette(window);"
            "  color: palette(window-text);"
            "}"
        ).arg(borderColor);
    }

    QString spinBoxStyle() {
        return QStringLiteral(
            "QAbstractSpinBox {"
            "  min-width: 72px;"
            "}"
        );
    }

    QString comboBoxStyle() {
        return QStringLiteral(
            "QComboBox {"
            "  min-width: 120px;"
            "}"
        );
    }

    QString progressBarStyle(const bool hasRange) {
        return QStringLiteral(
            "QProgressBar {"
            "  border: none;"
            "  background: palette(midlight);"
            "}"
            "QProgressBar::chunk {"
            "  background: %1;"
            "}"
        ).arg(hasRange ? QStringLiteral("palette(highlight)") : QStringLiteral("transparent"));
    }
}
