// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "sensors/libsensors_source.h"
#include "sensors/sensor_monitor.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QIcon>
#include <QLocale>
#include <QTranslator>

#include <cstdio>
#include <cstdlib>
#include <memory>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qsensors"));
    QCoreApplication::setApplicationName(QStringLiteral("qsensors"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QSENSORS_VERSION));

    // Always install English catalog first as baseline fallback.
    QTranslator translatorEn;
    if (translatorEn.load(QStringLiteral(":/i18n/qsensors_en.qm"))) {
        QCoreApplication::installTranslator(&translatorEn);
    }

    // Overlay locale-specific catalog if available.
    QTranslator translatorLocale;
    if (translatorLocale.load(QLocale(), QStringLiteral("qsensors"), QStringLiteral("_"), QStringLiteral(":/i18n"))) {
        QCoreApplication::installTranslator(&translatorLocale);
    }

    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/qsensors.png")));
    int exitCode = 0;
    {
        MainWindow window([] { return std::make_unique<LibsensorsSource>(); });
        window.show();
        exitCode = QApplication::exec();
    } // the window saves its state on close and stops the sensor worker here

    // A sensor read that never returned left its worker running; it may return at any
    // moment and run Qt and libsensors code. Destroying QApplication and the statics under
    // it could crash at exit, so end without teardown (settings are already written).
    if (SensorMonitor::workerAbandoned()) {
        std::fflush(nullptr);
        std::_Exit(exitCode);
    }
    return exitCode;
}
