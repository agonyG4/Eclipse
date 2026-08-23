#include "statusnotifier/StatusNotifierWatcherBridge.hpp"

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QTimer>

#include <algorithm>

namespace Astrea::StatusNotifier {
namespace {

constexpr QLatin1StringView kFreedesktopWatcher("org.freedesktop.StatusNotifierWatcher");
constexpr QLatin1StringView kKdeWatcher("org.kde.StatusNotifierWatcher");
constexpr QLatin1StringView kWatcherPath("/StatusNotifierWatcher");
constexpr QLatin1StringView kFreedesktopInterface("org.freedesktop.StatusNotifierWatcher");
constexpr QLatin1StringView kKdeInterface("org.kde.StatusNotifierWatcher");

QString canonicalRegistration(const ItemAddress &address)
{
    return address.service + address.objectPath;
}

QStringList watcherNames()
{
    return {QString::fromLatin1(kFreedesktopWatcher), QString::fromLatin1(kKdeWatcher)};
}

QString watcherInterface(const QString &name)
{
    return name == QString::fromLatin1(kFreedesktopWatcher)
        ? QString::fromLatin1(kFreedesktopInterface)
        : QString::fromLatin1(kKdeInterface);
}

class FreedesktopWatcherAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.StatusNotifierWatcher")
    Q_PROPERTY(QStringList RegisteredStatusNotifierItems READ registeredItems)
    Q_PROPERTY(bool IsStatusNotifierHostRegistered READ hostRegistered)
    Q_PROPERTY(int ProtocolVersion READ protocolVersion)

public:
    explicit FreedesktopWatcherAdaptor(StatusNotifierLocalWatcherObject *object)
        : QDBusAbstractAdaptor(object), m_object(object)
    {
        connect(m_object, &StatusNotifierLocalWatcherObject::itemRegisteredForDbus, this,
                &FreedesktopWatcherAdaptor::StatusNotifierItemRegistered);
        connect(m_object, &StatusNotifierLocalWatcherObject::itemUnregisteredForDbus, this,
                &FreedesktopWatcherAdaptor::StatusNotifierItemUnregistered);
        connect(m_object, &StatusNotifierLocalWatcherObject::hostRegisteredChanged, this,
                [this](const QString &host, bool registered) {
            Q_UNUSED(host)
            if (registered)
                emit StatusNotifierHostRegistered();
        });
    }

    QStringList registeredItems() const { return m_object->items(); }
    bool hostRegistered() const { return m_object->hostRegistered(); }
    int protocolVersion() const { return 0; }

public slots:
    void RegisterStatusNotifierItem(const QString &service)
    {
        m_object->registerItemFromOwner(service, m_object->callerUniqueOwner());
    }
    void UnregisterStatusNotifierItem(const QString &service)
    {
        m_object->unregisterItemFromOwner(service, m_object->callerUniqueOwner());
    }
    void RegisterStatusNotifierHost(const QString &host)
    {
        m_object->registerHostFromOwner(host, m_object->callerUniqueOwner());
    }

signals:
    void StatusNotifierItemRegistered(const QString &service);
    void StatusNotifierItemUnregistered(const QString &service);
    void StatusNotifierHostRegistered();

private:
    StatusNotifierLocalWatcherObject *m_object = nullptr;
};

class KdeWatcherAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierWatcher")
    Q_PROPERTY(QStringList RegisteredStatusNotifierItems READ registeredItems)
    Q_PROPERTY(bool IsStatusNotifierHostRegistered READ hostRegistered)
    Q_PROPERTY(int ProtocolVersion READ protocolVersion)

public:
    explicit KdeWatcherAdaptor(StatusNotifierLocalWatcherObject *object)
        : QDBusAbstractAdaptor(object), m_object(object)
    {
        connect(m_object, &StatusNotifierLocalWatcherObject::itemRegisteredForDbus, this,
                &KdeWatcherAdaptor::StatusNotifierItemRegistered);
        connect(m_object, &StatusNotifierLocalWatcherObject::itemUnregisteredForDbus, this,
                &KdeWatcherAdaptor::StatusNotifierItemUnregistered);
        connect(m_object, &StatusNotifierLocalWatcherObject::hostRegisteredChanged, this,
                [this](const QString &host, bool registered) {
            Q_UNUSED(host)
            if (registered)
                emit StatusNotifierHostRegistered();
        });
    }

