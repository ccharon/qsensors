// SPDX-License-Identifier: GPL-2.0-or-later

#include "theme/app_theme.h"

#include <QApplication>
#include <QLabel>
#include <QtTest/QtTest>

// Verifies that style sheets follow a runtime palette switch (system light/dark toggle).
class ThemeRefreshTest final : public QObject {
    Q_OBJECT

private slots:
    void refresh_resolves_palette_references_against_new_palette();
    void refresh_keeps_style_sheet_text_and_ignores_unstyled_widgets();
};

namespace {
    QPalette paletteWithHighlight(const QColor &highlight) {
        QPalette palette = QApplication::palette();
        palette.setColor(QPalette::Highlight, highlight);
        return palette;
    }
}

void ThemeRefreshTest::refresh_resolves_palette_references_against_new_palette() {
    QApplication::setPalette(paletteWithHighlight(Qt::red));
    QWidget root;
    auto *label = new QLabel(QStringLiteral("x"), &root);
    label->setStyleSheet(QStringLiteral("QLabel { color: palette(highlight); }"));
    label->ensurePolished();
    QCOMPARE(label->palette().color(QPalette::WindowText), QColor(Qt::red));

    QApplication::setPalette(paletteWithHighlight(Qt::blue));
    AppTheme::refreshStyleSheets(&root);
    label->ensurePolished();
    QCOMPARE(label->palette().color(QPalette::WindowText), QColor(Qt::blue));
}

void ThemeRefreshTest::refresh_keeps_style_sheet_text_and_ignores_unstyled_widgets() {
    QWidget root;
    auto *styled = new QLabel(&root);
    auto *plain = new QLabel(&root);
    const QString sheet = QStringLiteral("QLabel { background: palette(window); }");
    styled->setStyleSheet(sheet);

    AppTheme::refreshStyleSheets(&root);
    AppTheme::refreshStyleSheets(nullptr);

    QCOMPARE(styled->styleSheet(), sheet);
    QVERIFY(plain->styleSheet().isEmpty());
}

QTEST_MAIN(ThemeRefreshTest)
#include "test_theme_refresh.moc"
