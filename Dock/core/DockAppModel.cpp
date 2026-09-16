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
    case TaskKeyRole: return item.taskKey;
    case RuntimeAppIdRole: return item.runtimeAppId;
    case DesktopIdRole: return item.desktopId;
    case DisplayNameRole: return item.displayName;
    case IconNameRole: return item.iconName;
    case IconPathRole: return item.iconPath;
    case IconUrlRole: return item.iconUrl;
    case ResolvedRole: return item.resolved;
    case IdentityPendingRole: return item.identityPending;
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
        {TaskKeyRole, "taskKey"},
        {RuntimeAppIdRole, "runtimeAppId"},
        {DesktopIdRole, "desktopId"},
        {DisplayNameRole, "displayName"},
        {IconNameRole, "iconName"},
        {IconPathRole, "iconPath"},
        {IconUrlRole, "iconUrl"},
        {ResolvedRole, "resolved"},
        {IdentityPendingRole, "identityPending"},
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
        if (!pin.isEmpty() && !desired.contains(pin))
            desired.append(pin);
    }
    m_pins = std::move(desired);

    for (int index = m_dynamicOrder.size() - 1; index >= 0; --index) {
        if (isPinnedTaskKey(m_dynamicOrder.at(index)))
            m_dynamicOrder.removeAt(index);
    }
    if (m_runtimeAuthoritative) {
        for (const DockAppInfo &item : std::as_const(m_items)) {
            const auto it = m_runtimeStates.constFind(item.taskKey);
            if (it != m_runtimeStates.constEnd() && it->running
                && !isPinnedTaskKey(item.taskKey)
                && !m_dynamicOrder.contains(item.taskKey)) {
                m_dynamicOrder.append(item.taskKey);
            }
        }
    }
    reconcileRows();
}

void DockAppModel::setCatalogSnapshot(std::shared_ptr<const DesktopEntrySnapshot> snapshot)
{
    if (!snapshot)
        snapshot = std::make_shared<const DesktopEntrySnapshot>();
    m_catalog = std::move(snapshot);
    for (int row = 0; row < m_items.size(); ++row) {
        const DockAppInfo next = makeItem(m_items.at(row).taskKey, &m_items.at(row));
        updateItem(row, next);
    }
}

QString DockAppModel::desktopFileNameAt(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items.at(row).desktopFileName : QString();
}

QString DockAppModel::taskKeyAt(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items.at(row).taskKey : QString();
}

int DockAppModel::rowForDesktopFileName(const QString &desktopFileName) const
{
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items.at(row).desktopFileName == desktopFileName)
            return row;
    }
    return -1;
}

int DockAppModel::rowForTaskKey(const QString &taskKey) const
{
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items.at(row).taskKey == taskKey)
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

void DockAppModel::applyRuntimeProjection(
    const Astrea::Typhon::DockApplicationRuntimeProjection &projection,
    bool authoritative)
{
    if (!authoritative) {
        clearRuntimeProjection();
        return;
    }

    m_runtimeAuthoritative = true;
    m_runtimeStates = projection.states;
    for (const QString &key : projection.encounterOrder) {
        const auto it = m_runtimeStates.constFind(key);
        if (it != m_runtimeStates.constEnd() && it->running
            && !isPinnedTaskKey(key) && !m_dynamicOrder.contains(key)) {
            m_dynamicOrder.append(key);
        }
    }
    for (int index = m_dynamicOrder.size() - 1; index >= 0; --index) {
        const QString &key = m_dynamicOrder.at(index);
        const auto it = m_runtimeStates.constFind(key);
        if (isPinnedTaskKey(key) || it == m_runtimeStates.constEnd() || !it->running)
            m_dynamicOrder.removeAt(index);
    }
    reconcileRows();
}

void DockAppModel::clearRuntimeProjection()
{
    m_runtimeStates.clear();
    m_identityEnrichments.clear();
    m_dynamicOrder.clear();
    m_runtimeAuthoritative = false;
    reconcileRows();
}

void DockAppModel::reconcileRows()
{
    QStringList desired;
    desired.reserve(m_pins.size() + m_dynamicOrder.size());
    for (const QString &pin : std::as_const(m_pins))
        desired.append(desktopTaskKey(pin));
    for (const QString &key : std::as_const(m_dynamicOrder)) {
        const auto it = m_runtimeStates.constFind(key);
        if (it != m_runtimeStates.constEnd() && it->running && !desired.contains(key))
            desired.append(key);
    }

    for (int row = m_items.size() - 1; row >= 0; --row) {
        if (desired.contains(m_items.at(row).taskKey))
            continue;
        beginRemoveRows({}, row, row);
        m_items.removeAt(row);
        endRemoveRows();
    }

    for (int row = 0; row < desired.size(); ++row) {
        const QString &key = desired.at(row);
        const int existing = rowForTaskKey(key);
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
        const DockAppInfo next = makeItem(m_items.at(row).taskKey, &m_items.at(row));
        updateItem(row, next);
    }
}