    QStringList registeredItems() const { return m_object->items(); }
    bool hostRegistered() const { return m_object->hostRegistered(); }
    int protocolVersion() const { return 0; }

public slots:
    void RegisterStatusNotifierItem(const QString &service)
    {
        m_object->registerItemFromOwner(service, m_object->callerUniqueOwner());
    }
    void UnregisterStatusNotifierItem(const QString &service)
    {
        m_object->unregisterItemFromOwner(service, m_object->callerUniqueOwner());
    }
    void RegisterStatusNotifierHost(const QString &host)
    {
        m_object->registerHostFromOwner(host, m_object->callerUniqueOwner());
    }

signals:
    void StatusNotifierItemRegistered(const QString &service);
    void StatusNotifierItemUnregistered(const QString &service);
    void StatusNotifierHostRegistered();

private:
    StatusNotifierLocalWatcherObject *m_object = nullptr;
};

} // namespace

QStringList StatusNotifierLocalWatcherObject::items() const
{
    QStringList result;
    result.reserve(m_items.size());
    for (const ItemRecord &record : std::as_const(m_items))
        result.append(record.registration);
    std::sort(result.begin(), result.end());
    return result;
}

void StatusNotifierLocalWatcherObject::registerItem(const QString &registration)
{
    const QString owner = calledFromDBus() ? message().service() : QString();
    registerItemForOwner(registration, owner);
}

void StatusNotifierLocalWatcherObject::registerItemFromOwner(const QString &registration,
                                                             const QString &owner)
{
    registerItemForOwner(registration, owner);
}

void StatusNotifierLocalWatcherObject::unregisterItemFromOwner(const QString &registration,
                                                               const QString &owner)
{
    unregisterItemForOwner(registration, owner);
}

void StatusNotifierLocalWatcherObject::unregisterItemForOwner(const QString &registration,
                                                              const QString &owner)
{
    QString error;
    ItemAddress address = normalizeRegistration(registration, owner, &error);
    if (!address.isValid()) {
        emit registrationRejected(error);
        return;
    }
    if (address.uniqueOwner.isEmpty())
        address.uniqueOwner = owner;
    const QString key = address.key();
    const auto it = m_items.constFind(key);
    if (it == m_items.constEnd())
        return;
    const QString canonical = it->registration;
    const QString itemOwner = it->owner;
    m_items.erase(it);
    emit itemUnregistered(canonical, itemOwner);
    emit itemUnregisteredForDbus(canonical);
}

void StatusNotifierLocalWatcherObject::registerItemForOwner(const QString &registration,
                                                            const QString &owner)
{
    QString error;
    ItemAddress address = normalizeRegistration(registration, owner, &error);
    if (!address.isValid()) {
        emit registrationRejected(error);
        return;
    }
    if (address.uniqueOwner.isEmpty())
        address.uniqueOwner = owner;
    const QString key = address.key();
    const QString canonical = canonicalRegistration(address);
    const auto existing = m_items.constFind(key);
    if (existing != m_items.constEnd() && existing->owner == owner)
        return;
    if (existing != m_items.constEnd()) {
        emit itemUnregistered(existing->registration, existing->owner);
        emit itemUnregisteredForDbus(existing->registration);
        m_items.erase(existing);
    }
    m_items.insert(key, {address, canonical, owner});
    emit itemRegistered(canonical, owner);
    emit itemRegisteredForDbus(canonical);
}

void StatusNotifierLocalWatcherObject::registerHost(const QString &host)
{
    const QString owner = calledFromDBus() ? message().service() : QString();
    registerHostForOwner(host, owner);
}

void StatusNotifierLocalWatcherObject::registerHostFromOwner(const QString &host,
                                                             const QString &owner)
{
    registerHostForOwner(host, owner);
}

void StatusNotifierLocalWatcherObject::registerOwnedHost(const QString &host,
                                                          const QString &owner)
{
    registerVerifiedHost(host, owner);
}

void StatusNotifierLocalWatcherObject::registerHostForOwner(const QString &host,
                                                            const QString &owner)
{
    if (!isValidDBusServiceName(host) || !isValidDBusServiceName(owner)
        || !owner.startsWith(QLatin1Char(':')))
        return;
    if (host.startsWith(QLatin1Char(':'))) {
        if (host != owner)
            return;
        registerVerifiedHost(host, owner);
        return;
    }

    const quint64 request = m_nextHostRequest++;
    m_hostRequestGeneration.insert(host, request);
    m_hostRequestOwner.insert(host, owner);
    QDBusInterface dbus(QStringLiteral("org.freedesktop.DBus"),
                        QStringLiteral("/org/freedesktop/DBus"),
                        QStringLiteral("org.freedesktop.DBus"),
                        QDBusConnection::sessionBus());
    auto *pending = new QDBusPendingCallWatcher(
        dbus.asyncCall(QStringLiteral("GetNameOwner"), host), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this,
            [this, pending, host, owner, request] {
        const QDBusMessage reply = pending->reply();
        pending->deleteLater();
        if (m_hostRequestGeneration.value(host) != request
            || m_hostRequestOwner.value(host) != owner)
            return;
        m_hostRequestGeneration.remove(host);
        m_hostRequestOwner.remove(host);
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()
            || reply.arguments().constFirst().toString() != owner)
            return;
        registerVerifiedHost(host, owner);
    });
}

