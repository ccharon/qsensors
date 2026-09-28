// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include "ui/theme/app_theme.h"

#include <QHash>
#include <QString>
#include <QVector>

/**
 * Column planning for the sensor cards of all chips. Every chip lays out its
 * categories side by side; the columns per category depend on the width, and one
 * card width is shared by all chips, so all cards always have the same size.
 * Widths are measured inside a chip card (without frame and margins).
 */
class CardGridPlan {
public:
    /** Sensor counts per category (in category order) of every shown chip; returns true if they changed. */
    bool setChips(const QHash<QString, QVector<int>> &sensorCounts);

    /** Columns per category of @p chip within @p availableWidth. */
    [[nodiscard]] QVector<int> columns(const QString &chip, int availableWidth) const;

    /** Card width shared by all chips: the chip with the least spare width sets it. */
    [[nodiscard]] int cardWidth(int availableWidth) const;

    /** Narrowest width at which every chip still gets one column per category. */
    [[nodiscard]] int minimumWidth() const;

    /**
     * Widest useful width: every chip has all the columns that still save a row, and
     * all cards have kCardMaxWidth. More width would only add empty space.
     */
    [[nodiscard]] int maximumWidth() const;

    /**
     * Columns per category for @p sensorCounts within @p availableWidth. Every category
     * gets one column; each further column goes to the category with the most rows, as
     * long as it saves a row and fits.
     */
    [[nodiscard]] static QVector<int> columnsForCategories(const QVector<int> &sensorCounts, int availableWidth);

    /** Width of categories laid out with @p columns and @p cardWidth, including the gaps. */
    [[nodiscard]] static int categoriesWidth(const QVector<int> &columns, int cardWidth = AppTheme::kCardMinWidth);

    /**
     * Card width that spreads the width left over by @p columns within @p availableWidth
     * evenly over all cards, between kCardMinWidth and kCardMaxWidth.
     */
    [[nodiscard]] static int cardWidthFor(const QVector<int> &columns, int availableWidth);

    /** Width of one category with @p columns cards of @p cardWidth, including the gaps. */
    [[nodiscard]] static int widthForColumns(int columns, int cardWidth);

private:
    QHash<QString, QVector<int>> m_sensorCounts;
};
