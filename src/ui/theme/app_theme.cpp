// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/theme/app_theme.h"

#include <QWidget>

namespace AppTheme {
    QString sectionCardStyle() {
        return QStringLiteral(
            "#sectionCard {"
            "  border: 1px solid palette(mid);"
            "  background: palette(window);"
            "  color: palette(window-text);"
            "}"
        );
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

    void refreshStyleSheets(QWidget *root) {
        if (!root)
            return;
        QList<QWidget *> widgets = root->findChildren<QWidget *>();
        widgets.prepend(root);
        for (QWidget *widget: widgets) {
            const QString sheet = widget->styleSheet();
            if (sheet.isEmpty())
                continue;
            // Clearing first forces a full unpolish/polish, dropping cached palette colors.
            widget->setStyleSheet(QString());
            widget->setStyleSheet(sheet);
        }
    }
}