void StatusNotifierLocalWatcherObject::registerVerifiedHost(const QString &host,
                                                             const QString &owner)
{
    const bool wasRegistered = hostRegistered();
    const bool changed = !m_hosts.contains(host) || m_hosts.value(host) != owner;
    m_hosts.insert(host, owner);
    if (!wasRegistered || changed)
        emit hostRegisteredChanged(host, true);
}

void StatusNotifierLocalWatcherObject::removeHost(const QString &host)
{
    m_hostRequestGeneration.remove(host);
    m_hostRequestOwner.remove(host);
    if (m_hosts.remove(host) > 0)
        emit hostRegisteredChanged(host, false);
}

void StatusNotifierLocalWatcherObject::removeOwner(const QString &owner)
{
    const QStringList pendingHosts = m_hostRequestOwner.keys(owner);
    for (const QString &host : pendingHosts) {
        m_hostRequestGeneration.remove(host);
        m_hostRequestOwner.remove(host);
    }

    const QStringList itemKeys = m_items.keys();
    for (const QString &key : itemKeys) {
        const auto it = m_items.constFind(key);
        if (it == m_items.constEnd() || it->owner != owner)
            continue;
        const QString registration = it->registration;
        m_items.erase(it);
        emit itemUnregistered(registration, owner);
        emit itemUnregisteredForDbus(registration);
    }

    const QStringList hosts = m_hosts.keys();
    for (const QString &host : hosts) {
        if (m_hosts.value(host) != owner)
            continue;
        m_hosts.remove(host);
        emit hostRegisteredChanged(host, false);
    }
}

void StatusNotifierLocalWatcherObject::clear()
{
    m_items.clear();
    m_hosts.clear();
    m_hostRequestGeneration.clear();
    m_hostRequestOwner.clear();
}

StatusNotifierWatcherBridge::StatusNotifierWatcherBridge(QObject *parent)
    : QObject(parent)
{
}

WatcherAuthority StatusNotifierWatcherBridge::selectAuthority(const QString &freedesktopOwner,
                                                              const QString &kdeOwner)
{
    WatcherAuthority authority;
    authority.conflict = !freedesktopOwner.isEmpty() && !kdeOwner.isEmpty()
        && freedesktopOwner != kdeOwner;
    if (!freedesktopOwner.isEmpty()) {
        authority.name = QString::fromLatin1(kFreedesktopWatcher);
        authority.owner = freedesktopOwner;
    } else if (!kdeOwner.isEmpty()) {
        authority.name = QString::fromLatin1(kKdeWatcher);
        authority.owner = kdeOwner;
    }
    return authority;
}

StatusNotifierWatcherBridge::~StatusNotifierWatcherBridge()
{
    stop();
}

void StatusNotifierWatcherBridge::start()
{
    if (m_started)
        return;
    registerStatusNotifierDBusMetaTypes();
    m_started = true;
    ++m_watcherGeneration;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        setError(QStringLiteral("session bus is unavailable"));
        return;
    }

    m_serviceWatcher = new QDBusServiceWatcher(this);
    m_serviceWatcher->setConnection(bus);
    m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForRegistration
                                   | QDBusServiceWatcher::WatchForUnregistration
                                   | QDBusServiceWatcher::WatchForOwnerChange);
    for (const QString &name : watcherNames())
        m_serviceWatcher->addWatchedService(name);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            &StatusNotifierWatcherBridge::onServiceOwnerChanged);

    m_resolutionPending = watcherNames().size();
    const quint64 generation = m_watcherGeneration;
    QTimer::singleShot(0, this, [this, generation] {
        if (!m_started || generation != m_watcherGeneration)
            return;
        QDBusInterface dbus(QStringLiteral("org.freedesktop.DBus"),
                            QStringLiteral("/org/freedesktop/DBus"),
                            QStringLiteral("org.freedesktop.DBus"),
                            QDBusConnection::sessionBus());
        m_ownerByName.clear();
        for (const QString &name : watcherNames()) {
            const QDBusMessage reply = dbus.call(QStringLiteral("GetNameOwner"), name);
            if (reply.type() != QDBusMessage::ErrorMessage && !reply.arguments().isEmpty())
                m_ownerByName.insert(name, reply.arguments().constFirst().toString());
        }
        m_resolutionPending = 0;
        selectWatcher();
    });
}

