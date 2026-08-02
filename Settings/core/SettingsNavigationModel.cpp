#include "core/SettingsNavigationModel.hpp"

#include <QVariant>

namespace {
QString kindName(SettingsNavigationEntry::Kind kind)
{
    switch (kind) {
    case SettingsNavigationEntry::Kind::Item:
        return QStringLiteral("item");
    case SettingsNavigationEntry::Kind::Spacer:
        return QStringLiteral("spacer");
    }
    return QStringLiteral("item");
}
}

SettingsNavigationModel::SettingsNavigationModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_entries{
          {QStringLiteral("system"), QStringLiteral("System"),
           QStringLiteral("Device overview and core settings"), QStringLiteral("computer")},
          {QStringLiteral("software-update"), QStringLiteral("Software Update"),
           QStringLiteral("Updates and release information"), QStringLiteral("software-update-available")},
          {QStringLiteral("internet"), QStringLiteral("Internet"),
           QStringLiteral("Network connections and DNS"), QStringLiteral("network-wireless")},
          {QStringLiteral("bluetooth"), QStringLiteral("Bluetooth"),
           QStringLiteral("Devices and connection preferences"), QStringLiteral("bluetooth")},
          {QStringLiteral("audio"), QStringLiteral("Audio"),
           QStringLiteral("Output, input, and volume"), QStringLiteral("audio-volume-high")},
          {QString(), QString(), QString(), QString(), SettingsNavigationEntry::Kind::Spacer, false},
          {QStringLiteral("performance"), QStringLiteral("Performance"),
           QStringLiteral("Power and latency controls"), QStringLiteral("utilities-system-monitor")},
          {QStringLiteral("appearance"), QStringLiteral("Appearance"),
           QStringLiteral("Theme and desktop presentation"), QStringLiteral("preferences-desktop-theme")},
          {QStringLiteral("more-settings"), QStringLiteral("More Settings"),
           QStringLiteral("Applications, services, and compatibility"), QStringLiteral("preferences-system")},
      }
{
    rebuildVisibleRows();
    m_selectedId = QStringLiteral("system");
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
        return entry.title;
    case SubtitleRole:
        return entry.subtitle;
    case IconNameRole:
        return entry.iconName;
    case KindRole:
        return kindName(entry.kind);
    case EnabledRole:
        return entry.enabled;
    case SelectedRole:
        return entry.kind == SettingsNavigationEntry::Kind::Item
            && entry.id == m_selectedId;
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
    if (!entry.enabled || entry.kind != SettingsNavigationEntry::Kind::Item)
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

QString SettingsNavigationModel::titleForId(const QString &id) const
{
    const int sourceIndex = sourceIndexForId(id);
    return sourceIndex >= 0 ? m_entries.at(sourceIndex).title : QString();
}

bool SettingsNavigationModel::containsSelectableId(const QString &id) const
{
    const int sourceIndex = sourceIndexForId(id);
    if (sourceIndex < 0)
        return false;
    const SettingsNavigationEntry &entry = m_entries.at(sourceIndex);
    return entry.kind == SettingsNavigationEntry::Kind::Item && entry.enabled;
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
            || entry.title.contains(m_filterText, Qt::CaseInsensitive)
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
