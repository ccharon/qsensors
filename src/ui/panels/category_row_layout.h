// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Christian Charon <ccharon@mailbox.org>

#pragma once

#include <QLayout>
#include <QList>
#include <QString>
#include <QVector>

class CardGridPlan;

/**
 * Categories of one chip side by side, each a title above a grid of sensor cards.
 * Columns and card width come from the CardGridPlan shared by all chips, so a new
 * width only moves the cards; nothing is rebuilt. The height follows the width
 * (heightForWidth), since fewer columns mean more rows.
 */
class CategoryRowLayout final : public QLayout {
public:
    /** Lays out the categories of @p chip as planned by @p plan, which must outlive the layout. */
    CategoryRowLayout(const CardGridPlan *plan, QString chip, QWidget *parent = nullptr);

    ~CategoryRowLayout() override;

    /** Appends a category: @p title above a grid of @p cards, in plan order. */
    void addCategory(QWidget *title, const QList<QWidget *> &cards);

    /** Removes all categories and deletes their widgets. */
    void clear();

    void addItem(QLayoutItem *item) override;
    [[nodiscard]] QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    [[nodiscard]] int count() const override;

    [[nodiscard]] Qt::Orientations expandingDirections() const override;
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;
    [[nodiscard]] QSize minimumSize() const override;
    [[nodiscard]] QSize sizeHint() const override;
    void setGeometry(const QRect &rect) override;

private:
    struct Category {
        QLayoutItem *title = nullptr;
        QList<QLayoutItem *> cards;
    };

    /** Columns per category for @p width; one column each if the plan does not match yet. */
    [[nodiscard]] QVector<int> columnsFor(int width) const;

    /** Height of the tallest category with @p columns (without margins). */
    [[nodiscard]] int contentHeight(const QVector<int> &columns) const;

    [[nodiscard]] int titleHeight(const Category &category) const;
    [[nodiscard]] int cardHeight() const;

    const CardGridPlan *m_plan;
    QString m_chip;
    QList<Category> m_categories;
};
