#include "statusnotifier/StatusNotifierService.hpp"

#include "statusnotifier/DBusMenuModel.hpp"
#include "statusnotifier/StatusNotifierIconStore.hpp"
#include "statusnotifier/StatusNotifierItemModel.hpp"
#include "statusnotifier/StatusNotifierItemProxy.hpp"
#include "statusnotifier/StatusNotifierWatcherBridge.hpp"

#include <QJsonValue>

namespace Astrea::StatusNotifier {
namespace {

QString watcherModeName(WatcherMode mode)
{
    switch (mode) {
    case WatcherMode::Unavailable: return QStringLiteral("Unavailable");
    case WatcherMode::External: return QStringLiteral("External");
    case WatcherMode::Owned: return QStringLiteral("Owned");
    }
    return QStringLiteral("Unavailable");
}

} // namespace

StatusNotifierService::StatusNotifierService(QObject *parent)
    : QObject(parent)
    , m_watcher(std::make_unique<StatusNotifierWatcherBridge>())
    , m_iconStore(std::make_unique<StatusNotifierIconStore>())
    , m_model(std::make_unique<StatusNotifierItemModel>())
{
    connect(m_watcher.get(), &StatusNotifierWatcherBridge::itemRegistered,
            this, &StatusNotifierService::onItemRegistered);
    connect(m_watcher.get(), &StatusNotifierWatcherBridge::itemUnregistered,
            this, &StatusNotifierService::onItemUnregistered);
    connect(m_watcher.get(), &StatusNotifierWatcherBridge::itemOwnerVanished,
            this, &StatusNotifierService::onItemOwnerVanished);
    connect(m_watcher.get(), &StatusNotifierWatcherBridge::stateChanged,
            this, &StatusNotifierService::onWatcherStateChanged);
    connect(m_watcher.get(), &StatusNotifierWatcherBridge::healthWarning,
            this, &StatusNotifierService::healthWarning);
    connect(m_model.get(), &StatusNotifierItemModel::itemRemoved, this,
            &StatusNotifierService::itemRemoved);
    connect(m_iconStore.get(), &StatusNotifierIconStore::itemIconChanged, this,
            [this](const QString &key, quint64) {
        if (!m_model->contains(key))
            return;
        const ItemSnapshot snapshot = m_model->item(key);
        m_model->upsert(snapshot, m_iconStore->hasIcon(key) ? m_iconStore->imageSource(key)
                                                            : QString());
        emit itemChanged(key);
    });
}

StatusNotifierService::~StatusNotifierService()
{
    stop();
}

void StatusNotifierService::initialize()
{
    if (m_initialized)
        return;
    m_initialized = true;
}

void StatusNotifierService::start()
{
    if (m_started)
        return;
    initialize();
    m_started = true;
    m_watcher->start();
    emit stateChanged();
}

void StatusNotifierService::stop()
{
    if (!m_started && !m_initialized)
        return;
    m_started = false;
    for (auto *proxy : std::as_const(m_proxies)) {
        if (proxy) {
            proxy->stop();
            proxy->deleteLater();
        }
    }
    m_proxies.clear();
    for (auto *menu : std::as_const(m_menus)) {
        if (menu)
            menu->stop();
        if (menu)
            menu->deleteLater();
    }
    m_menus.clear();
    m_model->clear();
    m_iconStore->clear();
    m_watcher->stop();
    emit stateChanged();
}

QAbstractItemModel *StatusNotifierService::itemModel() const
{
    return m_model.get();
}

WatcherMode StatusNotifierService::watcherMode() const
{
    return m_watcher->mode();
}

QString StatusNotifierService::watcherOwner() const
{
    return m_watcher->watcherOwner();
}

QString StatusNotifierService::hostServiceName() const
{
    return m_watcher->hostServiceName();
}

bool StatusNotifierService::hostRegistered() const
{
    return m_watcher->hostRegistered();
}

int StatusNotifierService::itemCount() const
{
    return m_model->rowCount();
}

QString StatusNotifierService::lastError() const
{
    return m_watcher->lastError();
}

int StatusNotifierService::menuClientCount() const
{
    return m_menus.size();
}

QJsonObject StatusNotifierService::healthJson() const
{
    return {
        {QStringLiteral("mode"), watcherModeName(watcherMode())},
        {QStringLiteral("watcherOwner"), watcherOwner()},
        {QStringLiteral("hostRegistered"), hostRegistered()},
        {QStringLiteral("itemCount"), itemCount()},
        {QStringLiteral("menuClientCount"), menuClientCount()},
        {QStringLiteral("lastError"), lastError()},
    };
}

bool StatusNotifierService::hasMenuForItem(const QString &itemKey) const
{
    const ItemSnapshot snapshot = m_model->item(itemKey);
    return !snapshot.menuPath.isEmpty();
}

QObject *StatusNotifierService::menuModelForItem(const QString &itemKey) const
{
    const auto *menu = m_menus.value(itemKey);
    return menu ? menu->rootModel() : nullptr;
}

int StatusNotifierService::menuStateForItem(const QString &itemKey) const
{
    const auto *menu = m_menus.value(itemKey);
    return menu ? static_cast<int>(menu->state())
                : static_cast<int>(DBusMenuLifecycleState::Unavailable);
}

QString StatusNotifierService::tooltipTitleForItem(const QString &itemKey) const
{
    const ItemSnapshot snapshot = m_model->item(itemKey);
    return snapshot.tooltipTitle.isEmpty() ? snapshot.title : snapshot.tooltipTitle;
}

QString StatusNotifierService::tooltipDescriptionForItem(const QString &itemKey) const
{
    return m_model->item(itemKey).tooltipDescription;
}

void StatusNotifierService::activate(const QString &itemKey, int x, int y)
{
    if (auto *proxy = m_proxies.value(itemKey))
        proxy->activate(x, y);
}

void StatusNotifierService::secondaryActivate(const QString &itemKey, int x, int y)
{
    if (auto *proxy = m_proxies.value(itemKey))
        proxy->secondaryActivate(x, y);
}

void StatusNotifierService::contextMenu(const QString &itemKey, int x, int y)
{
    if (auto *proxy = m_proxies.value(itemKey))
        proxy->contextMenu(x, y);
}

void StatusNotifierService::scroll(const QString &itemKey, int delta, const QString &orientation)
{
    if (auto *proxy = m_proxies.value(itemKey))
        proxy->scroll(delta, orientation);
}

void StatusNotifierService::openMenu(const QString &itemKey)
{
    prepareMenuForPresentation(itemKey);
}

void StatusNotifierService::prepareMenuForPresentation(const QString &itemKey, int nodeId)
{
    if (auto *menu = m_menus.value(itemKey))
        menu->prepareForPresentation(nodeId);
}

void StatusNotifierService::aboutToShowMenu(const QString &itemKey, int nodeId)
{
    prepareMenuForPresentation(itemKey, nodeId);
}

void StatusNotifierService::closeMenu(const QString &itemKey)
{
    if (auto *menu = m_menus.value(itemKey))
        menu->stop();
    emit menuClosed(itemKey);
}

void StatusNotifierService::upsertTestItem(const ItemSnapshot &snapshot)
{
    if (!snapshot.address.isValid())
        return;
    ItemSnapshot copy = snapshot;
    copy.ready = true;
    copy.generation = copy.generation == 0 ? m_nextGeneration++ : copy.generation;
    updateSnapshot(copy);
    updateMenu(copy.address.key(), copy.menuPath);
}

void StatusNotifierService::removeTestItem(const QString &key)
{
    removeItem(key);
}

void StatusNotifierService::onItemRegistered(const ItemAddress &address)
{
    ensureProxy(address);
}

void StatusNotifierService::onItemUnregistered(const QString &key)
{
    removeItem(key);
}

void StatusNotifierService::onItemOwnerVanished(const QString &key, const QString &)
{
    removeItem(key);
    emit menuClosed(key);
}

void StatusNotifierService::onSnapshotChanged(const ItemSnapshot &snapshot)
{
    updateSnapshot(snapshot);
}

void StatusNotifierService::onProxyMenuPathChanged(const QString &key, const QString &menuPath)
{
    updateMenu(key, menuPath);
}

void StatusNotifierService::onWatcherStateChanged()
{
    emit stateChanged();
}

void StatusNotifierService::ensureProxy(const ItemAddress &address)
{
    if (!address.isValid())
        return;
    const QString key = address.key();
    removeItem(key);
    auto *proxy = new StatusNotifierItemProxy(address, m_nextGeneration++, this);
    m_proxies.insert(key, proxy);
    connect(proxy, &StatusNotifierItemProxy::snapshotChanged, this,
            &StatusNotifierService::onSnapshotChanged);
    connect(proxy, &StatusNotifierItemProxy::menuPathChanged, this,
            &StatusNotifierService::onProxyMenuPathChanged);
    connect(proxy, &StatusNotifierItemProxy::vanished, this,
            [this](const QString &vanishedKey, const QString &) { removeItem(vanishedKey); });
    connect(proxy, &StatusNotifierItemProxy::actionFailed, this,
            [this](const QString &, const QString &error) { emit healthWarning(error); });
    proxy->start();
}

void StatusNotifierService::removeItem(const QString &key)
{
    if (auto *proxy = m_proxies.take(key)) {
        proxy->stop();
        proxy->deleteLater();
    }
    if (auto *menu = m_menus.take(key)) {
        menu->stop();
        menu->deleteLater();
    }
    m_iconStore->clearItem(key);
    if (m_model->removeKey(key)) {
        emit stateChanged();
    }
}

void StatusNotifierService::updateSnapshot(const ItemSnapshot &snapshot)
{
    if (!snapshot.address.isValid() || !snapshot.ready)
        return;
    m_iconStore->updateItem(snapshot);
    const QString source = m_iconStore->hasIcon(snapshot.address.key())
        ? m_iconStore->imageSource(snapshot.address.key()) : QString();
    m_model->upsert(snapshot, source);
    emit itemChanged(snapshot.address.key());
    emit stateChanged();
}

void StatusNotifierService::updateMenu(const QString &key, const QString &menuPath)
{
    if (menuPath.isEmpty()) {
        if (auto *menu = m_menus.take(key)) {
            menu->stop();
            menu->deleteLater();
        }
        emit menuClientChanged(key);
        emit stateChanged();
        return;
    }
    const ItemSnapshot snapshot = m_model->item(key);
    if (!snapshot.address.isValid())
        return;
    if (auto *old = m_menus.take(key)) {
        old->stop();
        old->deleteLater();
    }
    auto *menu = new DBusMenuClient(snapshot.address, menuPath, m_iconStore.get(), this);
    m_menus.insert(key, menu);
    connect(menu, &DBusMenuClient::failed, this,
            [this](const QString &error) { emit healthWarning(error); });
    emit stateChanged();
    emit menuClientChanged(key);
}

} // namespace Astrea::StatusNotifier
