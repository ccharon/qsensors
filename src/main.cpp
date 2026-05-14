// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "main_window.h"

#include <QApplication>
#include <QIcon>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qsensors"));
    QCoreApplication::setApplicationName(QStringLiteral("qsensors"));

    // Always install English catalog first as baseline fallback.
    QTranslator translatorEn;
    if (translatorEn.load(QStringLiteral(":/i18n/qsensors_en.qm"))) {
        app.installTranslator(&translatorEn);
    }

    // Overlay locale-specific catalog if available.
    QTranslator translatorLocale;
    if (translatorLocale.load(QLocale(), QStringLiteral("qsensors"), QStringLiteral("_"), QStringLiteral(":/i18n"))) {
        app.installTranslator(&translatorLocale);
    }

    const QIcon appIcon(QStringLiteral(":/icons/xsensors.png"));
    app.setWindowIcon(appIcon);
    MainWindow window;
    window.setWindowIcon(appIcon);
    window.show();
    return QApplication::exec();
}
