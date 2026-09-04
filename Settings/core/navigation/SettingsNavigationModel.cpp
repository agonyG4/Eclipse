#include "core/navigation/SettingsNavigationModel.hpp"

namespace {

QString kindName(SettingsNavigationEntry::Kind kind)
{
    switch (kind) {
    case SettingsNavigationEntry::Kind::Page:
        return QStringLiteral("page");
    case SettingsNavigationEntry::Kind::Hub:
        return QStringLiteral("hub");
    case SettingsNavigationEntry::Kind::Spacer:
        return QStringLiteral("spacer");
    }
    return QStringLiteral("page");
}

} // namespace

SettingsNavigationModel::SettingsNavigationModel(const SettingsNavigationCatalog &catalog,
                                                 QObject *parent)
    : QAbstractListModel(parent)
    , m_entries(catalog.entries())
{
    for (int i = 0; i < m_entries.size(); ++i) {
        const auto &entry = m_entries.at(i);
        if (entry.sidebarVisible || entry.kind == SettingsNavigationEntry::Kind::Spacer)
            m_sidebarRows.append(i);
    }
}

SettingsNavigationModel::SettingsNavigationModel(QObject *parent)
    : SettingsNavigationModel(SettingsNavigationCatalog(), parent)
{
}

int SettingsNavigationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_sidebarRows.size());
}

QVariant SettingsNavigationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_sidebarRows.size())
        return {};

    const SettingsNavigationEntry &entry = m_entries.at(m_sidebarRows.at(index.row()));
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
        return isNavigable(entry);
    case LabelKeyRole:
        return entry.labelKey;
    case SymRole:
        return entry.sym;
    case IconSourceRole:
        return entry.iconSource;
    case PageSourceRole:
        return entry.pageSource;
    case SidebarVisibleRole:
        return entry.sidebarVisible;
    case ParentIdRole:
        return entry.parentId;
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
        {LabelRole, QByteArrayLiteral("label")},
        {LabelKeyRole, QByteArrayLiteral("labelKey")},
        {SymRole, QByteArrayLiteral("sym")},
        {IconSourceRole, QByteArrayLiteral("iconSource")},
        {IconKeyRole, QByteArrayLiteral("iconKey")},
        {PageSourceRole, QByteArrayLiteral("pageSource")},
        {SidebarVisibleRole, QByteArrayLiteral("sidebarVisible")},
        {ParentIdRole, QByteArrayLiteral("parentId")},
    };
}

const SettingsNavigationEntry *SettingsNavigationModel::entryForId(const QString &id) const
{
    const int index = sourceIndexForId(id);
    return index >= 0 ? &m_entries.at(index) : nullptr;
}

QVector<SettingsNavigationEntry> SettingsNavigationModel::childrenForId(const QString &id) const
{
    QVector<SettingsNavigationEntry> children;
    for (const auto &entry : m_entries) {
        if (entry.parentId == id && isNavigable(entry))
            children.append(entry);
    }
    return children;
}

QString SettingsNavigationModel::firstNavigableSidebarDestination() const
{
    for (const int sourceIndex : m_sidebarRows) {
        const auto &entry = m_entries.at(sourceIndex);
        if (isNavigable(entry))
            return entry.id;
    }
    return {};
}

QString SettingsNavigationModel::sidebarAncestorForId(const QString &id) const
{
    QString candidate = id;
    while (!candidate.isEmpty()) {
        const auto *entry = entryForId(candidate);
        if (!entry)
            return {};
        if (entry->sidebarVisible)
            return entry->id;
        candidate = entry->parentId;
    }
    return {};
}

QVariantMap SettingsNavigationModel::get(int row) const
{
    if (row < 0 || row >= m_sidebarRows.size())
        return {};
    return descriptorForEntry(m_entries.at(m_sidebarRows.at(row)));
}

QVariantMap SettingsNavigationModel::descriptorForId(const QString &id) const
{
    const auto *entry = entryForId(id);
    return entry ? descriptorForEntry(*entry) : QVariantMap{};
}

QVariantList SettingsNavigationModel::childDescriptorsForId(const QString &id) const
{
    QVariantList descriptors;
    for (const auto &entry : childrenForId(id))
        descriptors.append(descriptorForEntry(entry));
    return descriptors;
}

QString SettingsNavigationModel::titleForId(const QString &id) const
{
    const auto *entry = entryForId(id);
    return entry ? entry->label : QString();
}

QUrl SettingsNavigationModel::pageSourceForId(const QString &id) const
{
    const auto *entry = entryForId(id);
    return entry ? entry->pageSource : QUrl();
}

bool SettingsNavigationModel::containsNavigableId(const QString &id) const
{
    const auto *entry = entryForId(id);
    return entry && isNavigable(*entry);
}

bool SettingsNavigationModel::isNavigable(const SettingsNavigationEntry &entry) const
{
    if (!entry.enabled)
        return false;
    if (entry.kind == SettingsNavigationEntry::Kind::Page)
        return !entry.pageSource.isEmpty();
    if (entry.kind == SettingsNavigationEntry::Kind::Hub)
        return !entry.pageSource.isEmpty() && !childrenForId(entry.id).isEmpty();
    return false;
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

QVariantMap SettingsNavigationModel::descriptorForEntry(const SettingsNavigationEntry &entry) const
{
    return {
        {QStringLiteral("entryId"), entry.id},
        {QStringLiteral("label"), entry.label},
        {QStringLiteral("labelKey"), entry.labelKey},
        {QStringLiteral("subtitle"), entry.subtitle},
        {QStringLiteral("subtitleKey"), entry.subtitleKey},
        {QStringLiteral("kind"), kindName(entry.kind)},
        {QStringLiteral("sym"), entry.sym},
        {QStringLiteral("iconSource"), entry.iconSource},
        {QStringLiteral("iconKey"), entry.iconKey},
        {QStringLiteral("pageSource"), entry.pageSource},
        {QStringLiteral("entryEnabled"), isNavigable(entry)},
        {QStringLiteral("sidebarVisible"), entry.sidebarVisible},
        {QStringLiteral("parentId"), entry.parentId},
    };
}
