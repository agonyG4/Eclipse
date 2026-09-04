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

bool pixmapListsEqual(const QList<PixmapData> &left, const QList<PixmapData> &right)
{
    if (left.size() != right.size())
        return false;
    for (qsizetype index = 0; index < left.size(); ++index) {
        const PixmapData &leftPixmap = left.at(index);
        const PixmapData &rightPixmap = right.at(index);
        if (leftPixmap.width != rightPixmap.width
            || leftPixmap.height != rightPixmap.height
            || leftPixmap.argb32Network != rightPixmap.argb32Network)
            return false;
    }
    return true;
}

bool iconInputsEqual(const ItemSnapshot &left, const ItemSnapshot &right)
{
    return left.status == right.status
        && left.iconName == right.iconName
        && pixmapListsEqual(left.pixmaps, right.pixmaps)
        && left.attentionIconName == right.attentionIconName
        && pixmapListsEqual(left.attentionPixmaps, right.attentionPixmaps)
        && left.overlayIconName == right.overlayIconName
        && pixmapListsEqual(left.overlayPixmaps, right.overlayPixmaps)
        && left.iconThemePath == right.iconThemePath;
}

} // namespace

StatusNotifierService::ProjectionMutationGuard::ProjectionMutationGuard(
    StatusNotifierService *service)
    : m_service(service)
{
    ++m_service->m_projectionMutationDepth;
}

