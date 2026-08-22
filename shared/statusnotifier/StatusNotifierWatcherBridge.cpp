#include "statusnotifier/StatusNotifierWatcherBridge.hpp"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusVariant>

namespace Astrea::StatusNotifier {
namespace {

constexpr QLatin1StringView kFreedesktopWatcher("org.freedesktop.StatusNotifierWatcher");
constexpr QLatin1StringView kKdeWatcher("org.kde.StatusNotifierWatcher");
constexpr QLatin1StringView kWatcherPath("/StatusNotifierWatcher");
constexpr QLatin1StringView kFreedesktopInterface("org.freedesktop.StatusNotifierWatcher");
constexpr QLatin1StringView kKdeInterface("org.kde.StatusNotifierWatcher");
constexpr QLatin1StringView kHostName("org.astrea.Shell");

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
        connect(m_object, SIGNAL(itemRegisteredForDbus(QString)), this,
                SIGNAL(StatusNotifierItemRegistered(QString)));
        connect(m_object, SIGNAL(itemUnregistered(QString)), this,
                SIGNAL(StatusNotifierItemUnregistered(QString)));
    }

    QStringList registeredItems() const;
    bool hostRegistered() const;
    int protocolVersion() const { return 0; }

public slots:
    void RegisterStatusNotifierItem(const QString &service) { m_object->registerItem(service); }
    void RegisterStatusNotifierHost(const QString &host) { m_object->registerHost(host); }

signals:
    void StatusNotifierItemRegistered(const QString &service);
    void StatusNotifierItemUnregistered(const QString &service);

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
        connect(m_object, SIGNAL(itemRegisteredForDbus(QString)), this,
                SIGNAL(StatusNotifierItemRegistered(QString)));
        connect(m_object, SIGNAL(itemUnregistered(QString)), this,
                SIGNAL(StatusNotifierItemUnregistered(QString)));
    }

    QStringList registeredItems() const;
    bool hostRegistered() const;
    int protocolVersion() const { return 0; }

public slots:
    void RegisterStatusNotifierItem(const QString &service) { m_object->registerItem(service); }
    void RegisterStatusNotifierHost(const QString &host) { m_object->registerHost(host); }

signals:
    void StatusNotifierItemRegistered(const QString &service);
    void StatusNotifierItemUnregistered(const QString &service);

private:
    StatusNotifierLocalWatcherObject *m_object = nullptr;
};

} // namespace

QStringList FreedesktopWatcherAdaptor::registeredItems() const { return m_object->items(); }
bool FreedesktopWatcherAdaptor::hostRegistered() const { return m_object->hostRegistered(); }
QStringList KdeWatcherAdaptor::registeredItems() const { return m_object->items(); }
bool KdeWatcherAdaptor::hostRegistered() const { return m_object->hostRegistered(); }

StatusNotifierWatcherBridge::StatusNotifierWatcherBridge(QObject *parent)
    : QObject(parent)
{
}

StatusNotifierWatcherBridge::~StatusNotifierWatcherBridge()
{
    stop();
}

void StatusNotifierWatcherBridge::start()
{
    if (m_started)
        return;
    m_started = true;
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
    m_serviceWatcher->addWatchedService(QString::fromLatin1(kFreedesktopWatcher));
    m_serviceWatcher->addWatchedService(QString::fromLatin1(kKdeWatcher));
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            &StatusNotifierWatcherBridge::onServiceOwnerChanged);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceRegistered, this,
            &StatusNotifierWatcherBridge::resolveWatcherOwners);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this,
            &StatusNotifierWatcherBridge::resolveWatcherOwners);

    m_resolutionPending = 2;
    for (const QString &name : {QString::fromLatin1(kFreedesktopWatcher),
                                QString::fromLatin1(kKdeWatcher)}) {
        QDBusInterface dbus(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                            QStringLiteral("org.freedesktop.DBus"), bus);
        auto *watcher = new QDBusPendingCallWatcher(dbus.asyncCall(QStringLiteral("NameHasOwner"), name),
                                                    this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this, watcher, name] {
            const QDBusMessage reply = watcher->reply();
            watcher->deleteLater();
            if (reply.type() != QDBusMessage::ErrorMessage && !reply.arguments().isEmpty()
                && reply.arguments().constFirst().toBool()) {
                QDBusInterface ownerInterface(QStringLiteral("org.freedesktop.DBus"),
                                              QStringLiteral("/org/freedesktop/DBus"),
                                              QStringLiteral("org.freedesktop.DBus"),
                                              QDBusConnection::sessionBus());
                auto *ownerWatcher = new QDBusPendingCallWatcher(
                    ownerInterface.asyncCall(QStringLiteral("GetNameOwner"), name), this);
                connect(ownerWatcher, &QDBusPendingCallWatcher::finished, this,
                        [this, ownerWatcher, name] {
                    const QDBusMessage ownerReply = ownerWatcher->reply();
                    ownerWatcher->deleteLater();
                    if (ownerReply.type() != QDBusMessage::ErrorMessage
                        && !ownerReply.arguments().isEmpty())
                        m_ownerByName.insert(name, ownerReply.arguments().constFirst().toString());
                    --m_resolutionPending;
                    resolveWatcherOwners();
                });
                return;
            }
            m_ownerByName.remove(name);
            --m_resolutionPending;
            resolveWatcherOwners();
        });
    }
}

