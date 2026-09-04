#include "core/navigation/SettingsNavigationModel.hpp"

namespace {
QString kindName(SettingsNavigationEntry::Kind kind)
{
    switch (kind) {
    case SettingsNavigationEntry::Kind::Page:
        return QStringLiteral("page");
    case SettingsNavigationEntry::Kind::Section:
        return QStringLiteral("section");
    case SettingsNavigationEntry::Kind::Child:
        return QStringLiteral("child");
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
    const int firstSelectable = firstSelectableSourceIndex();
    if (firstSelectable >= 0)
        m_selectedId = m_entries.at(firstSelectable).id;
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
        return isSelectable(entry);
    case SelectedRole:
        return isSelectable(entry) && entry.id == m_selectedId;
    case LabelKeyRole:
        return entry.labelKey;
    case SymRole:
        return entry.sym;
    case IconSourceRole:
        return entry.iconSource;
    case PageSourceRole:
        return entry.pageSource;
    case SectionKeyRole:
        return entry.sectionKey;
    case ParentSectionRole:
        return entry.parentSection;
    case ExpandedRole:
        return entry.expanded;
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
        {SectionKeyRole, QByteArrayLiteral("sectionKey")},
        {ParentSectionRole, QByteArrayLiteral("parentSection")},
        {ExpandedRole, QByteArrayLiteral("expanded")},
    };
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
    if (!isSelectable(entry))
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

bool SettingsNavigationModel::toggleSection(const QString &sectionId)
{
    const int sourceIndex = sectionIndexForId(sectionId);
    if (sourceIndex < 0)
        return false;

    beginResetModel();
    m_entries[sourceIndex].expanded = !m_entries.at(sourceIndex).expanded;
    rebuildVisibleRows();
    endResetModel();
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
        {QStringLiteral("entryEnabled"), isSelectable(entry)},
        {QStringLiteral("sectionKey"), entry.sectionKey},
        {QStringLiteral("parentSection"), entry.parentSection},
        {QStringLiteral("expanded"), entry.expanded},
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
    return isSelectable(entry);
}

void SettingsNavigationModel::rebuildVisibleRows()
{
    m_visibleRows.clear();

    for (int i = 0; i < m_entries.size(); ++i) {
        const SettingsNavigationEntry &entry = m_entries.at(i);
        if (entry.kind != SettingsNavigationEntry::Kind::Child) {
            m_visibleRows.append(i);
            continue;
        }

        const int sectionIndex = sectionIndexForId(entry.parentSection);
        const bool expanded = sectionIndex < 0 || m_entries.at(sectionIndex).expanded;
        if (expanded || entry.id == m_selectedId)
            m_visibleRows.append(i);
    }
}

bool SettingsNavigationModel::isSelectable(const SettingsNavigationEntry &entry) const
{
    return entry.enabled
        && (entry.kind == SettingsNavigationEntry::Kind::Page
            || entry.kind == SettingsNavigationEntry::Kind::Child)
        && !entry.pageSource.isEmpty();
}

int SettingsNavigationModel::sectionIndexForId(const QString &sectionId) const
{
    const int index = sourceIndexForId(sectionId);
    if (index < 0 || m_entries.at(index).kind != SettingsNavigationEntry::Kind::Section)
        return -1;
    return index;
}

int SettingsNavigationModel::firstSelectableSourceIndex() const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (isSelectable(m_entries.at(i)))
            return i;
    }
    return -1;
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
