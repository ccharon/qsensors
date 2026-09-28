// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/panels/card_grid_plan.h"

#include "ui/theme/app_theme.h"

#include <algorithm>

bool CardGridPlan::setChips(const QHash<QString, QVector<int>> &sensorCounts) {
    if (sensorCounts == m_sensorCounts)
        return false;
    m_sensorCounts = sensorCounts;
    return true;
}

QVector<int> CardGridPlan::columns(const QString &chip, const int availableWidth) const {
    return columnsForCategories(m_sensorCounts.value(chip), availableWidth);
}

int CardGridPlan::cardWidth(const int availableWidth) const {
    int width = AppTheme::kCardMaxWidth;
    for (const QVector<int> &counts: m_sensorCounts) {
        width = std::min(width, cardWidthFor(columnsForCategories(counts, availableWidth), availableWidth));
    }
    return width;
}

int CardGridPlan::minimumWidth() const {
    int maxCategoryCount = 1;
    for (const QVector<int> &counts: m_sensorCounts) {
        maxCategoryCount = std::max(maxCategoryCount, static_cast<int>(counts.size()));
    }
    return categoriesWidth(QVector<int>(maxCategoryCount, 1));
}

QVector<int> CardGridPlan::columnsForCategories(const QVector<int> &sensorCounts, const int availableWidth) {
    QVector<int> columns(sensorCounts.size(), 1);
    const int pitch = AppTheme::kCardMinWidth + AppTheme::kUnifiedHorizontalSpacing;
    int used = categoriesWidth(columns);
    const auto rows = [](const int count, const int cols) { return (count + cols - 1) / cols; };

    while (used + pitch <= availableWidth) {
        int best = -1;
        int bestRows = 0;
        for (int i = 0; i < sensorCounts.size(); ++i) {
            const int count = std::max(1, sensorCounts.at(i));
            const bool savesRow = rows(count, columns.at(i) + 1) < rows(count, columns.at(i));
            if (savesRow && columns.at(i) < AppTheme::kMaxColumnsPerCategory && rows(count, columns.at(i)) > bestRows) {
                best = i;
                bestRows = rows(count, columns.at(i));
            }
        }
        if (best < 0)
            break;
        ++columns[best];
        used += pitch;
    }
    return columns;
}

int CardGridPlan::categoriesWidth(const QVector<int> &columns) {
    int width = 0;
    for (const int cols: columns)
        width += widthForColumns(cols, AppTheme::kCardMinWidth);
    return width + std::max<int>(0, static_cast<int>(columns.size()) - 1) * AppTheme::kUnifiedHorizontalSpacing;
}

int CardGridPlan::cardWidthFor(const QVector<int> &columns, const int availableWidth) {
    int totalColumns = 0;
    for (const int cols: columns)
        totalColumns += cols;
    const int spare = availableWidth - categoriesWidth(columns);
    if (totalColumns == 0 || spare <= 0)
        return AppTheme::kCardMinWidth;
    return std::min(AppTheme::kCardMinWidth + spare / totalColumns, AppTheme::kCardMaxWidth);
}

int CardGridPlan::widthForColumns(const int columns, const int cardWidth) {
    return (columns * cardWidth) + ((columns - 1) * AppTheme::kUnifiedHorizontalSpacing);
}
