#include "statusnotifier/DBusMenuModel.hpp"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusInterface>
#include <QDateTime>
#include <QVariantList>

#include <algorithm>

namespace Astrea::StatusNotifier {
namespace {

bool parseNodeVariant(const QVariant &value, DBusMenuNode &node, const DBusMenuLimits &limits,
                      int depth, int &nodeCount, QString *errorOut)
{
    if (depth > limits.maxDepth) {
        *errorOut = QStringLiteral("DBusMenu depth exceeds the safety limit");
        return false;
    }
    if (++nodeCount > limits.maxNodes) {
        *errorOut = QStringLiteral("DBusMenu node count exceeds the safety limit");
        return false;
    }
    const QVariantMap map = value.toMap();
    if (map.isEmpty()) {
        *errorOut = QStringLiteral("DBusMenu node is not a map");
        return false;
    }
    node.id = map.value(QStringLiteral("id")).toInt();
    const QVariantMap properties = map.value(QStringLiteral("properties")).toMap().isEmpty()
        ? map : map.value(QStringLiteral("properties")).toMap();
    const QString rawLabel = properties.value(QStringLiteral("label")).toString();
    node.label = menuLabelWithoutMnemonic(rawLabel);
    node.iconName = properties.value(QStringLiteral("icon-name")).toString();
    node.type = properties.value(QStringLiteral("type")).toString();
    node.toggleType = properties.value(QStringLiteral("toggle-type")).toString();
    node.childrenDisplay = properties.value(QStringLiteral("children-display")).toString();
    node.state = properties.value(QStringLiteral("toggle-state")).toInt();
    node.enabled = properties.value(QStringLiteral("enabled"), true).toBool();
    node.visible = properties.value(QStringLiteral("visible"), true).toBool();
    node.separator = node.type == QStringLiteral("separator");
    const QVariant iconData = properties.value(QStringLiteral("icon-data"));
    if (iconData.isValid() && iconData.toByteArray().size() > limits.maxIconDataBytes) {
        *errorOut = QStringLiteral("DBusMenu icon data exceeds the safety limit");
        return false;
    }
    const QVariantList children = map.value(QStringLiteral("children")).toList();
    if (children.size() > limits.maxChildren) {
        *errorOut = QStringLiteral("DBusMenu child count exceeds the safety limit");
        return false;
    }
    node.children.clear();
    for (const QVariant &childValue : children) {
        DBusMenuNode child;
        if (!parseNodeVariant(childValue, child, limits, depth + 1, nodeCount, errorOut))
            return false;
        node.children.append(child);
    }
    return true;
}

} // namespace

QString menuLabelWithoutMnemonic(const QString &label)
{
    QString result;
    result.reserve(label.size());
    for (qsizetype i = 0; i < label.size(); ++i) {
        const QChar ch = label.at(i);
        if (ch == QLatin1Char('_') && i + 1 < label.size()) {
            if (label.at(i + 1) == QLatin1Char('_')) {
                result.append(QLatin1Char('_'));
                ++i;
                continue;
            }
            ++i;
            result.append(label.at(i));
            continue;
        }
        result.append(ch);
    }
    return result;
}

DBusMenuParseResult parseMenuLayout(const QVariant &value, const DBusMenuLimits &limits)
{
    DBusMenuParseResult result;
    const QVariantList list = value.toList();
    QVariant nodeValue = value;
    if (list.size() >= 2) {
        result.revision = list.at(0).toUInt();
        nodeValue = list.at(1);
    }
    int nodeCount = 0;
    if (!parseNodeVariant(nodeValue, result.root, limits, 0, nodeCount, &result.error))
        result.root = {};
    return result;
}

DBusMenuParseResult parseMenuLayoutArgument(const QDBusArgument &argument, quint32 revision,
                                            const DBusMenuLimits &limits)
{
    DBusMenuParseResult result = parseMenuLayout(argument.asVariant(), limits);
    result.revision = revision;
    return result;
}

DBusMenuModel::DBusMenuModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DBusMenuModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_nodes.size();
}

QVariant DBusMenuModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_nodes.size())
        return {};
    const DBusMenuNode &node = m_nodes.at(index.row());
    switch (role) {
    case NodeIdRole: return node.id;
    case LabelRole: return node.label;
    case IconNameRole: return node.iconName;
    case TypeRole: return node.type;
    case ToggleTypeRole: return node.toggleType;
    case StateRole: return node.state;
    case EnabledRole: return node.enabled;
    case VisibleRole: return node.visible;
    case SeparatorRole: return node.separator;
    case HasChildrenRole: return !node.children.isEmpty();
    case ChildrenDisplayRole: return node.childrenDisplay;
    case ChildModelRole: return QVariant::fromValue(static_cast<QObject *>(childModel(node.id)));
    default: return {};
    }
}

