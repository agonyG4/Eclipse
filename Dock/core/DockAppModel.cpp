#include "core/DockAppModel.hpp"

#include <QUrl>

DockAppModel::DockAppModel(QObject *parent)
    : QAbstractListModel(parent), m_catalog(std::make_shared<const DesktopEntrySnapshot>())
{
}

int DockAppModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QVariant DockAppModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const DockAppInfo &item = m_items.at(index.row());
    switch (role) {
    case DesktopFileNameRole: return item.desktopFileName;
    case DesktopIdRole: return item.desktopId;
    case DisplayNameRole: return item.displayName;
    case IconNameRole: return item.iconName;
    case IconPathRole: return item.iconPath;
    case IconUrlRole: return item.iconUrl;
    case ResolvedRole: return item.resolved;
    case LaunchingRole: return item.launching;
    case LaunchErrorRole: return item.launchError;
    case PinnedRole: return item.pinned;
    case RuntimeKnownRole: return item.runtimeKnown;
    case RunningRole: return item.running;
    case ActiveRole: return item.active;
    case WindowCountRole: return item.windowCount;
    default: return {};
    }
}

QHash<int, QByteArray> DockAppModel::roleNames() const
{
    return {
        {DesktopFileNameRole, "desktopFileName"},
        {DesktopIdRole, "desktopId"},
        {DisplayNameRole, "displayName"},
        {IconNameRole, "iconName"},
        {IconPathRole, "iconPath"},
        {IconUrlRole, "iconUrl"},
        {ResolvedRole, "resolved"},
        {LaunchingRole, "launching"},
        {LaunchErrorRole, "launchError"},
        {PinnedRole, "pinned"},
        {RuntimeKnownRole, "runtimeKnown"},
        {RunningRole, "running"},
        {ActiveRole, "active"},
        {WindowCountRole, "windowCount"},
    };
}

void DockAppModel::setPins(const QStringList &pins)
{
    QStringList desired;
    for (const QString &pin : pins) {
        if (!desired.contains(pin))
            desired.append(pin);
    }

    for (int row = static_cast<int>(m_items.size()) - 1; row >= 0; --row) {
        if (!desired.contains(m_items.at(row).desktopFileName)) {
            beginRemoveRows({}, row, row);
            m_items.removeAt(row);
            endRemoveRows();
        }
    }

    for (int row = 0; row < desired.size(); ++row) {
        const QString &key = desired.at(row);
        const int existing = rowForDesktopFileName(key);
        if (existing == row)
            continue;
        if (existing >= 0) {
            const int destination = existing < row ? row + 1 : row;
            beginMoveRows({}, existing, existing, {}, destination);
            m_items.move(existing, row);
            endMoveRows();
        } else {
            beginInsertRows({}, row, row);
            m_items.insert(row, makeItem(key));
            endInsertRows();
        }
    }

    for (int row = 0; row < m_items.size(); ++row) {
        const DockAppInfo next = makeItem(m_items.at(row).desktopFileName, &m_items.at(row));
        updateItem(row, next);
    }
}

void DockAppModel::setCatalogSnapshot(std::shared_ptr<const DesktopEntrySnapshot> snapshot)
{
    if (!snapshot)
        snapshot = std::make_shared<const DesktopEntrySnapshot>();
    m_catalog = std::move(snapshot);
    for (int row = 0; row < m_items.size(); ++row) {
        const DockAppInfo next = makeItem(m_items.at(row).desktopFileName, &m_items.at(row));
        updateItem(row, next);
    }
}

QString DockAppModel::desktopFileNameAt(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items.at(row).desktopFileName : QString();
}

int DockAppModel::rowForDesktopFileName(const QString &desktopFileName) const
{
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items.at(row).desktopFileName == desktopFileName)
            return row;
    }
    return -1;
}

const DockAppInfo *DockAppModel::itemAt(int row) const
{
    return row >= 0 && row < m_items.size() ? &m_items.at(row) : nullptr;
}

bool DockAppModel::setLaunching(const QString &desktopFileName, bool launching)
{
    const int row = rowForDesktopFileName(desktopFileName);
    if (row < 0 || m_items[row].launching == launching)
        return false;
    DockAppInfo next = m_items.at(row);
    next.launching = launching;
    updateItem(row, next);
    return true;
}

