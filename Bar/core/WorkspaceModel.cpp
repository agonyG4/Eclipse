#include "core/WorkspaceModel.hpp"

#include <algorithm>

namespace {

bool numericLess(const WorkspaceItem &left, const WorkspaceItem &right)
{
    bool leftOk = false;
    bool rightOk = false;
    const int leftNumber = left.id.toInt(&leftOk);
    const int rightNumber = right.id.toInt(&rightOk);
    if (leftOk && rightOk && leftNumber != rightNumber)
        return leftNumber < rightNumber;
    if (leftOk != rightOk)
        return leftOk;
    return left.id < right.id;
}

} // namespace

WorkspaceModel::WorkspaceModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int WorkspaceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant WorkspaceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const WorkspaceItem &item = m_items.at(index.row());
    switch (role) {
    case IdRole: return item.id;
    case ActiveRole: return item.active;
    case OccupiedRole: return item.occupied;
    case UrgentRole: return item.urgent;
    case OutputIdRole: return item.outputId;
    default: return {};
    }
}

QHash<int, QByteArray> WorkspaceModel::roleNames() const
{
    return {
        {IdRole, QByteArrayLiteral("id")},
        {ActiveRole, QByteArrayLiteral("active")},
        {OccupiedRole, QByteArrayLiteral("occupied")},
        {UrgentRole, QByteArrayLiteral("urgent")},
        {OutputIdRole, QByteArrayLiteral("outputId")},
    };
}

void WorkspaceModel::replaceWorkspaces(QVector<WorkspaceItem> workspaces)
{
    std::stable_sort(workspaces.begin(), workspaces.end(), numericLess);
    beginResetModel();
    m_items = std::move(workspaces);
    endResetModel();
    emit countChanged();
}