void StatusNotifierWatcherBridge::stop()
{
    if (!m_started && !m_hostServiceOwned && !m_localWatcher)
        return;
    m_started = false;
    ++m_watcherGeneration;
    disconnectExternalWatcher();
    clearRegisteredItems();
    if (m_localWatcher) {
        m_localWatcher->clear();
        QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kWatcherPath));
        m_localWatcher.clear();
    }
    releaseHostService();
    QDBusConnection bus = QDBusConnection::sessionBus();
    for (const QString &name : std::as_const(m_ownedNames))
        bus.unregisterService(name);
    m_ownedNames.clear();
    if (m_serviceWatcher)
        m_serviceWatcher->deleteLater();
    m_serviceWatcher.clear();
    m_mode = WatcherMode::Unavailable;
    m_watcherName.clear();
    m_watcherOwner.clear();
    m_ownerByName.clear();
    m_resolutionPending = 0;
    m_registeredHosts.clear();
    emit stateChanged();
}

void StatusNotifierWatcherBridge::registerTestAddress(const ItemAddress &address)
{
    if (!address.isValid())
        return;
    if (m_addresses.contains(address.key()))
        return;
    m_addresses.insert(address.key(), address);
    emit itemRegistered(address);
    emit stateChanged();
}

void StatusNotifierWatcherBridge::unregisterTestKey(const QString &key)
{
    if (m_addresses.remove(key) > 0)
        emit itemUnregistered(key);
}

void StatusNotifierWatcherBridge::onServiceOwnerChanged(const QString &service,
                                                        const QString &oldOwner,
                                                        const QString &newOwner)
{
    const QStringList aliases = watcherNames();
    if (aliases.contains(service)) {
        if (newOwner.isEmpty())
            m_ownerByName.remove(service);
        else
            m_ownerByName.insert(service, newOwner);
        reevaluateWatcherAuthority();
        return;
    }

    if (m_registeredHosts.contains(service)) {
        m_registeredHosts.remove(service);
        emit stateChanged();
    }
    for (auto it = m_registeredHosts.begin(); it != m_registeredHosts.end();) {
        if (it.value() == service) {
            it = m_registeredHosts.erase(it);
            emit stateChanged();
        } else {
            ++it;
        }
    }

    if (m_mode == WatcherMode::Owned && m_localWatcher) {
        if (!oldOwner.isEmpty() && oldOwner != newOwner)
            m_localWatcher->removeOwner(oldOwner);
        if (newOwner.isEmpty())
            m_localWatcher->removeOwner(service);
        m_localWatcher->removeHost(service);
    }

    const QStringList keys = m_addresses.keys();
    for (const QString &key : keys) {
        const auto it = m_addresses.constFind(key);
        if (it == m_addresses.constEnd())
            continue;
        const ItemAddress address = it.value();
        const bool ownerMatch = address.uniqueOwner == service;
        const bool wellKnownMatch = !service.startsWith(QLatin1Char(':'))
            && address.service == service;
        if (!ownerMatch && !wellKnownMatch)
            continue;
        if (newOwner.isEmpty() || (wellKnownMatch && address.uniqueOwner != newOwner)) {
            const QString owner = address.uniqueOwner;
            m_addresses.remove(key);
            emit itemOwnerVanished(key, owner);
            emit itemUnregistered(key);
        }
    }
}

void StatusNotifierWatcherBridge::onItemRegistered(const QString &registration)
{
    handleRegistration(registration, {}, m_watcherGeneration);
}

void StatusNotifierWatcherBridge::onItemUnregistered(const QString &registration)
{
    QString error;
    const auto address = normalizeRegistration(registration, {}, &error);
    QStringList keys;
    if (address.isValid()) {
        if (m_addresses.contains(address.key()))
            keys.append(address.key());
    } else if (registration.startsWith(QLatin1Char('/'))) {
        for (auto it = m_addresses.cbegin(); it != m_addresses.cend(); ++it) {
            if (it->objectPath == registration)
                keys.append(it.key());
        }
    }
    for (const QString &key : keys) {
        m_addresses.remove(key);
        emit itemUnregistered(key);
    }
}

void StatusNotifierWatcherBridge::onLocalItemRegistered(const QString &registration,
                                                        const QString &sender)
{
    handleRegistration(registration, sender, m_watcherGeneration);
}

void StatusNotifierWatcherBridge::onLocalItemUnregistered(const QString &registration,
                                                          const QString &)
{
    onItemUnregistered(registration);
}

void StatusNotifierWatcherBridge::onLocalHostRegistered(const QString &host, bool)
{
    if (m_serviceWatcher)
        m_serviceWatcher->addWatchedService(host);
    emit stateChanged();
}