void StatusNotifierWatcherBridge::stop()
{
    if (!m_started)
        return;
    m_started = false;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (m_localWatcher) {
        bus.unregisterObject(QString::fromLatin1(kWatcherPath));
        m_localWatcher.clear();
    }
    for (const QString &name : std::as_const(m_ownedNames))
        bus.unregisterService(name);
    m_ownedNames.clear();
    if (m_serviceWatcher)
        m_serviceWatcher->deleteLater();
    m_serviceWatcher.clear();
    m_mode = WatcherMode::Unavailable;
    m_watcherName.clear();
    m_watcherOwner.clear();
    m_hostRegistered = false;
    m_addresses.clear();
    m_keyByService.clear();
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
                                                        const QString &, const QString &newOwner)
{
    if (service == m_watcherName) {
        m_watcherOwner = newOwner;
        if (newOwner.isEmpty()) {
            m_mode = WatcherMode::Unavailable;
            m_hostRegistered = false;
        }
        emit stateChanged();
        resolveWatcherOwners();
        return;
    }
    const QString key = m_keyByService.value(service);
    if (!key.isEmpty() && newOwner.isEmpty()) {
        const QString oldOwner = m_addresses.value(key).uniqueOwner;
        emit itemOwnerVanished(key, oldOwner);
        emit itemUnregistered(key);
        m_addresses.remove(key);
    }
}

void StatusNotifierWatcherBridge::onItemRegistered(const QString &registration)
{
    handleRegistration(registration);
}

void StatusNotifierWatcherBridge::onItemUnregistered(const QString &registration)
{
    QString error;
    const auto address = normalizeRegistration(registration, QString(),
                                               QStringLiteral("/StatusNotifierItem"), &error);
    if (!address.isValid())
        return;
    const QString key = address.key();
    m_addresses.remove(key);
    emit itemUnregistered(key);
}

void StatusNotifierWatcherBridge::onLocalItemRegistered(const QString &registration,
                                                        const QString &sender)
{
    handleRegistration(registration, sender);
}

void StatusNotifierWatcherBridge::onLocalHostRegistered(const QString &)
{
    m_hostRegistered = true;
    emit stateChanged();
}

void StatusNotifierWatcherBridge::resolveWatcherOwners()
{
    if (!m_started || m_resolutionPending > 0)
        return;
    selectWatcher();
}

void StatusNotifierWatcherBridge::handleRegistration(const QString &registration,
                                                     const QString &senderUniqueOwner)
{
    QString error;
    const auto partial = normalizeRegistration(registration, senderUniqueOwner, QString(), &error);
    if (!partial.isValid()) {
        emit healthWarning(error);
        return;
    }
    if (!partial.uniqueOwner.isEmpty()) {
        m_keyByService.insert(partial.service, partial.key());
        m_addresses.insert(partial.key(), partial);
        registerItemOwnerWatcher(partial);
        emit itemRegistered(partial);
        return;
    }
    QDBusInterface dbus(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                        QStringLiteral("org.freedesktop.DBus"), QDBusConnection::sessionBus());
    auto *watcher = new QDBusPendingCallWatcher(
        dbus.asyncCall(QStringLiteral("GetNameOwner"), partial.service), this);
    const QString service = partial.service;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, partial, service] {
        const QDBusMessage reply = watcher->reply();
        watcher->deleteLater();
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
            emit healthWarning(QStringLiteral("StatusNotifierItem owner disappeared: %1").arg(service));
            return;
        }
        ItemAddress address = partial;
        address.uniqueOwner = reply.arguments().constFirst().toString();
        m_keyByService.insert(address.service, address.key());
        m_addresses.insert(address.key(), address);
        registerItemOwnerWatcher(address);
        emit itemRegistered(address);
    });
}

void StatusNotifierWatcherBridge::enumerateExternalItems()
{
    if (m_mode != WatcherMode::External)
        return;
    QDBusInterface watcher(m_watcherName, QString::fromLatin1(kWatcherPath),
                           QStringLiteral("org.freedesktop.DBus.Properties"),
                           QDBusConnection::sessionBus());
    auto *pending = new QDBusPendingCallWatcher(
        watcher.asyncCall(QStringLiteral("Get"), m_watcherName,
                          QStringLiteral("RegisteredStatusNotifierItems")), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this,
            [this, pending] {
        const QDBusMessage reply = pending->reply();
        pending->deleteLater();
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty())
            return;
        QVariant value = reply.arguments().constFirst();
        if (value.canConvert<QDBusVariant>())
            value = value.value<QDBusVariant>().variant();
        for (const QString &registration : value.toStringList())
            handleRegistration(registration);
    });
}

