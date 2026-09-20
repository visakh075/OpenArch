#pragma once

#include <QSortFilterProxyModel>
#include <QString>
#include <QModelIndex>

#include "MainWindow.h"

class ArchitectureFilterProxyModel : public QSortFilterProxyModel
{
public:
    enum class FilterCategory : int
    {
        All    = 0,
        Nodes  = 1,
        Layers = 2
    };

    explicit ArchitectureFilterProxyModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setFilterCaseSensitivity(Qt::CaseInsensitive);
    }

    void setSearchText(const QString& text)
    {
        searchText_ = text.trimmed();
        invalidateFilter();
    }

    void setCategoryFilter(int filterCategory)
    {
        categoryFilter_ = static_cast<FilterCategory>(filterCategory);
        invalidateFilter();
    }

    const QString& searchText() const { return searchText_; }
    FilterCategory categoryFilter() const { return categoryFilter_; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override
    {
        QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
        if (!index.isValid())
            return false;

        // 1. Category filter
        auto itemType = static_cast<ItemType>(index.data(NavRole::Type).toInt());

        if (categoryFilter_ == FilterCategory::Nodes)
        {
            if (itemType == ItemType::Layer)
                return false;
            if (!sourceParent.isValid() && index.data(Qt::DisplayRole).toString() == "Layers")
                return false;
        }
        else if (categoryFilter_ == FilterCategory::Layers)
        {
            if (itemType == ItemType::Node)
                return false;
            if (!sourceParent.isValid() && index.data(Qt::DisplayRole).toString() == "Nodes")
                return false;
        }

        // 2. Search text filter
        if (searchText_.isEmpty())
            return true;

        // Direct item match
        if (itemMatchesSearch(index))
            return true;

        // If item has children, accept if any descendant matches
        if (hasMatchingDescendant(index))
            return true;

        // If parent matched directly (e.g. searching "Node" matches the "Nodes" category),
        // show child items if allowed by category filter
        if (sourceParent.isValid() && itemMatchesSearch(sourceParent))
            return true;

        return false;
    }

private:
    bool itemMatchesSearch(const QModelIndex& index) const
    {
        if (searchText_.isEmpty())
            return true;

        // Display name
        QString name = index.data(Qt::DisplayRole).toString();
        if (name.contains(searchText_, Qt::CaseInsensitive))
            return true;

        // Subtype (e.g. node type, layer kind)
        QString subtype = index.data(NavRole::Subtype).toString();
        if (!subtype.isEmpty() && subtype.contains(searchText_, Qt::CaseInsensitive))
            return true;

        // ID search (e.g. "#1" or "id:1" or exact numeric ID)
        qulonglong id = index.data(NavRole::Id).toULongLong();
        if (id > 0)
        {
            if (searchText_.startsWith("#") && searchText_.mid(1).trimmed() == QString::number(id))
                return true;
            if (searchText_.startsWith("id:", Qt::CaseInsensitive) && searchText_.mid(3).trimmed() == QString::number(id))
                return true;
            if (searchText_ == QString::number(id))
                return true;
        }

        return false;
    }

    bool hasMatchingDescendant(const QModelIndex& parent) const
    {
        int count = sourceModel()->rowCount(parent);
        for (int i = 0; i < count; ++i)
        {
            QModelIndex child = sourceModel()->index(i, 0, parent);
            if (itemMatchesSearch(child) || hasMatchingDescendant(child))
                return true;
        }
        return false;
    }

    QString searchText_;
    FilterCategory categoryFilter_{FilterCategory::All};
};