void StatusNotifierWatcherBridge::onWatcherPropertiesChanged(const QString &interfaceName,
                                                             const QVariantMap &changed,
                                                             const QStringList &invalidated)
{
    Q_UNUSED(invalidated)
    if (m_mode != WatcherMode::External || interfaceName != watcherInterface(m_watcherName))
        return;
    if (changed.contains(QStringLiteral("RegisteredStatusNotifierItems")))
        enumerateExternalItems();
    if (changed.contains(QStringLiteral("IsStatusNotifierHostRegistered")))
        refreshExternalHostRegistration();
}

void StatusNotifierWatcherBridge::resolveWatcherOwner(const QString &name, quint64 generation)
{
    QDBusInterface dbus(QStringLiteral("org.freedesktop.DBus"),
                        QStringLiteral("/org/freedesktop/DBus"),
                        QStringLiteral("org.freedesktop.DBus"),
                        QDBusConnection::sessionBus());
    auto *pending = new QDBusPendingCallWatcher(
        dbus.asyncCall(QStringLiteral("GetNameOwner"), name), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this,
            [this, pending, name, generation] {
        const QDBusMessage reply = pending->reply();
        pending->deleteLater();
        if (generation != m_watcherGeneration || !m_started)
            return;
        if (reply.type() != QDBusMessage::ErrorMessage && !reply.arguments().isEmpty())
            m_ownerByName.insert(name, reply.arguments().constFirst().toString());
        else
            m_ownerByName.remove(name);
        m_resolutionPending = qMax(0, m_resolutionPending - 1);
        resolveWatcherOwners();
    });
}

void StatusNotifierWatcherBridge::resolveWatcherOwners()
{
    if (!m_started || m_resolutionPending > 0)
        return;
    selectWatcher();
}

void StatusNotifierWatcherBridge::handleRegistration(const QString &registration,
                                                     const QString &senderUniqueOwner,
                                                     quint64 generation)
{
    if (generation != 0 && generation != m_watcherGeneration)
        return;
    QString error;
    const auto partial = normalizeRegistration(registration, senderUniqueOwner, &error);
    if (!partial.isValid()) {
        emit healthWarning(error);
        return;
    }
    if (!partial.uniqueOwner.isEmpty() || !senderUniqueOwner.isEmpty()) {
        ItemAddress address = partial;
        if (address.uniqueOwner.isEmpty())
            address.uniqueOwner = senderUniqueOwner;
        const QString key = address.key();
        const auto existing = m_addresses.constFind(key);
        if (existing != m_addresses.constEnd() && existing->uniqueOwner == address.uniqueOwner)
            return;
        if (existing != m_addresses.constEnd()) {
            const QString oldOwner = existing->uniqueOwner;
            m_addresses.remove(key);
            emit itemOwnerVanished(key, oldOwner);
            emit itemUnregistered(key);
        }
        m_addresses.insert(key, address);
        registerItemOwnerWatcher(address);
        emit itemRegistered(address);
        return;
    }

    QDBusInterface dbus(QStringLiteral("org.freedesktop.DBus"),
                        QStringLiteral("/org/freedesktop/DBus"),
                        QStringLiteral("org.freedesktop.DBus"),
                        QDBusConnection::sessionBus());
    const QString service = partial.service;
    auto *pending = new QDBusPendingCallWatcher(
        dbus.asyncCall(QStringLiteral("GetNameOwner"), service), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this,
            [this, pending, partial, service, generation] {
        const QDBusMessage reply = pending->reply();
        pending->deleteLater();
        if (generation != m_watcherGeneration || !m_started)
            return;
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
            emit healthWarning(QStringLiteral("StatusNotifierItem owner disappeared: %1")
                                   .arg(service));
            return;
        }
        ItemAddress address = partial;
        address.uniqueOwner = reply.arguments().constFirst().toString();
        const QString key = address.key();
        const auto existing = m_addresses.constFind(key);
        if (existing != m_addresses.constEnd() && existing->uniqueOwner == address.uniqueOwner)
            return;
        if (existing != m_addresses.constEnd()) {
            const QString oldOwner = existing->uniqueOwner;
            m_addresses.remove(key);
            emit itemOwnerVanished(key, oldOwner);
            emit itemUnregistered(key);
        }
        m_addresses.insert(key, address);
        registerItemOwnerWatcher(address);
        emit itemRegistered(address);
    });
}

