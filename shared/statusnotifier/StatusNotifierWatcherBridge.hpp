#pragma once

#include "statusnotifier/StatusNotifierTypes.hpp"

#include <QDBusContext>
#include <QDBusMessage>
#include <QObject>
#include <QHash>
#include <QPointer>

class QDBusServiceWatcher;

namespace Astrea::StatusNotifier {

class StatusNotifierLocalWatcherObject final : public QObject, protected QDBusContext {
    Q_OBJECT

public:
    explicit StatusNotifierLocalWatcherObject(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    QStringList items() const;
    bool hostRegistered() const { return !m_hosts.isEmpty(); }
    void registerItem(const QString &registration);
    void registerItemFromOwner(const QString &registration, const QString &owner);
    void registerHost(const QString &host);
    void registerHostFromOwner(const QString &host, const QString &owner);
    void registerOwnedHost(const QString &host, const QString &owner);
    void removeOwner(const QString &owner);
    void clear();

signals:
    void itemRegistered(const QString &registration, const QString &sender);
    void itemUnregistered(const QString &registration, const QString &sender);
    void itemRegisteredForDbus(const QString &registration);
    void itemUnregisteredForDbus(const QString &registration);
    void itemUnregistered(const QString &registration);
    void hostRegisteredChanged(const QString &host, bool registered);

private:
    struct ItemRecord {
        ItemAddress address;
        QString registration;
        QString owner;
    };

    void registerItemForOwner(const QString &registration, const QString &owner);
    void registerHostForOwner(const QString &host, const QString &owner);

    QHash<QString, ItemRecord> m_items;
    QHash<QString, QString> m_hosts;
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
    { return m_localWatcher ? m_localWatcher->hostRegistered() : m_hostRegistered; }
    int protocolVersion() const { return 0; }
    QString lastError() const { return m_lastError; }

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
    void resolveWatcherOwners();

private:
    void handleRegistration(const QString &registration, const QString &senderUniqueOwner = {},
                            quint64 generation = 0);
    void enumerateExternalItems();
    void selectWatcher();
    void tryOwnWatcher();
    void attachExternalWatcher(const QString &name, const QString &owner);
    void detachExternalWatcher();
    void disconnectExternalWatcher();
    void registerHostWithExternalWatcher();
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
    bool m_hostRegistered = false;
    QHash<QString, QString> m_ownerByName;
    QHash<QString, ItemAddress> m_addresses;
    QPointer<QDBusServiceWatcher> m_serviceWatcher;
    QPointer<StatusNotifierLocalWatcherObject> m_localWatcher;
    QStringList m_ownedNames;
};

} // namespace Astrea::StatusNotifier
