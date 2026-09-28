// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#include "ui/panels/category_row_layout.h"

#include "ui/panels/card_grid_plan.h"
#include "ui/theme/app_theme.h"

#include <QWidget>

#include <algorithm>
#include <utility>

namespace {
    int rowCount(const qsizetype cards, const int columns) {
        return static_cast<int>((cards + columns - 1) / columns);
    }

    // Asks the widget directly: QWidgetItem reports 0 for new cards until they are shown.
    int preferredHeight(const QLayoutItem *item) {
        const QWidget *widget = item->widget();
        if (!widget)
            return item->sizeHint().height();
        return std::clamp(widget->sizeHint().height(), widget->minimumHeight(), widget->maximumHeight());
    }
}

CategoryRowLayout::CategoryRowLayout(const CardGridPlan *plan, QString chip, QWidget *parent)
    : QLayout(parent), m_plan(plan), m_chip(std::move(chip)) {
    setContentsMargins(0, 0, 0, 0);
}

CategoryRowLayout::~CategoryRowLayout() {
    while (QLayoutItem *item = takeAt(0))
        delete item;
}

void CategoryRowLayout::addCategory(QWidget *title, const QList<QWidget *> &cards) {
    m_categories.append(Category{});
    addWidget(title);
    for (QWidget *card: cards)
        addWidget(card);
}

void CategoryRowLayout::clear() {
    // Take the items out first, so deleting the widgets does not call back into takeAt().
    const QList<Category> categories = std::exchange(m_categories, {});
    for (const Category &category: categories) {
        QList<QLayoutItem *> items = category.cards;
        if (category.title)
            items.prepend(category.title);
        for (QLayoutItem *item: items) {
            delete item->widget();
            delete item;
        }
    }
    invalidate();
}

void CategoryRowLayout::addItem(QLayoutItem *item) {
    // Items arrive through addCategory(): the first one of a category is its title.
    if (m_categories.isEmpty())
        m_categories.append(Category{});
    Category &category = m_categories.last();
    if (!category.title && category.cards.isEmpty())
        category.title = item;
    else
        category.cards.append(item);
    invalidate();
}

QLayoutItem *CategoryRowLayout::itemAt(int index) const {
    for (const Category &category: m_categories) {
        if (category.title) {
            if (index == 0)
                return category.title;
            --index;
        }
        if (index < category.cards.size())
            return index >= 0 ? category.cards.at(index) : nullptr;
        index -= static_cast<int>(category.cards.size());
    }
    return nullptr;
}

QLayoutItem *CategoryRowLayout::takeAt(int index) {
    for (auto it = m_categories.begin(); it != m_categories.end(); ++it) {
        QLayoutItem *taken = nullptr;
        if (it->title) {
            if (index == 0)
                taken = std::exchange(it->title, nullptr);
            else
                --index;
        }
        if (!taken && index >= 0 && index < it->cards.size())
            taken = it->cards.takeAt(index);
        if (taken) {
            if (!it->title && it->cards.isEmpty())
                m_categories.erase(it);
            invalidate();
            return taken;
        }
        index -= static_cast<int>(it->cards.size());
    }
    return nullptr;
}

int CategoryRowLayout::count() const {
    int items = 0;
    for (const Category &category: m_categories)
        items += (category.title ? 1 : 0) + static_cast<int>(category.cards.size());
    return items;
}

Qt::Orientations CategoryRowLayout::expandingDirections() const {
    return Qt::Horizontal;
}

bool CategoryRowLayout::hasHeightForWidth() const {
    return true;
}

int CategoryRowLayout::heightForWidth(const int width) const {
    const QMargins margins = contentsMargins();
    const int inner = width - margins.left() - margins.right();
    return contentHeight(columnsFor(inner)) + margins.top() + margins.bottom();
}

QSize CategoryRowLayout::minimumSize() const {
    const QMargins margins = contentsMargins();
    const QVector<int> single(m_categories.size(), 1);
    int oneRow = 0;
    for (const Category &category: m_categories)
        oneRow = std::max(oneRow, titleHeight(category) + AppTheme::kCategoryBlockSpacing + cardHeight());
    return {CardGridPlan::categoriesWidth(single) + margins.left() + margins.right(),
            oneRow + margins.top() + margins.bottom()};
}

QSize CategoryRowLayout::sizeHint() const {
    const int width = minimumSize().width();
    return {width, heightForWidth(width)};
}

void CategoryRowLayout::setGeometry(const QRect &rect) {
    QLayout::setGeometry(rect);
    const QRect area = rect.marginsRemoved(contentsMargins());
    const QVector<int> columns = columnsFor(area.width());
    const int cardWidth = m_plan->cardWidth(area.width());
    const int cardHeight = this->cardHeight();

    int x = area.x();
    for (int i = 0; i < m_categories.size(); ++i) {
        const Category &category = m_categories.at(i);
        const int cols = columns.at(i);
        const int categoryWidth = CardGridPlan::widthForColumns(cols, cardWidth);
        const int titleHeight = this->titleHeight(category);
        if (category.title)
            category.title->setGeometry(QRect(x, area.y(), categoryWidth, titleHeight));

        const int gridTop = area.y() + titleHeight + AppTheme::kCategoryBlockSpacing;
        for (int n = 0; n < category.cards.size(); ++n) {
            const int row = n / cols;
            const int col = n % cols;
            category.cards.at(n)->setGeometry(QRect(x + col * (cardWidth + AppTheme::kUnifiedHorizontalSpacing),
                                                    gridTop + row * (cardHeight + AppTheme::kGridSpacing),
                                                    cardWidth, cardHeight));
        }
        x += categoryWidth + AppTheme::kUnifiedHorizontalSpacing;
    }
}

QVector<int> CategoryRowLayout::columnsFor(const int width) const {
    const QVector<int> planned = m_plan->columns(m_chip, width);
    return planned.size() == m_categories.size() ? planned : QVector<int>(m_categories.size(), 1);
}

int CategoryRowLayout::contentHeight(const QVector<int> &columns) const {
    const int cardHeight = this->cardHeight();
    int height = 0;
    for (int i = 0; i < m_categories.size(); ++i) {
        const Category &category = m_categories.at(i);
        const int rows = std::max(1, rowCount(category.cards.size(), columns.at(i)));
        height = std::max(height, titleHeight(category) + AppTheme::kCategoryBlockSpacing
                                  + rows * cardHeight + (rows - 1) * AppTheme::kGridSpacing);
    }
    return height;
}

int CategoryRowLayout::titleHeight(const Category &category) const {
    return category.title ? preferredHeight(category.title) : 0;
}

int CategoryRowLayout::cardHeight() const {
    // All cards have the same fixed height.
    for (const Category &category: m_categories) {
        if (!category.cards.isEmpty())
            return preferredHeight(category.cards.first());
    }
    return 0;
}