bool DockAppModel::setLaunchError(const QString &desktopFileName, const QString &error)
{
    const int row = rowForDesktopFileName(desktopFileName);
    if (row < 0 || m_items[row].launchError == error)
        return false;
    DockAppInfo next = m_items.at(row);
    next.launchError = error;
    updateItem(row, next);
    return true;
}

void DockAppModel::applyRuntimeStates(
    const QHash<QString, Astrea::Typhon::DockApplicationRuntimeState> &states,
    bool authoritative)
{
    for (int row = 0; row < m_items.size(); ++row) {
        DockAppInfo next = m_items.at(row);
        if (!authoritative) {
            next.runtimeKnown = false;
            next.running = false;
            next.active = false;
            next.windowCount = 0;
            updateItem(row, next);
            continue;
        }

        next.runtimeKnown = next.resolved;
        const auto it = states.constFind(next.desktopFileName);
        if (!next.runtimeKnown || it == states.constEnd()) {
            next.running = false;
            next.active = false;
            next.windowCount = 0;
        } else {
            next.running = it->running;
            next.active = it->active;
            next.windowCount = it->windowCount;
        }
        updateItem(row, next);
    }
}

DockAppInfo DockAppModel::makeItem(const QString &desktopFileName, const DockAppInfo *previous) const
{
    DockAppInfo item;
    item.desktopFileName = desktopFileName;
    item.desktopId = desktopFileName.endsWith(QStringLiteral(".desktop"))
        ? desktopFileName.chopped(QStringLiteral(".desktop").size()) : desktopFileName;
    item.displayName = item.desktopId;
    if (previous) {
        item.launching = previous->launching;
        item.launchError = previous->launchError;
        item.runtimeKnown = previous->runtimeKnown;
        item.running = previous->running;
        item.active = previous->active;
        item.windowCount = previous->windowCount;
    }

    const auto it = m_catalog->byDesktopFileName.constFind(desktopFileName);
    if (it == m_catalog->byDesktopFileName.constEnd())
        return item;

    const DesktopEntryRecord &record = m_catalog->entries.at(it.value());
    item.desktopId = record.id;
    item.displayName = record.name.isEmpty() ? record.id : record.name;
    if (record.icon.startsWith(QLatin1Char('/')) || record.icon.startsWith(QStringLiteral("file://"))) {
        item.iconPath = record.icon;
        item.iconUrl = record.icon.startsWith(QStringLiteral("file://"))
            ? record.icon : QUrl::fromLocalFile(record.icon).toString();
    } else if (record.icon.contains(QStringLiteral("://"))) {
        item.iconUrl = record.icon;
    } else {
        item.iconName = record.icon;
    }
    item.resolved = true;
    return item;
}

void DockAppModel::updateItem(int row, const DockAppInfo &next)
{
    const QList<int> roles = changedRoles(m_items.at(row), next);
    if (roles.isEmpty())
        return;
    m_items[row] = next;
    const QModelIndex modelIndex = index(row, 0);
    emit dataChanged(modelIndex, modelIndex, roles);
}

QList<int> DockAppModel::changedRoles(const DockAppInfo &before, const DockAppInfo &after)
{
    QList<int> roles;
    if (before.desktopFileName != after.desktopFileName) roles.append(DesktopFileNameRole);
    if (before.desktopId != after.desktopId) roles.append(DesktopIdRole);
    if (before.displayName != after.displayName) roles.append(DisplayNameRole);
    if (before.iconName != after.iconName) roles.append(IconNameRole);
    if (before.iconPath != after.iconPath) roles.append(IconPathRole);
    if (before.iconUrl != after.iconUrl) roles.append(IconUrlRole);
    if (before.resolved != after.resolved) roles.append(ResolvedRole);
    if (before.launching != after.launching) roles.append(LaunchingRole);
    if (before.launchError != after.launchError) roles.append(LaunchErrorRole);
    if (before.pinned != after.pinned) roles.append(PinnedRole);
    if (before.runtimeKnown != after.runtimeKnown) roles.append(RuntimeKnownRole);
    if (before.running != after.running) roles.append(RunningRole);
    if (before.active != after.active) roles.append(ActiveRole);
    if (before.windowCount != after.windowCount) roles.append(WindowCountRole);
    return roles;
}
