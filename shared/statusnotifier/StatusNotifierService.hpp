#pragma once

#include "statusnotifier/StatusNotifierTypes.hpp"

#include <QAbstractItemModel>
#include <QJsonObject>
#include <QObject>
#include <QHash>
#include <memory>

namespace Astrea::StatusNotifier {
class DBusMenuClient;
class DBusMenuModel;
class StatusNotifierIconStore;
class StatusNotifierItemModel;
class StatusNotifierItemProxy;
class StatusNotifierWatcherBridge;

class StatusNotifierService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *itemModel READ itemModel CONSTANT)
    Q_PROPERTY(WatcherMode watcherMode READ watcherMode NOTIFY stateChanged)
    Q_PROPERTY(QString watcherName READ watcherName NOTIFY stateChanged)
    Q_PROPERTY(QString watcherOwner READ watcherOwner NOTIFY stateChanged)
    Q_PROPERTY(QString hostServiceName READ hostServiceName NOTIFY stateChanged)
    Q_PROPERTY(bool hostRegistered READ hostRegistered NOTIFY stateChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(int menuClientCount READ menuClientCount NOTIFY stateChanged)
    Q_PROPERTY(quint64 presentationRevision READ presentationRevision NOTIFY presentationRevisionChanged)

public:
    explicit StatusNotifierService(QObject *parent = nullptr);
    ~StatusNotifierService() override;

    void initialize();
    void start();
    void stop();
    bool isStarted() const { return m_started; }

    QAbstractItemModel *itemModel() const;
    StatusNotifierItemModel *typedItemModel() const { return m_model.get(); }
    StatusNotifierIconStore *iconStore() const { return m_iconStore.get(); }
    WatcherMode watcherMode() const;
    QString watcherName() const;
    QString watcherOwner() const;
    QString hostServiceName() const;
    bool hostRegistered() const;
    int itemCount() const;
    QString lastError() const;
    int menuClientCount() const;
    quint64 presentationRevision() const { return m_presentationRevision; }
    QJsonObject healthJson() const;

    Q_INVOKABLE bool hasMenuForItem(const QString &itemKey) const;
    Q_INVOKABLE QObject *menuModelForItem(const QString &itemKey) const;
    Q_INVOKABLE int menuStateForItem(const QString &itemKey) const;
    Q_INVOKABLE QString displayTitleForItem(const QString &itemKey) const;
    Q_INVOKABLE QString iconSourceForItem(const QString &itemKey) const;
    Q_INVOKABLE QString tooltipTitleForItem(const QString &itemKey) const;
    Q_INVOKABLE QString tooltipDescriptionForItem(const QString &itemKey) const;
    Q_INVOKABLE void activate(const QString &itemKey, int x, int y);
    Q_INVOKABLE void secondaryActivate(const QString &itemKey, int x, int y);
    Q_INVOKABLE void contextMenu(const QString &itemKey, int x, int y);
    Q_INVOKABLE void scroll(const QString &itemKey, int delta, const QString &orientation);
    Q_INVOKABLE void openMenu(const QString &itemKey);
    Q_INVOKABLE void prepareMenuForPresentation(const QString &itemKey, int nodeId = 0);
    Q_INVOKABLE void aboutToShowMenu(const QString &itemKey, int nodeId);

    void upsertTestItem(const ItemSnapshot &snapshot);
    void removeTestItem(const QString &key);

signals:
    void stateChanged();
    void itemChanged(const QString &itemKey);
    void itemRemoved(const QString &itemKey);
    void menuClientChanged(const QString &itemKey);
    void menuContentChanged(const QString &itemKey);
    void presentationRevisionChanged();
    void healthWarning(const QString &warning);

private slots:
    void onItemRegistered(const ItemAddress &address);
    void onItemUnregistered(const QString &key);
    void onItemOwnerVanished(const QString &key, const QString &uniqueOwner);
    void onSnapshotChanged(const ItemSnapshot &snapshot);
    void onProxyMenuPathChanged(const QString &key, const QString &menuPath);
    void onWatcherStateChanged();

private:
    void ensureProxy(const ItemAddress &address);
    void removeItem(const QString &key);
    void updateSnapshot(const ItemSnapshot &snapshot);
    void updateMenu(const QString &key, const QString &menuPath);
    void bumpPresentationRevision();

    std::unique_ptr<StatusNotifierWatcherBridge> m_watcher;
    std::unique_ptr<StatusNotifierIconStore> m_iconStore;
    std::unique_ptr<StatusNotifierItemModel> m_model;
    QHash<QString, StatusNotifierItemProxy *> m_proxies;
    QHash<QString, DBusMenuClient *> m_menus;
    QString m_lastHealthWarning;
    quint64 m_nextGeneration = 1;
    quint64 m_presentationRevision = 0;
    bool m_initialized = false;
    bool m_started = false;
    bool m_stopping = false;
};

} // namespace Astrea::StatusNotifier