void StatusNotifierWatcherBridge::enumerateExternalItems()
{
    if (m_mode != WatcherMode::External)
        return;
    const quint64 generation = m_watcherGeneration;
    const QString expectedName = m_watcherName;
    const QString expectedOwner = m_watcherOwner;
    QDBusInterface watcher(m_watcherName, QString::fromLatin1(kWatcherPath),
                            QStringLiteral("org.freedesktop.DBus.Properties"),
                            QDBusConnection::sessionBus());
    auto *pending = new QDBusPendingCallWatcher(
        watcher.asyncCall(QStringLiteral("Get"), watcherInterface(m_watcherName),
                          QStringLiteral("RegisteredStatusNotifierItems")), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this,
            [this, pending, generation, expectedName, expectedOwner] {
        const QDBusMessage reply = pending->reply();
        pending->deleteLater();
        if (generation != m_watcherGeneration || !m_started || m_mode != WatcherMode::External
            || expectedName != m_watcherName || expectedOwner != m_watcherOwner)
            return;
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty())
            return;
        QVariant value = reply.arguments().constFirst();
        if (value.canConvert<QDBusVariant>())
            value = value.value<QDBusVariant>().variant();
        if (value.canConvert<QDBusArgument>())
            value = value.value<QDBusArgument>().asVariant();
        for (const QString &registration : value.toStringList())
            handleRegistration(registration, {}, generation);
    });
}

void StatusNotifierWatcherBridge::selectWatcher()
{
    if (!m_started)
        return;
    const QString freedesktop = QString::fromLatin1(kFreedesktopWatcher);
    const QString kde = QString::fromLatin1(kKdeWatcher);
    const QString fdoOwner = m_ownerByName.value(freedesktop);
    const QString kdeOwner = m_ownerByName.value(kde);

    WatcherAuthority authority = selectAuthority(fdoOwner, kdeOwner);
    const QString localOwner = QDBusConnection::sessionBus().baseService();
    if (authority.conflict) {
        emit healthWarning(QStringLiteral(
            "freedesktop and KDE StatusNotifierWatcher owners differ; using %1")
                               .arg(authority.name));
        // Never keep an Eclipse-owned alias while the other compatibility alias
        // is controlled by an external registry.
        if (fdoOwner == localOwner && kdeOwner != localOwner)
            authority = {kde, kdeOwner, true};
        else if (kdeOwner == localOwner && fdoOwner != localOwner)
            authority = {freedesktop, fdoOwner, true};
    }

    if (authority.isValid()) {
        if (m_mode == WatcherMode::Owned && authority.owner == localOwner)
            return;
        if (m_mode == WatcherMode::Owned)
            detachLocalWatcher();
        attachExternalWatcher(authority.name, authority.owner);
        return;
    }

    if (m_mode == WatcherMode::External)
        detachExternalWatcher();
    if (m_mode == WatcherMode::Owned)
        detachLocalWatcher();
    tryOwnWatcher();
}

void StatusNotifierWatcherBridge::reevaluateWatcherAuthority()
{
    if (!m_started || m_resolutionPending > 0)
        return;
    selectWatcher();
}

bool StatusNotifierWatcherBridge::ensureHostService()
{
    if (m_hostServiceOwned)
        return true;
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString uniqueId = bus.baseService().mid(1).replace(QLatin1Char('.'), QLatin1Char('-'));
    m_hostServiceName = QStringLiteral("org.freedesktop.StatusNotifierHost-%1-%2-%3")
        .arg(uniqueId)
        .arg(QCoreApplication::applicationPid())
        .arg(++m_hostGeneration);
    if (bus.registerService(m_hostServiceName)
        != QDBusConnectionInterface::ServiceRegistered) {
        setError(QStringLiteral("unable to own StatusNotifierHost service %1")
                     .arg(m_hostServiceName));
        m_hostServiceName.clear();
        return false;
    }
    m_hostServiceOwned = true;
    return true;
}

void StatusNotifierWatcherBridge::releaseHostService()
{
    if (m_hostServiceOwned) {
        QDBusConnection::sessionBus().unregisterService(m_hostServiceName);
        m_registeredHosts.remove(m_hostServiceName);
    }
    m_hostServiceOwned = false;
    m_hostServiceName.clear();
}

