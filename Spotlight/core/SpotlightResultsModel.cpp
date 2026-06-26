#include "core/SpotlightResultsModel.hpp"

SpotlightResultsModel::SpotlightResultsModel(QObject *parent)
    : QAbstractListModel(parent) {}

int SpotlightResultsModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_results.size());
}

QVariant SpotlightResultsModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_results.size())
        return {};

    const auto &item = m_results.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return item.name;
    case GenericNameRole: return item.genericName;
    case CommentRole: return item.comment;
    case IconNameRole: return item.icon;
    case EntryIdRole: return item.id;
    case ExecRole: return item.exec;
    case DesktopFileNameRole: return item.desktopFileName;
    case DesktopFilePathRole: return item.desktopFilePath;
    case StartupWmClassRole: return item.startupWmClass;
    case TerminalRole: return item.terminal;
    case ScoreRole: return item.score;
    case UsageRole: return item.usage;
    default: return {};
    }
}

QHash<int, QByteArray> SpotlightResultsModel::roleNames() const {
    return {
        {NameRole, "name"},
        {GenericNameRole, "genericName"},
        {CommentRole, "comment"},
        {IconNameRole, "iconName"},
        {EntryIdRole, "entryId"},
        {ExecRole, "exec"},
        {DesktopFileNameRole, "desktopFileName"},
        {DesktopFilePathRole, "desktopFilePath"},
        {StartupWmClassRole, "startupWmClass"},
        {TerminalRole, "terminal"},
        {ScoreRole, "score"},
        {UsageRole, "usage"},
    };
}

void SpotlightResultsModel::setResults(const QJsonArray &results) {
    beginResetModel();
    m_results.clear();
    m_results.reserve(results.size());

    for (const auto &val : results) {
        QJsonObject obj = val.toObject();
        SearchResultItem item;
        item.id = obj.value(QStringLiteral("id")).toString();
        item.name = obj.value(QStringLiteral("name")).toString();
        item.genericName = obj.value(QStringLiteral("genericName")).toString();
        item.comment = obj.value(QStringLiteral("comment")).toString();
        item.icon = obj.value(QStringLiteral("icon")).toString();
        item.exec = obj.value(QStringLiteral("exec")).toString();
        item.desktopFileName = obj.value(QStringLiteral("desktopFileName")).toString();
        item.desktopFilePath = obj.value(QStringLiteral("desktopFilePath")).toString();
        item.startupWmClass = obj.value(QStringLiteral("startupWmClass")).toString();
        item.terminal = obj.value(QStringLiteral("terminal")).toBool();
        item.score = obj.value(QStringLiteral("score")).toInt();
        item.usage = obj.value(QStringLiteral("usage")).toInt();
        m_results.append(item);
    }
    endResetModel();
}

void SpotlightResultsModel::clear() {
    if (m_results.isEmpty()) return;
    beginResetModel();
    m_results.clear();
    endResetModel();
}

SearchResultItem SpotlightResultsModel::resultAt(int row) const {
    if (row < 0 || row >= m_results.size())
        return {};
    return m_results.at(row);
}
