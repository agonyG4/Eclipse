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

    QStringList items() const { return m_items; }
    bool hostRegistered() const { return m_hostRegistered; }
    void registerItem(const QString &registration)
    {
        const QString sender = calledFromDBus() ? message().service() : QString();
        if (!m_items.contains(registration)) {
            m_items.append(registration);
            emit itemRegistered(registration, sender);
            emit itemRegisteredForDbus(registration);
        }
    }
    void registerHost(const QString &host)
    {
        if (host.isEmpty())
            return;
        m_hostRegistered = true;
        emit hostRegisteredChanged(host);
    }

signals:
    void itemRegistered(const QString &registration, const QString &sender);
    void itemRegisteredForDbus(const QString &registration);
    void itemUnregistered(const QString &registration);
    void hostRegisteredChanged(const QString &host);

private:
    QStringList m_items;
    bool m_hostRegistered = false;
};

class StatusNotifierWatcherBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(WatcherMode mode READ mode NOTIFY stateChanged)
    Q_PROPERTY(QString watcherName READ watcherName NOTIFY stateChanged)
    Q_PROPERTY(QString watcherOwner READ watcherOwner NOTIFY stateChanged)
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
    bool hostRegistered() const { return m_hostRegistered; }
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
    void onLocalHostRegistered(const QString &host);
    void resolveWatcherOwners();

private:
    void handleRegistration(const QString &registration, const QString &senderUniqueOwner = {});
    void enumerateExternalItems();
    void selectWatcher();
    void tryOwnWatcher();
    void attachExternalWatcher(const QString &name, const QString &owner);
    void registerHostWithExternalWatcher();
    void registerItemOwnerWatcher(const ItemAddress &address);
    void setError(const QString &error);

    WatcherMode m_mode = WatcherMode::Unavailable;
    QString m_watcherName;
    QString m_watcherOwner;
    QString m_lastError;
    bool m_hostRegistered = false;
    bool m_started = false;
    int m_resolutionPending = 0;
    QHash<QString, QString> m_ownerByName;
    QHash<QString, QString> m_keyByService;
    QHash<QString, ItemAddress> m_addresses;
    QPointer<QDBusServiceWatcher> m_serviceWatcher;
    QPointer<StatusNotifierLocalWatcherObject> m_localWatcher;
    QStringList m_ownedNames;
};

} // namespace Astrea::StatusNotifier