void StatusNotifierWatcherBridge::tryOwnWatcher()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString freedesktop = QString::fromLatin1(kFreedesktopWatcher);
    if (!ensureHostService())
        return;
    if (bus.registerService(freedesktop) != QDBusConnectionInterface::ServiceRegistered) {
        releaseHostService();
        setError(QStringLiteral("unable to own a StatusNotifierWatcher name"));
        return;
    }

    m_localWatcher = new StatusNotifierLocalWatcherObject(this);
    new FreedesktopWatcherAdaptor(m_localWatcher.data());
    new KdeWatcherAdaptor(m_localWatcher.data());
    if (!bus.registerObject(QString::fromLatin1(kWatcherPath), m_localWatcher.data(),
                            QDBusConnection::ExportAdaptors)) {
        bus.unregisterService(freedesktop);
        m_localWatcher.clear();
        releaseHostService();
        setError(QStringLiteral("unable to export the owned StatusNotifierWatcher"));
        return;
    }
    m_ownedNames.append(freedesktop);
    m_ownerByName.insert(freedesktop, bus.baseService());
    if (bus.registerService(QString::fromLatin1(kKdeWatcher))
        == QDBusConnectionInterface::ServiceRegistered) {
        m_ownedNames.append(QString::fromLatin1(kKdeWatcher));
        m_ownerByName.insert(QString::fromLatin1(kKdeWatcher), bus.baseService());
    } else {
        emit healthWarning(QStringLiteral(
            "owned watcher could not claim the KDE compatibility alias"));
    }
    connect(m_localWatcher, &StatusNotifierLocalWatcherObject::itemRegistered, this,
            &StatusNotifierWatcherBridge::onLocalItemRegistered);
    connect(m_localWatcher,
            static_cast<void (StatusNotifierLocalWatcherObject::*)(const QString &, const QString &)>(
                &StatusNotifierLocalWatcherObject::itemUnregistered), this,
            &StatusNotifierWatcherBridge::onLocalItemUnregistered);
    connect(m_localWatcher, &StatusNotifierLocalWatcherObject::hostRegisteredChanged, this,
            &StatusNotifierWatcherBridge::onLocalHostRegistered);
    connect(m_localWatcher, &StatusNotifierLocalWatcherObject::registrationRejected, this,
            &StatusNotifierWatcherBridge::healthWarning);
    m_localWatcher->registerVerifiedHost(m_hostServiceName, bus.baseService());
    m_mode = WatcherMode::Owned;
    m_watcherName = freedesktop;
    m_watcherOwner = bus.baseService();
    emit stateChanged();

    const QString kdeOwner = bus.interface()
        ? bus.interface()->serviceOwner(QString::fromLatin1(kKdeWatcher)).value()
        : QString();
    if (!kdeOwner.isEmpty() && kdeOwner != bus.baseService()) {
        m_ownerByName.insert(QString::fromLatin1(kKdeWatcher), kdeOwner);
        QTimer::singleShot(0, this, [this] { reevaluateWatcherAuthority(); });
    }
}

