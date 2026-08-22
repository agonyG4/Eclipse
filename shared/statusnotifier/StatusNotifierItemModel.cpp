#include "statusnotifier/StatusNotifierItemModel.hpp"

#include <algorithm>

namespace Astrea::StatusNotifier {

StatusNotifierItemModel::StatusNotifierItemModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int StatusNotifierItemModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant StatusNotifierItemModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row &row = m_rows.at(index.row());
    switch (role) {
    case KeyRole: return row.snapshot.address.key();
    case IdRole: return row.snapshot.id;
    case TitleRole: return row.snapshot.title;
    case CategoryRole: return row.snapshot.category;
    case StatusRole: return itemStatusName(row.snapshot.status);
    case IconSourceRole: return row.iconSource;
    case TooltipTitleRole: return row.snapshot.tooltipTitle;
    case TooltipDescriptionRole: return row.snapshot.tooltipDescription;
    case HasMenuRole: return !row.snapshot.menuPath.isEmpty();
    case OnlyMenuRole: return row.snapshot.itemIsMenu;
    case ReadyRole: return row.snapshot.ready;
    default: return {};
    }
}

QHash<int, QByteArray> StatusNotifierItemModel::roleNames() const
{
    return {
        {KeyRole, "key"}, {IdRole, "id"}, {TitleRole, "title"}, {CategoryRole, "category"},
        {StatusRole, "status"}, {IconSourceRole, "iconSource"},
        {TooltipTitleRole, "tooltipTitle"}, {TooltipDescriptionRole, "tooltipDescription"},
        {HasMenuRole, "hasMenu"}, {OnlyMenuRole, "onlyMenu"}, {ReadyRole, "ready"}
    };
}

void StatusNotifierItemModel::upsert(const ItemSnapshot &snapshot, const QString &iconSource)
{
    const QString key = snapshot.address.key();
    auto it = std::find_if(m_rows.begin(), m_rows.end(), [&key](const Row &row) {
        return row.snapshot.address.key() == key;
    });
    if (it != m_rows.end()) {
        const int row = std::distance(m_rows.begin(), it);
        it->snapshot = snapshot;
        it->iconSource = iconSource;
        emit dataChanged(index(row), index(row));
        return;
    }
    const int insertion = m_rows.size();
    beginInsertRows({}, insertion, insertion);
    m_rows.append({snapshot, iconSource});
    endInsertRows();
}

bool StatusNotifierItemModel::removeKey(const QString &key)
{
    auto it = std::find_if(m_rows.begin(), m_rows.end(), [&key](const Row &row) {
        return row.snapshot.address.key() == key;
    });
    if (it == m_rows.end())
        return false;
    const int row = std::distance(m_rows.begin(), it);
    beginRemoveRows({}, row, row);
    m_rows.erase(it);
    endRemoveRows();
    emit itemRemoved(key);
    return true;
}

void StatusNotifierItemModel::clear()
{
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

ItemSnapshot StatusNotifierItemModel::item(const QString &key) const
{
    for (const Row &row : m_rows) {
        if (row.snapshot.address.key() == key)
            return row.snapshot;
    }
    return {};
}

bool StatusNotifierItemModel::contains(const QString &key) const
{
    return std::any_of(m_rows.cbegin(), m_rows.cend(), [&key](const Row &row) {
        return row.snapshot.address.key() == key;
    });
}

QStringList StatusNotifierItemModel::keys() const
{
    QStringList result;
    result.reserve(m_rows.size());
    for (const Row &row : m_rows)
        result.append(row.snapshot.address.key());
    return result;
}

} // namespace Astrea::StatusNotifier
