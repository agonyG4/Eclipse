#include "core/navigation/SettingsNavigationModel.hpp"

namespace {
QString kindName(SettingsNavigationEntry::Kind kind)
{
    switch (kind) {
    case SettingsNavigationEntry::Kind::Page:
        return QStringLiteral("page");
    case SettingsNavigationEntry::Kind::Group:
        return QStringLiteral("group");
    case SettingsNavigationEntry::Kind::Spacer:
        return QStringLiteral("spacer");
    }
    return QStringLiteral("page");
}
}

SettingsNavigationModel::SettingsNavigationModel(const SettingsNavigationCatalog &catalog,
                                                 QObject *parent)
    : QAbstractListModel(parent)
    , m_entries(catalog.entries())
{
    rebuildVisibleRows();
    m_selectedId = QStringLiteral("system");
}

SettingsNavigationModel::SettingsNavigationModel(QObject *parent)
    : SettingsNavigationModel(SettingsNavigationCatalog(), parent)
{
}

int SettingsNavigationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_visibleRows.size());
}

QVariant SettingsNavigationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleRows.size())
        return {};

    const SettingsNavigationEntry &entry = m_entries.at(m_visibleRows.at(index.row()));
    switch (role) {
    case IdRole:
        return entry.id;
    case TitleRole:
    case LabelRole:
        return entry.label;
    case SubtitleRole:
        return entry.subtitle;
    case IconNameRole:
    case IconKeyRole:
        return entry.iconKey;
    case KindRole:
        return kindName(entry.kind);
    case EnabledRole:
        return entry.enabled;
    case SelectedRole:
        return entry.kind != SettingsNavigationEntry::Kind::Spacer && entry.id == m_selectedId;
    case LabelKeyRole:
        return entry.labelKey;
    case SymRole:
        return entry.sym;
    case IconSourceRole:
        return entry.iconSource;
    case PageSourceRole:
        return entry.pageSource;
    default:
        return {};
    }
}

QHash<int, QByteArray> SettingsNavigationModel::roleNames() const
{
    return {
        {IdRole, QByteArrayLiteral("entryId")},
        {TitleRole, QByteArrayLiteral("title")},
        {SubtitleRole, QByteArrayLiteral("subtitle")},
        {IconNameRole, QByteArrayLiteral("iconName")},
        {KindRole, QByteArrayLiteral("kind")},
        {EnabledRole, QByteArrayLiteral("entryEnabled")},
        {SelectedRole, QByteArrayLiteral("selected")},
        {LabelRole, QByteArrayLiteral("label")},
        {LabelKeyRole, QByteArrayLiteral("labelKey")},
        {SymRole, QByteArrayLiteral("sym")},
        {IconSourceRole, QByteArrayLiteral("iconSource")},
        {IconKeyRole, QByteArrayLiteral("iconKey")},
        {PageSourceRole, QByteArrayLiteral("pageSource")},
    };
}

QString SettingsNavigationModel::filterText() const
{
    return m_filterText;
}

void SettingsNavigationModel::setFilterText(const QString &filterText)
{
    const QString normalized = filterText.trimmed();
    if (m_filterText == normalized)
        return;

    beginResetModel();
    m_filterText = normalized;
    rebuildVisibleRows();
    endResetModel();
    emit filterTextChanged();
}

QString SettingsNavigationModel::selectedId() const
{
    return m_selectedId;
}

bool SettingsNavigationModel::setSelectedId(const QString &id)
{
    const int sourceIndex = sourceIndexForId(id);
    if (sourceIndex < 0)
        return false;

    const SettingsNavigationEntry &entry = m_entries.at(sourceIndex);
    if (!entry.enabled || entry.kind == SettingsNavigationEntry::Kind::Spacer)
        return false;
    if (m_selectedId == id)
        return true;

    const int oldSourceIndex = sourceIndexForId(m_selectedId);
    m_selectedId = id;

    const int oldVisibleRow = visibleRowForSourceIndex(oldSourceIndex);
    if (oldVisibleRow >= 0) {
        const QModelIndex oldIndex = index(oldVisibleRow, 0);
        emit dataChanged(oldIndex, oldIndex, {SelectedRole});
    }

    const int newVisibleRow = visibleRowForSourceIndex(sourceIndex);
    if (newVisibleRow >= 0) {
        const QModelIndex newIndex = index(newVisibleRow, 0);
        emit dataChanged(newIndex, newIndex, {SelectedRole});
    }

    emit selectedIdChanged();
    return true;
}

QVariantMap SettingsNavigationModel::get(int row) const
{
    if (row < 0 || row >= m_visibleRows.size())
        return {};
    const SettingsNavigationEntry &entry = m_entries.at(m_visibleRows.at(row));
    return {
        {QStringLiteral("entryId"), entry.id},
        {QStringLiteral("label"), entry.label},
        {QStringLiteral("labelKey"), entry.labelKey},
        {QStringLiteral("subtitle"), entry.subtitle},
        {QStringLiteral("kind"), kindName(entry.kind)},
        {QStringLiteral("sym"), entry.sym},
        {QStringLiteral("iconSource"), entry.iconSource},
        {QStringLiteral("iconKey"), entry.iconKey},
        {QStringLiteral("pageSource"), entry.pageSource},
    };
}

QString SettingsNavigationModel::titleForId(const QString &id) const
{
    const int sourceIndex = sourceIndexForId(id);
    return sourceIndex >= 0 ? m_entries.at(sourceIndex).label : QString();
}

QUrl SettingsNavigationModel::pageSourceForId(const QString &id) const
{
    const int sourceIndex = sourceIndexForId(id);
    return sourceIndex >= 0 ? m_entries.at(sourceIndex).pageSource : QUrl();
}

bool SettingsNavigationModel::containsSelectableId(const QString &id) const
{
    const int sourceIndex = sourceIndexForId(id);
    if (sourceIndex < 0)
        return false;
    const SettingsNavigationEntry &entry = m_entries.at(sourceIndex);
    return entry.kind != SettingsNavigationEntry::Kind::Spacer && entry.enabled;
}

void SettingsNavigationModel::rebuildVisibleRows()
{
    m_visibleRows.clear();
    const bool filtering = !m_filterText.isEmpty();

    for (int i = 0; i < m_entries.size(); ++i) {
        const SettingsNavigationEntry &entry = m_entries.at(i);
        if (entry.kind == SettingsNavigationEntry::Kind::Spacer) {
            if (!filtering)
                m_visibleRows.append(i);
            continue;
        }

        if (!filtering
            || entry.label.contains(m_filterText, Qt::CaseInsensitive)
            || entry.subtitle.contains(m_filterText, Qt::CaseInsensitive)
            || entry.id.contains(m_filterText, Qt::CaseInsensitive)) {
            m_visibleRows.append(i);
        }
    }
}

int SettingsNavigationModel::sourceIndexForId(const QString &id) const
{
    if (id.isEmpty())
        return -1;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == id)
            return i;
    }
    return -1;
}

int SettingsNavigationModel::visibleRowForSourceIndex(int sourceIndex) const
{
    return static_cast<int>(m_visibleRows.indexOf(sourceIndex));
}