void StatusNotifierWatcherBridge::detachLocalWatcher()
{
    if (m_mode != WatcherMode::Owned && !m_localWatcher && m_ownedNames.isEmpty())
        return;
    ++m_watcherGeneration;
    clearRegisteredItems();
    if (m_localWatcher) {
        m_localWatcher->clear();
        QDBusConnection::sessionBus().unregisterObject(QString::fromLatin1(kWatcherPath));
        m_localWatcher.clear();
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    for (const QString &name : std::as_const(m_ownedNames))
        bus.unregisterService(name);
    m_ownedNames.clear();
    releaseHostService();
    m_mode = WatcherMode::Unavailable;
    m_watcherName.clear();
    m_watcherOwner.clear();
    emit stateChanged();
}

void StatusNotifierWatcherBridge::disconnectExternalWatcher()
{
    if (m_watcherName.isEmpty() || m_mode != WatcherMode::External)
        return;
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString interfaceName = watcherInterface(m_watcherName);
    bus.disconnect(m_watcherName, QString::fromLatin1(kWatcherPath), interfaceName,
                   QStringLiteral("StatusNotifierItemRegistered"), this,
                   SLOT(onItemRegistered(QString)));
    bus.disconnect(m_watcherName, QString::fromLatin1(kWatcherPath), interfaceName,
                   QStringLiteral("StatusNotifierItemUnregistered"), this,
                   SLOT(onItemUnregistered(QString)));
    bus.disconnect(m_watcherName, QString::fromLatin1(kWatcherPath),
                   QStringLiteral("org.freedesktop.DBus.Properties"),
                   QStringLiteral("PropertiesChanged"), this,
                   SLOT(onWatcherPropertiesChanged(QString,QVariantMap,QStringList)));
    if (m_serviceWatcher)
        m_serviceWatcher->removeWatchedService(m_watcherName);
}

void StatusNotifierWatcherBridge::detachExternalWatcher()
{
    if (m_mode != WatcherMode::External)
        return;
    disconnectExternalWatcher();
    ++m_watcherGeneration;
    clearRegisteredItems();
    releaseHostService();
    m_mode = WatcherMode::Unavailable;
    m_watcherName.clear();
    m_watcherOwner.clear();
    m_registeredHosts.clear();
    emit stateChanged();
}

void StatusNotifierWatcherBridge::attachExternalWatcher(const QString &name,
                                                         const QString &owner)
{
    if (m_mode == WatcherMode::External && m_watcherName == name && m_watcherOwner == owner)
        return;
    if (m_mode == WatcherMode::External)
        detachExternalWatcher();
    if (!ensureHostService())
        return;
    m_mode = WatcherMode::External;
    m_watcherName = name;
    m_watcherOwner = owner;
    ++m_watcherGeneration;
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString interfaceName = watcherInterface(name);
    bus.connect(name, QString::fromLatin1(kWatcherPath), interfaceName,
                QStringLiteral("StatusNotifierItemRegistered"), this,
                SLOT(onItemRegistered(QString)));
    bus.connect(name, QString::fromLatin1(kWatcherPath), interfaceName,
                QStringLiteral("StatusNotifierItemUnregistered"), this,
                SLOT(onItemUnregistered(QString)));
    bus.connect(name, QString::fromLatin1(kWatcherPath),
                QStringLiteral("org.freedesktop.DBus.Properties"),
                QStringLiteral("PropertiesChanged"), this,
                SLOT(onWatcherPropertiesChanged(QString,QVariantMap,QStringList)));
    m_serviceWatcher->addWatchedService(m_hostServiceName);
    registerHostWithExternalWatcher();
    enumerateExternalItems();
    refreshExternalHostRegistration();
    emit stateChanged();
}

void StatusNotifierWatcherBridge::registerHostWithExternalWatcher()
{
    const quint64 generation = m_watcherGeneration;
    const QString expectedName = m_watcherName;
    const QString expectedOwner = m_watcherOwner;
    QDBusInterface watcher(m_watcherName, QString::fromLatin1(kWatcherPath),
                            watcherInterface(m_watcherName), QDBusConnection::sessionBus());
    auto *pending = new QDBusPendingCallWatcher(
        watcher.asyncCall(QStringLiteral("RegisterStatusNotifierHost"), m_hostServiceName), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this,
            [this, pending, generation, expectedName, expectedOwner] {
        const QDBusMessage reply = pending->reply();
        pending->deleteLater();
        if (!m_started || generation != m_watcherGeneration || m_mode != WatcherMode::External
            || expectedName != m_watcherName || expectedOwner != m_watcherOwner)
            return;
        if (reply.type() == QDBusMessage::ErrorMessage) {
            setError(reply.errorMessage());
            return;
        }
        m_registeredHosts.insert(m_hostServiceName,
                                 QDBusConnection::sessionBus().baseService());
        emit stateChanged();
    });
}

void StatusNotifierWatcherBridge::refreshExternalHostRegistration()
{
    if (m_mode != WatcherMode::External || m_watcherName.isEmpty())
        return;
    const quint64 generation = m_watcherGeneration;
    const QString expectedName = m_watcherName;
    const QString expectedOwner = m_watcherOwner;
    QDBusInterface watcher(m_watcherName, QString::fromLatin1(kWatcherPath),
                            QStringLiteral("org.freedesktop.DBus.Properties"),
                            QDBusConnection::sessionBus());
    auto *pending = new QDBusPendingCallWatcher(
        watcher.asyncCall(QStringLiteral("Get"), watcherInterface(m_watcherName),
                          QStringLiteral("IsStatusNotifierHostRegistered")), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this,
            [this, pending, generation, expectedName, expectedOwner] {
        const QDBusMessage reply = pending->reply();
        pending->deleteLater();
        if (!m_started || generation != m_watcherGeneration || m_mode != WatcherMode::External
            || expectedName != m_watcherName || expectedOwner != m_watcherOwner)
            return;
        bool registered = false;
        if (reply.type() != QDBusMessage::ErrorMessage && !reply.arguments().isEmpty()) {
            QVariant value = reply.arguments().constFirst();
            if (value.canConvert<QDBusVariant>())
                value = value.value<QDBusVariant>().variant();
            registered = value.toBool();
        }
        if (registered)
            m_registeredHosts.insert(m_hostServiceName,
                                     QDBusConnection::sessionBus().baseService());
        else
            m_registeredHosts.remove(m_hostServiceName);
        emit stateChanged();
    });
}

void StatusNotifierWatcherBridge::registerItemOwnerWatcher(const ItemAddress &address)
{
    if (!m_serviceWatcher)
        return;
    if (!address.service.startsWith(QLatin1Char(':')))
        m_serviceWatcher->addWatchedService(address.service);
    if (!address.uniqueOwner.isEmpty())
        m_serviceWatcher->addWatchedService(address.uniqueOwner);
}

void StatusNotifierWatcherBridge::clearRegisteredItems()
{
    const auto addresses = m_addresses;
    m_addresses.clear();
    for (auto it = addresses.cbegin(); it != addresses.cend(); ++it) {
        emit itemOwnerVanished(it.key(), it->uniqueOwner);
        emit itemUnregistered(it.key());
    }
}

void StatusNotifierWatcherBridge::setError(const QString &error)
{
    m_lastError = error;
    emit stateChanged();
}

} // namespace Astrea::StatusNotifier

#include "StatusNotifierWatcherBridge.moc"