DockAppInfo DockAppModel::makeItem(const QString &taskKey, const DockAppInfo *previous) const
{
    DockAppInfo item;
    item.taskKey = taskKey;
    const auto runtime = m_runtimeStates.constFind(taskKey);
    if (runtime != m_runtimeStates.constEnd()) {
        item.runtimeAppId = runtime->appId;
        item.desktopFileName = runtime->desktopFileName;
        item.desktopId = runtime->desktopId;
        item.displayName = runtime->displayName;
        item.iconName = runtime->iconName;
        item.iconPath = runtime->iconPath;
        item.identityPending = runtime->identityPending;
    } else {
        item.desktopFileName = desktopFileNameForTaskKey(taskKey);
        item.desktopId = item.desktopFileName.endsWith(QStringLiteral(".desktop"))
            ? item.desktopFileName.chopped(QStringLiteral(".desktop").size()) : item.desktopFileName;
        item.displayName = item.desktopId;
    }
    if (item.desktopFileName.isEmpty() && item.displayName.isEmpty())
        item.displayName = item.runtimeAppId.isEmpty() ? QStringLiteral("Application") : item.runtimeAppId;
    if (previous) {
        item.launching = previous->launching;
        item.launchError = previous->launchError;
    }

    const auto it = m_catalog->byDesktopFileName.constFind(item.desktopFileName);
    if (!item.desktopFileName.isEmpty() && it != m_catalog->byDesktopFileName.constEnd()) {
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
    }
    item.pinned = !item.desktopFileName.isEmpty() && m_pins.contains(item.desktopFileName);
    if (m_runtimeAuthoritative) {
        item.runtimeKnown = item.resolved || runtime != m_runtimeStates.constEnd();
        if (runtime != m_runtimeStates.constEnd()) {
            item.runtimeAppId = runtime->appId;
            item.running = runtime->running;
            item.active = runtime->active;
            item.windowCount = runtime->windowCount;
            item.identityPending = runtime->identityPending;
        }
    }
    const auto enrichment = m_identityEnrichments.constFind(taskKey);
    if (enrichment != m_identityEnrichments.constEnd()) {
        if (!enrichment->displayName.isEmpty())
            item.displayName = enrichment->displayName;
        if (!enrichment->iconName.isEmpty())
            item.iconName = enrichment->iconName;
        if (!enrichment->iconPath.isEmpty())
            item.iconPath = enrichment->iconPath;
        item.identityPending = enrichment->iconPending;
    }
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
    if (before.taskKey != after.taskKey) roles.append(TaskKeyRole);
    if (before.runtimeAppId != after.runtimeAppId) roles.append(RuntimeAppIdRole);
    if (before.desktopFileName != after.desktopFileName) roles.append(DesktopFileNameRole);
    if (before.desktopId != after.desktopId) roles.append(DesktopIdRole);
    if (before.displayName != after.displayName) roles.append(DisplayNameRole);
    if (before.iconName != after.iconName) roles.append(IconNameRole);
    if (before.iconPath != after.iconPath) roles.append(IconPathRole);
    if (before.iconUrl != after.iconUrl) roles.append(IconUrlRole);
    if (before.resolved != after.resolved) roles.append(ResolvedRole);
    if (before.identityPending != after.identityPending) roles.append(IdentityPendingRole);
    if (before.launching != after.launching) roles.append(LaunchingRole);
    if (before.launchError != after.launchError) roles.append(LaunchErrorRole);
    if (before.pinned != after.pinned) roles.append(PinnedRole);
    if (before.runtimeKnown != after.runtimeKnown) roles.append(RuntimeKnownRole);
    if (before.running != after.running) roles.append(RunningRole);
    if (before.active != after.active) roles.append(ActiveRole);
    if (before.windowCount != after.windowCount) roles.append(WindowCountRole);
    return roles;
}

QString DockAppModel::desktopTaskKey(const QString &desktopFileName)
{
    return QStringLiteral("desktop:") + desktopFileName;
}

QString DockAppModel::desktopFileNameForTaskKey(const QString &taskKey)
{
    const QString prefix = QStringLiteral("desktop:");
    if (!taskKey.startsWith(prefix))
        return {};
    return taskKey.mid(prefix.size());
}

bool DockAppModel::isPinnedTaskKey(const QString &taskKey) const
{
    const QString desktopFileName = desktopFileNameForTaskKey(taskKey);
    return !desktopFileName.isEmpty() && m_pins.contains(desktopFileName);
}

bool DockAppModel::applyIdentityEnrichment(const QString &taskKey, const AppIdentity &identity)
{
    const int row = rowForTaskKey(taskKey);
    if (row < 0)
        return false;

    m_identityEnrichments.insert(taskKey, identity);
    const DockAppInfo next = makeItem(taskKey, &m_items.at(row));
    updateItem(row, next);
    return true;
}
