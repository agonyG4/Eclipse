#pragma once

#include "statusnotifier/StatusNotifierTypes.hpp"

#include <QAbstractListModel>
#include <QDBusArgument>
#include <QPointer>

namespace Astrea::StatusNotifier {

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
};

struct DBusMenuParseResult {
    quint32 revision = 0;
    DBusMenuNode root;
    QString error;

    bool ok() const { return error.isEmpty(); }
};

DBusMenuParseResult parseMenuLayout(const QVariant &value,
                                    const DBusMenuLimits &limits = {});
DBusMenuParseResult parseMenuLayoutArgument(const QDBusArgument &argument, quint32 revision,
                                            const DBusMenuLimits &limits = {});
QString menuLabelWithoutMnemonic(const QString &label);

class DBusMenuModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        NodeIdRole = Qt::UserRole + 1,
        LabelRole,
        IconNameRole,
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
    DBusMenuNode node(int row) const;
    Q_INVOKABLE QObject *childModel(int nodeId) const;
    Q_INVOKABLE void activate(int nodeId);

signals:
    void activateRequested(int nodeId);

private:
    void rebuildChildren();

    QList<DBusMenuNode> m_nodes;
    QHash<int, DBusMenuModel *> m_children;
};

class DBusMenuClient final : public QObject {
    Q_OBJECT
    Q_PROPERTY(quint32 revision READ revision NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)

public:
    DBusMenuClient(const ItemAddress &address, const QString &menuPath,
                   QObject *parent = nullptr);

    DBusMenuModel *rootModel() const { return m_rootModel; }
    quint32 revision() const { return m_revision; }
    bool loading() const { return m_loading; }
    QString menuPath() const { return m_menuPath; }

    void load();
    Q_INVOKABLE void aboutToShow(int nodeId);
    Q_INVOKABLE void activate(int nodeId);
    void stop();

signals:
    void changed();
    void actionRequested(int nodeId, const QString &eventName);
    void failed(const QString &error);

private:
    void requestLayout(int parentId = 0);
    void applyLayout(const DBusMenuParseResult &layout);

    ItemAddress m_address;
    QString m_menuPath;
    DBusMenuModel *m_rootModel = nullptr;
    quint32 m_revision = 0;
    quint64 m_generation = 0;
    bool m_loading = false;
    bool m_stopped = false;
};

} // namespace Astrea::StatusNotifier

Q_DECLARE_METATYPE(Astrea::StatusNotifier::DBusMenuNode)