StatusNotifierService::ProjectionMutationGuard::~ProjectionMutationGuard()
{
    --m_service->m_projectionMutationDepth;
}

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
            this, [this](const QString &warning) {
        m_lastHealthWarning = warning;
        emit healthWarning(warning);
        emit stateChanged();
    });
    connect(m_model.get(), &StatusNotifierItemModel::itemRemoved, this,
            &StatusNotifierService::itemRemoved);
    connect(m_iconStore.get(), &StatusNotifierIconStore::itemIconChanged, this,
            [this](const QString &key, quint64) {
        if (m_stopping || m_projectionMutationDepth > 0 || !m_model->contains(key))
            return;
        const ItemSnapshot snapshot = m_model->item(key);
        m_model->upsert(snapshot, m_iconStore->hasIcon(key) ? m_iconStore->imageSource(key)
                                                            : QString());
        bumpPresentationRevision();
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
    m_stopping = true;
    m_started = false;
    {
        const ProjectionMutationGuard guard(this);
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
        m_iconStore->clear();
        m_model->clear();
        m_watcher->stop();
    }
    m_stopping = false;
    bumpPresentationRevision();
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

QString StatusNotifierService::watcherName() const
{
    return m_watcher->watcherName();
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
        {QStringLiteral("healthWarning"), m_lastHealthWarning},
    };
}

bool StatusNotifierService::hasMenuForItem(const QString &itemKey) const
{
    const ItemSnapshot snapshot = m_model->item(itemKey);
    return !snapshot.menuPath.isEmpty();
}

bool StatusNotifierService::hasUsableMenuForItem(const QString &itemKey) const
{
    const auto *menu = m_menus.value(itemKey);
    if (!menu)
        return false;
    return menu->state() != DBusMenuLifecycleState::Unavailable
        && menu->state() != DBusMenuLifecycleState::Error
        && menu->state() != DBusMenuLifecycleState::Stopped;
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

QString StatusNotifierService::displayTitleForItem(const QString &itemKey) const
{
    const ItemSnapshot snapshot = m_model->item(itemKey);
    if (!snapshot.tooltipTitle.isEmpty())
        return snapshot.tooltipTitle;
    if (!snapshot.title.isEmpty())
        return snapshot.title;
    if (!snapshot.id.isEmpty())
        return snapshot.id;
    return QStringLiteral("Tray item");
}

QString StatusNotifierService::iconSourceForItem(const QString &itemKey) const
{
    return m_iconStore->hasIcon(itemKey) ? m_iconStore->imageSource(itemKey) : QString();
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

void StatusNotifierService::upsertTestItem(const ItemSnapshot &snapshot)
{
    if (!snapshot.address.isValid())
        return;
    ItemSnapshot copy = snapshot;
    copy.ready = true;
    copy.generation = copy.generation == 0 ? m_nextGeneration++ : copy.generation;
    applySnapshotProjection(copy);
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
}

void StatusNotifierService::onSnapshotChanged(const ItemSnapshot &snapshot)
{
    applySnapshotProjection(snapshot);
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
    connect(proxy, &StatusNotifierItemProxy::vanished, this,
            [this](const QString &vanishedKey, const QString &) { removeItem(vanishedKey); });
    connect(proxy, &StatusNotifierItemProxy::actionFailed, this,
            [this](const QString &, const QString &error) { emit healthWarning(error); });
    proxy->start();
}

void StatusNotifierService::removeItem(const QString &key)
{
    const bool hadState = m_proxies.contains(key) || m_menus.contains(key)
        || m_model->contains(key) || m_iconStore->hasIcon(key);
    if (!hadState)
        return;
    bool modelRemoved = false;
    {
        const ProjectionMutationGuard guard(this);
        if (auto *proxy = m_proxies.take(key)) {
            proxy->stop();
            proxy->deleteLater();
        }
        if (auto *menu = m_menus.take(key)) {
            menu->stop();
            menu->deleteLater();
        }
        m_iconStore->clearItem(key);
        modelRemoved = m_model->removeKey(key);
    }
    bumpPresentationRevision();
    if (modelRemoved)
        emit stateChanged();
}

void StatusNotifierService::applySnapshotProjection(const ItemSnapshot &snapshot)
{
    if (!snapshot.address.isValid() || !snapshot.ready)
        return;

    const QString key = snapshot.address.key();
    const bool hasCommittedSnapshot = m_model->contains(key);
    const ItemSnapshot previous = hasCommittedSnapshot ? m_model->item(key) : ItemSnapshot{};
    const bool iconChanged = !hasCommittedSnapshot || !iconInputsEqual(previous, snapshot);
    MenuIdentityChange menuChange = MenuIdentityChange::Unchanged;
    {
        const ProjectionMutationGuard guard(this);
        if (iconChanged)
            m_iconStore->updateItem(snapshot);
        const QString source = m_iconStore->hasIcon(key) ? m_iconStore->imageSource(key)
                                                          : QString();
        menuChange = reconcileMenu(snapshot);
        m_model->upsert(snapshot, source);
    }

    // A presentation revision is the commit marker for a coherent StatusNotifier projection.
    bumpPresentationRevision();
    if (menuChange == MenuIdentityChange::Changed)
        emit menuClientChanged(key);
    emit itemChanged(key);
    emit stateChanged();
}

StatusNotifierService::MenuIdentityChange
StatusNotifierService::reconcileMenu(const ItemSnapshot &snapshot)
{
    const QString key = snapshot.address.key();
    if (!snapshot.address.isValid())
        return MenuIdentityChange::Unchanged;
    if (auto *existing = m_menus.value(key)) {
        if (existing->menuPath() == snapshot.menuPath
            && existing->itemGeneration() == snapshot.generation)
            return MenuIdentityChange::Unchanged;
    }
    if (snapshot.menuPath.isEmpty()) {
        if (auto *menu = m_menus.take(key)) {
            menu->stop();
            menu->deleteLater();
            return MenuIdentityChange::Changed;
        }
        return MenuIdentityChange::Unchanged;
    }
    if (auto *old = m_menus.take(key)) {
        old->stop();
        old->deleteLater();
    }
    auto *menu = new DBusMenuClient(snapshot.address, snapshot.menuPath, m_iconStore.get(),
                                    snapshot.generation, this);
    m_menus.insert(key, menu);
    connect(menu, &DBusMenuClient::changed, this,
            [this, key, menu] {
                if (m_stopping || m_projectionMutationDepth > 0 || m_menus.value(key) != menu)
                    return;
                bumpPresentationRevision();
                emit menuContentChanged(key);
            });
    connect(menu, &DBusMenuClient::failed, this,
            [this](const QString &error) { emit healthWarning(error); });
    return MenuIdentityChange::Changed;
}

void StatusNotifierService::bumpPresentationRevision()
{
    ++m_presentationRevision;
    emit presentationRevisionChanged();
}

} // namespace Astrea::StatusNotifier
