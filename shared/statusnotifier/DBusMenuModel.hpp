#pragma once

#include "statusnotifier/StatusNotifierTypes.hpp"

#include <QAbstractListModel>
#include <QDBusArgument>
#include <QPointer>

namespace Astrea::StatusNotifier {

class StatusNotifierIconStore;

struct DBusMenuNode {
    int id = 0;
    QString label;
    QString iconName;
    QString type;
    QString toggleType;
    QString childrenDisplay;
    int state = 0;
    bool enabled = true;
    bool visible = true;
    bool separator = false;
    QList<DBusMenuNode> children;
    QByteArray iconData;
    QString iconSource;
};

struct DBusMenuLayoutNodeWire {
    int id = 0;
    QVariantMap properties;
    QList<DBusMenuLayoutNodeWire> children;
};

struct DBusMenuLayoutReply {
    quint32 revision = 0;
    DBusMenuLayoutNodeWire root;
};

struct DBusMenuParseResult {
    quint32 revision = 0;
    DBusMenuNode root;
    QString error;

    bool ok() const { return error.isEmpty(); }
};

enum class DBusMenuLifecycleState {
    Unavailable,
    Unloaded,
    Loading,
    Ready,
    Empty,
    Error,
    Stopped,
};

DBusMenuParseResult parseMenuLayout(const QVariant &value,
                                    const DBusMenuLimits &limits = {});
DBusMenuParseResult parseMenuLayoutArgument(const QDBusArgument &argument, quint32 revision,
                                            const DBusMenuLimits &limits = {});
void registerDBusMenuMetaTypes();
QString menuLabelWithoutMnemonic(const QString &label);

class DBusMenuModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        NodeIdRole = Qt::UserRole + 1,
        LabelRole,
    IconNameRole,
        IconSourceRole,
        TypeRole,
        ToggleTypeRole,
        StateRole,
        EnabledRole,
        VisibleRole,
        SeparatorRole,
        HasChildrenRole,
        ChildrenDisplayRole,
        ChildModelRole,
    };
    Q_ENUM(Role)

    explicit DBusMenuModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setNodes(const QList<DBusMenuNode> &nodes);
    void setRoot(const DBusMenuNode &root);
    bool replaceSubtree(int parentId, const DBusMenuNode &subtree);
    bool updateNodeProperties(const QList<DBusMenuPropertyUpdate> &updates,
                              const QList<DBusMenuRemovedProperties> &removedProperties);
    bool setIconSource(int nodeId, const QString &source);
    DBusMenuNode nodeById(int nodeId) const;
    DBusMenuNode node(int row) const;
    Q_INVOKABLE QObject *childModel(int nodeId) const;
    Q_INVOKABLE void activate(int nodeId);

signals:
    void activateRequested(int nodeId);

private:
    void rebuildChildren();
    bool replaceSubtreeInNodes(int parentId, const DBusMenuNode &subtree);
    bool updatePropertiesInNodes(const QList<DBusMenuPropertyUpdate> &updates,
                                 const QList<DBusMenuRemovedProperties> &removedProperties);
    static void applyProperties(DBusMenuNode &node, const QVariantMap &properties,
                                const QStringList &removedProperties);

    QList<DBusMenuNode> m_nodes;
    QHash<int, DBusMenuModel *> m_children;
};

class DBusMenuClient final : public QObject {
    Q_OBJECT
    Q_PROPERTY(quint32 revision READ revision NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(DBusMenuLifecycleState state READ state NOTIFY changed)

public:
    DBusMenuClient(const ItemAddress &address, const QString &menuPath,
                   StatusNotifierIconStore *iconStore = nullptr,
                   QObject *parent = nullptr);

    DBusMenuModel *rootModel() const { return m_rootModel; }
    quint32 revision() const { return m_revision; }
    bool loading() const { return m_loading; }
    DBusMenuLifecycleState state() const { return m_state; }
    QString menuPath() const { return m_menuPath; }

    void load();
    Q_INVOKABLE void prepareForPresentation(int nodeId = 0);
    Q_INVOKABLE void aboutToShow(int nodeId);
    Q_INVOKABLE void activate(int nodeId);
    void stop();

signals:
    void changed();
    void actionRequested(int nodeId, const QString &eventName);
    void failed(const QString &error);

private:
    void requestLayout(int parentId = 0);
    void applyLayout(const DBusMenuParseResult &layout, int requestedParentId);
    void connectSignals();
    void disconnectSignals();
    void decorateIcons(DBusMenuNode &node);
    void updateRemoteIcon(DBusMenuNode &node);

private slots:
    void onLayoutUpdated(quint32 revision, int parentId);
    void onItemsPropertiesUpdated(const QList<DBusMenuPropertyUpdate> &updated,
                                  const QList<DBusMenuRemovedProperties> &removed);

private:
    ItemAddress m_address;
    QString m_menuPath;
    DBusMenuModel *m_rootModel = nullptr;
    StatusNotifierIconStore *m_iconStore = nullptr;
    quint32 m_revision = 0;
    quint64 m_generation = 0;
    bool m_loading = false;
    DBusMenuLifecycleState m_state = DBusMenuLifecycleState::Unloaded;
    bool m_stopped = false;
    bool m_signalsConnected = false;
};

} // namespace Astrea::StatusNotifier

Q_DECLARE_METATYPE(Astrea::StatusNotifier::DBusMenuNode)
Q_DECLARE_METATYPE(Astrea::StatusNotifier::DBusMenuLifecycleState)
Q_DECLARE_METATYPE(Astrea::StatusNotifier::DBusMenuLayoutNodeWire)
Q_DECLARE_METATYPE(Astrea::StatusNotifier::DBusMenuLayoutReply)

QDBusArgument &operator<<(QDBusArgument &argument,
                          const Astrea::StatusNotifier::DBusMenuLayoutNodeWire &node);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Astrea::StatusNotifier::DBusMenuLayoutNodeWire &node);
QDBusArgument &operator<<(QDBusArgument &argument,
                          const Astrea::StatusNotifier::DBusMenuLayoutReply &reply);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Astrea::StatusNotifier::DBusMenuLayoutReply &reply);