void StatusNotifierWatcherBridge::selectWatcher()
{
    if (!m_started || m_mode == WatcherMode::Owned)
        return;
    const QString freedesktop = QString::fromLatin1(kFreedesktopWatcher);
    const QString kde = QString::fromLatin1(kKdeWatcher);
    const QString fdoOwner = m_ownerByName.value(freedesktop);
    const QString kdeOwner = m_ownerByName.value(kde);
    if (!fdoOwner.isEmpty() || !kdeOwner.isEmpty()) {
        if (!fdoOwner.isEmpty() && !kdeOwner.isEmpty() && fdoOwner != kdeOwner)
            emit healthWarning(QStringLiteral("freedesktop and KDE StatusNotifierWatcher owners differ; using freedesktop"));
        attachExternalWatcher(!fdoOwner.isEmpty() ? freedesktop : kde,
                               !fdoOwner.isEmpty() ? fdoOwner : kdeOwner);
        return;
    }
    tryOwnWatcher();
}

void StatusNotifierWatcherBridge::tryOwnWatcher()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString freedesktop = QString::fromLatin1(kFreedesktopWatcher);
    const QString kde = QString::fromLatin1(kKdeWatcher);
    if (bus.registerService(freedesktop) != QDBusConnectionInterface::ServiceRegistered) {
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
        setError(QStringLiteral("unable to export the owned StatusNotifierWatcher"));
        return;
    }
    m_ownedNames.append(freedesktop);
    if (bus.registerService(kde) == QDBusConnectionInterface::ServiceRegistered)
        m_ownedNames.append(kde);
    else
        emit healthWarning(QStringLiteral("owned watcher could not claim the KDE compatibility alias"));
    connect(m_localWatcher, &StatusNotifierLocalWatcherObject::itemRegistered, this,
            &StatusNotifierWatcherBridge::onLocalItemRegistered);
    connect(m_localWatcher, &StatusNotifierLocalWatcherObject::hostRegisteredChanged, this,
            &StatusNotifierWatcherBridge::onLocalHostRegistered);
    m_mode = WatcherMode::Owned;
    m_watcherName = freedesktop;
    m_watcherOwner = bus.baseService();
    m_hostRegistered = true;
    emit stateChanged();
}

void StatusNotifierWatcherBridge::attachExternalWatcher(const QString &name, const QString &owner)
{
    if (m_mode == WatcherMode::External && m_watcherName == name && m_watcherOwner == owner)
        return;
    QDBusConnection bus = QDBusConnection::sessionBus();
    m_mode = WatcherMode::External;
    m_watcherName = name;
    m_watcherOwner = owner;
    const QString interfaceName = name == QString::fromLatin1(kFreedesktopWatcher)
        ? QString::fromLatin1(kFreedesktopInterface) : QString::fromLatin1(kKdeInterface);
    bus.connect(name, QString::fromLatin1(kWatcherPath), interfaceName,
                QStringLiteral("StatusNotifierItemRegistered"), this,
                SLOT(onItemRegistered(QString)));
    bus.connect(name, QString::fromLatin1(kWatcherPath), interfaceName,
                QStringLiteral("StatusNotifierItemUnregistered"), this,
                SLOT(onItemUnregistered(QString)));
    registerHostWithExternalWatcher();
    enumerateExternalItems();
    emit stateChanged();
}

void StatusNotifierWatcherBridge::registerHostWithExternalWatcher()
{
    QDBusInterface watcher(m_watcherName, QString::fromLatin1(kWatcherPath),
                           m_watcherName == QString::fromLatin1(kFreedesktopWatcher)
                               ? QString::fromLatin1(kFreedesktopInterface)
                               : QString::fromLatin1(kKdeInterface),
                           QDBusConnection::sessionBus());
    auto *pending = new QDBusPendingCallWatcher(
        watcher.asyncCall(QStringLiteral("RegisterStatusNotifierHost"), QString::fromLatin1(kHostName)),
        this);
    connect(pending, &QDBusPendingCallWatcher::finished, this,
            [this, pending] {
        const QDBusMessage reply = pending->reply();
        pending->deleteLater();
        if (reply.type() == QDBusMessage::ErrorMessage) {
            setError(reply.errorMessage());
            return;
        }
        m_hostRegistered = true;
        emit stateChanged();
    });
}

void StatusNotifierWatcherBridge::registerItemOwnerWatcher(const ItemAddress &address)
{
    if (m_serviceWatcher && !address.service.startsWith(QLatin1Char(':')))
        m_serviceWatcher->addWatchedService(address.service);
}

void StatusNotifierWatcherBridge::setError(const QString &error)
{
    m_lastError = error;
    emit stateChanged();
}

} // namespace Astrea::StatusNotifier

#include "StatusNotifierWatcherBridge.moc"