QHash<int, QByteArray> DBusMenuModel::roleNames() const
{
    return {{NodeIdRole, "nodeId"}, {LabelRole, "label"}, {IconNameRole, "iconName"},
            {TypeRole, "type"}, {ToggleTypeRole, "toggleType"}, {StateRole, "state"},
            {EnabledRole, "enabled"}, {VisibleRole, "visible"}, {SeparatorRole, "separator"},
            {HasChildrenRole, "hasChildren"}, {ChildrenDisplayRole, "childrenDisplay"},
            {ChildModelRole, "childModel"}};
}

void DBusMenuModel::setNodes(const QList<DBusMenuNode> &nodes)
{
    beginResetModel();
    m_nodes = nodes;
    for (auto *child : std::as_const(m_children))
        child->deleteLater();
    m_children.clear();
    endResetModel();
    rebuildChildren();
}

void DBusMenuModel::setRoot(const DBusMenuNode &root)
{
    setNodes(root.children);
}

DBusMenuNode DBusMenuModel::node(int row) const
{
    return row >= 0 && row < m_nodes.size() ? m_nodes.at(row) : DBusMenuNode{};
}

QObject *DBusMenuModel::childModel(int nodeId) const
{
    return m_children.value(nodeId);
}

void DBusMenuModel::activate(int nodeId)
{
    emit activateRequested(nodeId);
}

void DBusMenuModel::rebuildChildren()
{
    for (const DBusMenuNode &node : m_nodes) {
        if (node.children.isEmpty())
            continue;
        auto *child = new DBusMenuModel(const_cast<DBusMenuModel *>(this));
        child->setNodes(node.children);
        connect(child, &DBusMenuModel::activateRequested, this,
                &DBusMenuModel::activateRequested);
        m_children.insert(node.id, child);
    }
}

DBusMenuClient::DBusMenuClient(const ItemAddress &address, const QString &menuPath,
                               QObject *parent)
    : QObject(parent), m_address(address), m_menuPath(menuPath), m_rootModel(new DBusMenuModel(this))
{
    connect(m_rootModel, &DBusMenuModel::activateRequested, this,
            &DBusMenuClient::activate);
}

void DBusMenuClient::load()
{
    if (m_stopped || m_menuPath.isEmpty() || !m_address.isValid())
        return;
    requestLayout();
}

void DBusMenuClient::aboutToShow(int nodeId)
{
    if (m_stopped || m_menuPath.isEmpty())
        return;
    QDBusInterface iface(m_address.service, m_menuPath, QStringLiteral("com.canonical.dbusmenu"),
                         QDBusConnection::sessionBus());
    QDBusPendingCall pending = iface.asyncCall(QStringLiteral("AboutToShow"), nodeId);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    const quint64 generation = m_generation;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, nodeId] {
        const QDBusMessage reply = watcher->reply();
        watcher->deleteLater();
        if (generation != m_generation || reply.type() == QDBusMessage::ErrorMessage)
            return;
        const QVariantList args = reply.arguments();
        if (!args.isEmpty() && args.constFirst().toBool())
            requestLayout(nodeId);
    });
}

void DBusMenuClient::activate(int nodeId)
{
    if (m_stopped || m_menuPath.isEmpty())
        return;
    QDBusInterface iface(m_address.service, m_menuPath, QStringLiteral("com.canonical.dbusmenu"),
                         QDBusConnection::sessionBus());
    iface.asyncCall(QStringLiteral("Event"), nodeId, QStringLiteral("clicked"), QVariant(),
                    quint32(QDateTime::currentMSecsSinceEpoch() / 1000));
    emit actionRequested(nodeId, QStringLiteral("clicked"));
}

void DBusMenuClient::stop()
{
    m_stopped = true;
    ++m_generation;
}

void DBusMenuClient::requestLayout(int parentId)
{
    if (m_stopped || m_menuPath.isEmpty())
        return;
    m_loading = true;
    emit changed();
    const quint64 generation = ++m_generation;
    QDBusInterface iface(m_address.service, m_menuPath, QStringLiteral("com.canonical.dbusmenu"),
                         QDBusConnection::sessionBus());
    QDBusPendingCall pending = iface.asyncCall(QStringLiteral("GetLayout"), parentId, -1,
                                               QStringList());
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation] {
        const QDBusMessage reply = watcher->reply();
        watcher->deleteLater();
        if (generation != m_generation || m_stopped)
            return;
        m_loading = false;
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().size() < 2) {
            const QString error = reply.errorMessage().isEmpty()
                ? QStringLiteral("DBusMenu GetLayout returned no layout") : reply.errorMessage();
            emit failed(error);
            emit changed();
            return;
        }
        const QVariantList args = reply.arguments();
        const quint32 revision = args.at(0).toUInt();
        const auto argument = args.at(1).value<QDBusArgument>();
        const auto result = parseMenuLayoutArgument(argument, revision);
        if (!result.ok()) {
            emit failed(result.error);
            emit changed();
            return;
        }
        applyLayout(result);
    });
}

void DBusMenuClient::applyLayout(const DBusMenuParseResult &layout)
{
    m_revision = layout.revision;
    m_rootModel->setRoot(layout.root);
    emit changed();
}

} // namespace Astrea::StatusNotifier
