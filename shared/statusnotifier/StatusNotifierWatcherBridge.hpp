#pragma once

#include "statusnotifier/StatusNotifierTypes.hpp"

#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QObject>
#include <QHash>
#include <QPointer>

class QDBusServiceWatcher;

namespace Astrea::StatusNotifier {

struct WatcherAuthority {
    QString name;
    QString owner;
    bool conflict = false;

    bool isValid() const { return !name.isEmpty() && !owner.isEmpty(); }
};

class StatusNotifierLocalWatcherObject final : public QObject, protected QDBusContext {
    Q_OBJECT

public:
    explicit StatusNotifierLocalWatcherObject(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    QStringList items() const;
    QString callerUniqueOwner() const
    { return calledFromDBus() ? message().service() : QString(); }
    bool hostRegistered() const { return !m_hosts.isEmpty(); }
    void registerItem(const QString &registration);
    void registerItemFromOwner(const QString &registration, const QString &owner);
    void unregisterItemFromOwner(const QString &registration, const QString &owner);
    void registerHost(const QString &host);
    void registerHostFromOwner(const QString &host, const QString &owner);
    void registerOwnedHost(const QString &host, const QString &owner);
    void registerVerifiedHost(const QString &host, const QString &owner);
    void removeOwner(const QString &owner);
    void removeHost(const QString &host);
    void clear();

signals:
    void itemRegistered(const QString &registration, const QString &sender);
    void itemUnregistered(const QString &registration, const QString &sender);
    void itemRegisteredForDbus(const QString &registration);
    void itemUnregisteredForDbus(const QString &registration);
    void itemUnregistered(const QString &registration);
    void hostRegisteredChanged(const QString &host, bool registered);
    void registrationRejected(const QString &error);

private:
    struct ItemRecord {
        ItemAddress address;
        QString registration;
        QString owner;
    };

    void registerItemForOwner(const QString &registration, const QString &owner);
    void unregisterItemForOwner(const QString &registration, const QString &owner);
    void registerHostForOwner(const QString &host, const QString &owner);

    QHash<QString, ItemRecord> m_items;
    QHash<QString, QString> m_hosts;
    QHash<QString, quint64> m_hostRequestGeneration;
    QHash<QString, QString> m_hostRequestOwner;
    quint64 m_nextHostRequest = 1;
};

class StatusNotifierWatcherBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(WatcherMode mode READ mode NOTIFY stateChanged)
    Q_PROPERTY(QString watcherName READ watcherName NOTIFY stateChanged)
    Q_PROPERTY(QString watcherOwner READ watcherOwner NOTIFY stateChanged)
    Q_PROPERTY(QString hostServiceName READ hostServiceName NOTIFY stateChanged)
    Q_PROPERTY(bool hostRegistered READ hostRegistered NOTIFY stateChanged)
    Q_PROPERTY(int protocolVersion READ protocolVersion CONSTANT)

public:
    explicit StatusNotifierWatcherBridge(QObject *parent = nullptr);
    ~StatusNotifierWatcherBridge() override;

    void start();
    void stop();
    WatcherMode mode() const { return m_mode; }
    QString watcherName() const { return m_watcherName; }
    QString watcherOwner() const { return m_watcherOwner; }
    QString hostServiceName() const { return m_hostServiceName; }
    bool hostRegistered() const
    { return m_localWatcher ? m_localWatcher->hostRegistered() : !m_registeredHosts.isEmpty(); }
    int protocolVersion() const { return 0; }
    QString lastError() const { return m_lastError; }

    static WatcherAuthority selectAuthority(const QString &freedesktopOwner,
                                            const QString &kdeOwner);

    // These two hooks make parser/model and session-bus recovery tests deterministic.
    void registerTestAddress(const ItemAddress &address);
    void unregisterTestKey(const QString &key);

signals:
    void stateChanged();
    void itemRegistered(const ItemAddress &address);
    void itemUnregistered(const QString &key);
    void itemOwnerVanished(const QString &key, const QString &uniqueOwner);
    void healthWarning(const QString &warning);

private slots:
    void onServiceOwnerChanged(const QString &service, const QString &oldOwner,
                               const QString &newOwner);
    void onItemRegistered(const QString &registration);
    void onItemUnregistered(const QString &registration);
    void onLocalItemRegistered(const QString &registration, const QString &sender);
    void onLocalItemUnregistered(const QString &registration, const QString &sender);
    void onLocalHostRegistered(const QString &host, bool registered);
    void onWatcherPropertiesChanged(const QString &interfaceName,
                                    const QVariantMap &changed,
                                    const QStringList &invalidated);
    void resolveWatcherOwners();

private:
    void handleRegistration(const QString &registration, const QString &senderUniqueOwner = {},
                            quint64 generation = 0);
    void enumerateExternalItems();
    void selectWatcher();
    void tryOwnWatcher();
    void detachLocalWatcher();
    void reevaluateWatcherAuthority();
    void attachExternalWatcher(const QString &name, const QString &owner);
    void detachExternalWatcher();
    void disconnectExternalWatcher();
    void registerHostWithExternalWatcher();
    void refreshExternalHostRegistration();
    void registerItemOwnerWatcher(const ItemAddress &address);
    void resolveWatcherOwner(const QString &name, quint64 generation);
    void clearRegisteredItems();
    bool ensureHostService();
    void releaseHostService();
    void setError(const QString &error);

    WatcherMode m_mode = WatcherMode::Unavailable;
    QString m_watcherName;
    QString m_watcherOwner;
    QString m_hostServiceName;
    QString m_lastError;
    bool m_started = false;
    int m_resolutionPending = 0;
    quint64 m_watcherGeneration = 0;
    quint64 m_hostGeneration = 0;
    bool m_hostServiceOwned = false;
    QHash<QString, QString> m_ownerByName;
    QHash<QString, QString> m_registeredHosts;
    QHash<QString, ItemAddress> m_addresses;
    QPointer<QDBusServiceWatcher> m_serviceWatcher;
    QPointer<StatusNotifierLocalWatcherObject> m_localWatcher;
    QStringList m_ownedNames;
};

} // namespace Astrea::StatusNotifier
