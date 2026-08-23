#include "statusnotifier/DBusMenuModel.hpp"

#include "statusnotifier/StatusNotifierIconStore.hpp"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusInterface>
#include <QDBusVariant>
#include <QBuffer>
#include <QDateTime>
#include <QImage>
#include <QImageReader>
#include <QVariantList>

#include <algorithm>

namespace Astrea::StatusNotifier {
namespace {

QVariant unwrap(const QVariant &value)
{
    if (value.canConvert<QDBusVariant>())
        return value.value<QDBusVariant>().variant();
    return value;
}

bool parseWireVariant(const QVariant &value, DBusMenuLayoutNodeWire &node, QString *errorOut);
QVariantMap normalizedProperties(const QVariantMap &properties);

bool parseWireArgument(const QDBusArgument &argument, DBusMenuLayoutNodeWire &node,
                       QString *errorOut)
{
    Q_UNUSED(errorOut)
    const QDBusArgument copy = argument;
    copy >> node;
    return true;
}

bool isSafeIconData(const QByteArray &data, const DBusMenuLimits &limits, QString *errorOut)
{
    if (data.isEmpty())
        return true;
    if (limits.maxIconDataBytes < 0 || data.size() > limits.maxIconDataBytes) {
        if (errorOut)
            *errorOut = QStringLiteral("DBusMenu icon data exceeds the safety limit");
        return false;
    }
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    const QSize size = reader.size();
    if (!reader.canRead() || !size.isValid() || size.width() <= 0 || size.height() <= 0
        || size.width() > limits.maxIconDimension || size.height() > limits.maxIconDimension
        || qint64(size.width()) * qint64(size.height()) > limits.maxIconPixels) {
        if (errorOut)
            *errorOut = QStringLiteral("DBusMenu icon-data is not a safe PNG image");
        return false;
    }
    return true;
}

bool parseWireVariant(const QVariant &value, DBusMenuLayoutNodeWire &node, QString *errorOut)
{
    const QVariant unwrapped = unwrap(value);
    if (unwrapped.canConvert<DBusMenuLayoutNodeWire>()) {
        node = unwrapped.value<DBusMenuLayoutNodeWire>();
        return true;
    }
    if (unwrapped.canConvert<QDBusArgument>())
        return parseWireArgument(unwrapped.value<QDBusArgument>(), node, errorOut);

    const QVariantMap map = unwrapped.toMap();
    if (!map.isEmpty()) {
        node.id = map.value(QStringLiteral("id")).toInt();
        node.properties = map.value(QStringLiteral("properties")).toMap();
        if (node.properties.isEmpty())
            node.properties = map;
        const QVariantList children = unwrap(map.value(QStringLiteral("children"))).toList();
        for (const QVariant &childValue : children) {
            DBusMenuLayoutNodeWire child;
            if (!parseWireVariant(childValue, child, errorOut))
                return false;
            node.children.append(child);
        }
        return true;
    }

    const QVariantList list = unwrapped.toList();
    if (list.size() >= 3) {
        node.id = unwrap(list.at(0)).toInt();
        node.properties = unwrap(list.at(1)).toMap();
        for (const QVariant &childValue : unwrap(list.at(2)).toList()) {
            DBusMenuLayoutNodeWire child;
            if (!parseWireVariant(childValue, child, errorOut))
                return false;
            node.children.append(child);
        }
        return true;
    }

    if (errorOut)
        *errorOut = QStringLiteral("DBusMenu layout node is not a supported structure");
    return false;
}

void applyNodeProperties(DBusMenuNode &node, const QVariantMap &rawProperties,
                         const QStringList &removedProperties, const DBusMenuLimits &limits)
{
    const QVariantMap properties = normalizedProperties(rawProperties);
    const auto removed = [&removedProperties](const QString &name) {
        return removedProperties.contains(name);
    };
    const bool iconChanged = properties.contains(QStringLiteral("icon-name"))
        || properties.contains(QStringLiteral("icon-data"))
        || removed(QStringLiteral("icon-name")) || removed(QStringLiteral("icon-data"));

    if (properties.contains(QStringLiteral("label")))
        node.label = menuLabelWithoutMnemonic(properties.value(QStringLiteral("label")).toString());
    else if (removed(QStringLiteral("label")))
        node.label.clear();
    if (limits.maxLabelLength >= 0 && node.label.size() > limits.maxLabelLength)
        node.label.truncate(limits.maxLabelLength);

    if (properties.contains(QStringLiteral("icon-name")))
        node.iconName = properties.value(QStringLiteral("icon-name")).toString();
    else if (removed(QStringLiteral("icon-name")))
        node.iconName.clear();
    if (properties.contains(QStringLiteral("icon-data"))) {
        const QByteArray iconData = properties.value(QStringLiteral("icon-data")).toByteArray();
        QString ignored;
        node.iconData = isSafeIconData(iconData, limits, &ignored) ? iconData : QByteArray();
    } else if (removed(QStringLiteral("icon-data"))) {
        node.iconData.clear();
    }
    if (properties.contains(QStringLiteral("type")))
        node.type = properties.value(QStringLiteral("type")).toString();
    else if (removed(QStringLiteral("type")))
        node.type.clear();
    if (properties.contains(QStringLiteral("toggle-type")))
        node.toggleType = properties.value(QStringLiteral("toggle-type")).toString();
    else if (removed(QStringLiteral("toggle-type")))
        node.toggleType.clear();
    if (properties.contains(QStringLiteral("toggle-state")))
        node.state = properties.value(QStringLiteral("toggle-state")).toInt();
    else if (removed(QStringLiteral("toggle-state")))
        node.state = 0;
    if (properties.contains(QStringLiteral("children-display")))
        node.childrenDisplay = properties.value(QStringLiteral("children-display")).toString();
    else if (removed(QStringLiteral("children-display")))
        node.childrenDisplay.clear();
    if (properties.contains(QStringLiteral("enabled")))
        node.enabled = properties.value(QStringLiteral("enabled")).toBool();
    else if (removed(QStringLiteral("enabled")))
        node.enabled = true;
    if (properties.contains(QStringLiteral("visible")))
        node.visible = properties.value(QStringLiteral("visible")).toBool();
    else if (removed(QStringLiteral("visible")))
        node.visible = true;

    node.separator = node.type == QStringLiteral("separator");
    if (iconChanged)
        node.iconSource.clear();
}

bool parseNodeWire(const DBusMenuLayoutNodeWire &wire, DBusMenuNode &node,
                  const DBusMenuLimits &limits, int depth, int &nodeCount, QString *errorOut)
{
    if (depth > limits.maxDepth) {
        *errorOut = QStringLiteral("DBusMenu depth exceeds the safety limit");
        return false;
    }
    if (++nodeCount > limits.maxNodes) {
        *errorOut = QStringLiteral("DBusMenu node count exceeds the safety limit");
        return false;
    }
    if (wire.children.size() > limits.maxChildren) {
        *errorOut = QStringLiteral("DBusMenu child count exceeds the safety limit");
        return false;
    }

    node = {};
    node.id = wire.id;
    applyNodeProperties(node, wire.properties, {}, limits);
    node.children.clear();
    for (const auto &childWire : wire.children) {
        DBusMenuNode child;
        if (!parseNodeWire(childWire, child, limits, depth + 1, nodeCount, errorOut))
            return false;
        node.children.append(child);
    }
    return true;
}

bool parseNodeVariant(const QVariant &value, DBusMenuNode &node, const DBusMenuLimits &limits,
                      int depth, int &nodeCount, QString *errorOut)
{
    DBusMenuLayoutNodeWire wire;
    if (!parseWireVariant(value, wire, errorOut))
        return false;
    return parseNodeWire(wire, node, limits, depth, nodeCount, errorOut);
}

QVariantMap normalizedProperties(const QVariantMap &properties)
{
    QVariantMap normalized;
    for (auto it = properties.cbegin(); it != properties.cend(); ++it)
        normalized.insert(it.key(), unwrap(it.value()));
    return normalized;
}

void updateIconSource(StatusNotifierIconStore *store, const QString &key,
                      const QByteArray &iconData, const DBusMenuLimits &limits)
{
    if (!store)
        return;
    QImage image;
    QString ignored;
    if (!iconData.isEmpty() && isSafeIconData(iconData, limits, &ignored))
        image = QImage::fromData(iconData);
    store->updateAuxiliaryImage(key, image);
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

void registerDBusMenuMetaTypes()
{
    static const bool registered = [] {
        qDBusRegisterMetaType<DBusMenuLayoutNodeWire>();
        qDBusRegisterMetaType<DBusMenuLayoutReply>();
        return true;
    }();
    Q_UNUSED(registered)
}

DBusMenuParseResult parseMenuLayout(const QVariant &value, const DBusMenuLimits &limits)
{
    DBusMenuParseResult result;
    const QVariant unwrapped = unwrap(value);
    if (unwrapped.canConvert<DBusMenuLayoutReply>()) {
        const auto reply = unwrapped.value<DBusMenuLayoutReply>();
        result.revision = reply.revision;
        int nodeCount = 0;
        if (!parseNodeWire(reply.root, result.root, limits, 0, nodeCount, &result.error))
            result.root = {};
        return result;
    }
    if (unwrapped.canConvert<QDBusArgument>())
        return parseMenuLayoutArgument(unwrapped.value<QDBusArgument>(), 0, limits);

    QVariant nodeValue = unwrapped;
    const QVariantList list = unwrapped.toList();
    if (list.size() >= 2) {
        result.revision = unwrap(list.at(0)).toUInt();
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
    registerDBusMenuMetaTypes();
    DBusMenuParseResult result;
    DBusMenuLayoutReply wire;
    const QDBusArgument copy = argument;
    copy >> wire;
    result.revision = wire.revision == 0 ? revision : wire.revision;
    int nodeCount = 0;
    if (!parseNodeWire(wire.root, result.root, limits, 0, nodeCount, &result.error))
        result.root = {};
    return result;
}

DBusMenuModel::DBusMenuModel(QObject *parent)
    : DBusMenuModel(DBusMenuLimits{}, parent)
{
}

DBusMenuModel::DBusMenuModel(const DBusMenuLimits &limits, QObject *parent)
    : QAbstractListModel(parent), m_limits(limits)
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
    case IconSourceRole: return node.iconSource;
    case TypeRole: return node.type;
    case ToggleTypeRole: return node.toggleType;
    case StateRole: return node.state;
    case EnabledRole: return node.enabled;
    case VisibleRole: return node.visible;
    case SeparatorRole: return node.separator;
    case HasChildrenRole: return !node.children.isEmpty()
        || node.childrenDisplay == QStringLiteral("submenu");
    case ChildrenDisplayRole: return node.childrenDisplay;
    case ChildModelRole: return QVariant::fromValue(static_cast<QObject *>(childModel(node.id)));
    default: return {};
    }
}

QHash<int, QByteArray> DBusMenuModel::roleNames() const
{
    return {{NodeIdRole, "nodeId"}, {LabelRole, "label"}, {IconNameRole, "iconName"},
            {IconSourceRole, "iconSource"}, {TypeRole, "type"},
            {ToggleTypeRole, "toggleType"}, {StateRole, "state"}, {EnabledRole, "enabled"},
            {VisibleRole, "visible"}, {SeparatorRole, "separator"},
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

bool DBusMenuModel::setIconSource(int nodeId, const QString &source)
{
    for (int row = 0; row < m_nodes.size(); ++row) {
        if (m_nodes.at(row).id != nodeId)
            continue;
        if (m_nodes[row].iconSource == source)
            return true;
        m_nodes[row].iconSource = source;
        emit dataChanged(index(row), index(row), {IconSourceRole});
        return true;
    }
    for (auto *child : std::as_const(m_children)) {
        if (child && child->setIconSource(nodeId, source))
            return true;
    }
    return false;
}

DBusMenuNode DBusMenuModel::nodeById(int nodeId) const
{
    for (const DBusMenuNode &node : m_nodes) {
        if (node.id == nodeId)
            return node;
    }
    for (auto *child : std::as_const(m_children)) {
        if (child) {
            const DBusMenuNode node = child->nodeById(nodeId);
            if (node.id == nodeId)
                return node;
        }
    }
    return {};
}

bool DBusMenuModel::replaceSubtree(int parentId, const DBusMenuNode &subtree)
{
    if (parentId == 0) {
        setRoot(subtree);
        return true;
    }
    return replaceSubtreeInNodes(parentId, subtree);
}

bool DBusMenuModel::replaceSubtreeInNodes(int parentId, const DBusMenuNode &subtree)
{
    for (int row = 0; row < m_nodes.size(); ++row) {
        if (m_nodes.at(row).id != parentId)
            continue;
        m_nodes[row] = subtree;
        const auto childIt = m_children.find(parentId);
        if (subtree.children.isEmpty()) {
            if (childIt != m_children.end()) {
                childIt.value()->deleteLater();
                m_children.erase(childIt);
            }
        } else if (childIt != m_children.end()) {
            childIt.value()->setNodes(subtree.children);
        } else {
            auto *child = new DBusMenuModel(m_limits, this);
            child->setNodes(subtree.children);
            connect(child, &DBusMenuModel::activateRequested, this,
                    &DBusMenuModel::activateRequested);
            m_children.insert(parentId, child);
        }
        emit dataChanged(index(row), index(row));
        return true;
    }
    for (auto *child : std::as_const(m_children)) {
        if (child && child->replaceSubtreeInNodes(parentId, subtree))
            return true;
    }
    return false;
}

bool DBusMenuModel::updateNodeProperties(
    const QList<DBusMenuPropertyUpdate> &updates,
    const QList<DBusMenuRemovedProperties> &removedProperties)
{
    return updatePropertiesInNodes(updates, removedProperties);
}

bool DBusMenuModel::updatePropertiesInNodes(
    const QList<DBusMenuPropertyUpdate> &updates,
    const QList<DBusMenuRemovedProperties> &removedProperties)
{
    bool found = false;
    for (int row = 0; row < m_nodes.size(); ++row) {
        DBusMenuNode &node = m_nodes[row];
        bool changed = false;
        for (const auto &update : updates) {
            if (update.id != node.id)
                continue;
            applyProperties(node, update.properties, {}, m_limits);
            changed = true;
        }
        for (const auto &removed : removedProperties) {
            if (removed.id != node.id)
                continue;
            applyProperties(node, {}, removed.properties, m_limits);
            changed = true;
        }
        if (changed) {
            found = true;
            emit dataChanged(index(row), index(row));
        }
    }
    for (auto *child : std::as_const(m_children)) {
        if (child)
            found = child->updatePropertiesInNodes(updates, removedProperties) || found;
    }
    return found;
}

void DBusMenuModel::applyProperties(DBusMenuNode &node, const QVariantMap &rawProperties,
                                    const QStringList &removedProperties,
                                    const DBusMenuLimits &limits)
{
    applyNodeProperties(node, rawProperties, removedProperties, limits);
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
        auto *child = new DBusMenuModel(m_limits, const_cast<DBusMenuModel *>(this));
        child->setNodes(node.children);
        connect(child, &DBusMenuModel::activateRequested, this,
                &DBusMenuModel::activateRequested);
        m_children.insert(node.id, child);
    }
}

DBusMenuClient::DBusMenuClient(const ItemAddress &address, const QString &menuPath,
                               StatusNotifierIconStore *iconStore, quint64 itemGeneration,
                               QObject *parent)
    : QObject(parent), m_address(address), m_menuPath(menuPath),
      m_itemGeneration(itemGeneration),
      m_rootModel(new DBusMenuModel(m_limits, this)), m_iconStore(iconStore)
{
    registerStatusNotifierDBusMetaTypes();
    registerDBusMenuMetaTypes();
    connect(m_rootModel, &DBusMenuModel::activateRequested, this,
            &DBusMenuClient::activate);
}

void DBusMenuClient::load()
{
    prepareForPresentation();
}

void DBusMenuClient::prepareForPresentation(int nodeId)
{
    if (m_stopped || m_menuPath.isEmpty())
        return;
    m_stopped = false;
    connectSignals();
    const bool initialRoot = nodeId == 0
        && (m_state == DBusMenuLifecycleState::Unloaded
            || m_state == DBusMenuLifecycleState::Error);
    m_state = DBusMenuLifecycleState::Loading;
    m_loading = true;
    emit changed();
    QDBusInterface iface(m_address.service, m_menuPath, QStringLiteral("com.canonical.dbusmenu"),
                         QDBusConnection::sessionBus());
    QDBusPendingCall pending = iface.asyncCall(QStringLiteral("AboutToShow"), nodeId);
    const quint64 generation = m_generation;
    const quint64 itemGeneration = m_itemGeneration;
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, itemGeneration, nodeId, initialRoot] {
        const QDBusMessage reply = watcher->reply();
        watcher->deleteLater();
        if (generation != m_generation || itemGeneration != m_itemGeneration || m_stopped)
            return;
        const bool failed = reply.type() == QDBusMessage::ErrorMessage;
        const QVariantList args = reply.arguments();
        const bool needUpdate = !failed && !args.isEmpty() && unwrap(args.constFirst()).toBool();
        if (initialRoot || needUpdate)
            requestLayout(nodeId);
        else {
            m_loading = false;
            m_state = m_rootModel->rowCount() == 0 ? DBusMenuLifecycleState::Empty
                                                   : DBusMenuLifecycleState::Ready;
            emit changed();
        }
    });
}

void DBusMenuClient::aboutToShow(int nodeId)
{
    prepareForPresentation(nodeId);
}

void DBusMenuClient::activate(int nodeId)
{
    if (m_stopped || m_menuPath.isEmpty())
        return;
    QVariant eventData;
    eventData.setValue(QDBusVariant(QVariant(QString())));
    QDBusMessage event = QDBusMessage::createMethodCall(
        m_address.service, m_menuPath, QStringLiteral("com.canonical.dbusmenu"),
        QStringLiteral("Event"));
    event.setArguments({nodeId, QStringLiteral("clicked"), eventData,
                        quint32(QDateTime::currentMSecsSinceEpoch() / 1000)});
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(event), this);
    const quint64 generation = m_generation;
    const quint64 itemGeneration = m_itemGeneration;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, itemGeneration] {
        const QDBusMessage reply = watcher->reply();
        watcher->deleteLater();
        if (generation != m_generation || itemGeneration != m_itemGeneration || m_stopped)
            return;
        if (reply.type() == QDBusMessage::ErrorMessage)
            emit failed(reply.errorMessage());
    });
    emit actionRequested(nodeId, QStringLiteral("clicked"));
}

void DBusMenuClient::stop()
{
    if (m_stopped)
        return;
    m_stopped = true;
    ++m_generation;
    disconnectSignals();
    if (m_iconStore)
        m_iconStore->clearAuxiliaryImages(QStringLiteral("menu:%1:").arg(m_address.key()));
    m_state = DBusMenuLifecycleState::Stopped;
    m_loading = false;
    emit changed();
}

void DBusMenuClient::requestLayout(int parentId)
{
    if (m_stopped || m_menuPath.isEmpty())
        return;
    m_loading = true;
    m_state = DBusMenuLifecycleState::Loading;
    emit changed();
    const quint64 generation = m_generation;
    QDBusInterface iface(m_address.service, m_menuPath, QStringLiteral("com.canonical.dbusmenu"),
                         QDBusConnection::sessionBus());
    QDBusPendingCall pending = iface.asyncCall(QStringLiteral("GetLayout"), parentId, -1,
                                               QStringList());
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    const quint64 itemGeneration = m_itemGeneration;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation, itemGeneration, parentId] {
        const QDBusMessage reply = watcher->reply();
        watcher->deleteLater();
        if (generation != m_generation || itemGeneration != m_itemGeneration || m_stopped)
            return;
        m_loading = false;
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
            const QString error = reply.errorMessage().isEmpty()
                ? QStringLiteral("DBusMenu GetLayout returned no layout") : reply.errorMessage();
            emit failed(error);
            m_state = DBusMenuLifecycleState::Error;
            emit changed();
            return;
        }
        DBusMenuParseResult result;
        const QVariantList args = reply.arguments();
        if (args.size() == 1) {
            const QVariant only = args.constFirst();
            if (only.canConvert<DBusMenuLayoutReply>())
                result = parseMenuLayout(only, m_limits);
            else if (only.canConvert<QDBusArgument>())
                result = parseMenuLayoutArgument(only.value<QDBusArgument>(), 0, m_limits);
            else
                result = parseMenuLayout(only, m_limits);
        } else {
            const quint32 revision = unwrap(args.at(0)).toUInt();
            const QVariant layoutValue = args.at(1);
            if (layoutValue.canConvert<QDBusArgument>())
                result = parseMenuLayoutArgument(layoutValue.value<QDBusArgument>(), revision,
                                                 m_limits);
            else
                result = parseMenuLayout(layoutValue, m_limits);
        }
        if (!result.ok()) {
            emit failed(result.error);
            m_state = DBusMenuLifecycleState::Error;
            emit changed();
            return;
        }
        applyLayout(result, parentId);
    });
}

void DBusMenuClient::applyLayout(const DBusMenuParseResult &layout, int requestedParentId)
{
    if (layout.revision < m_revision)
        return;
    DBusMenuNode decorated = layout.root;
    decorateIcons(decorated);
    const bool applied = requestedParentId == 0
        ? (m_rootModel->setRoot(decorated), true)
        : m_rootModel->replaceSubtree(requestedParentId, decorated);
    if (!applied && requestedParentId != 0)
        requestLayout();
    m_revision = qMax(m_revision, layout.revision);
    m_state = m_rootModel->rowCount() == 0 ? DBusMenuLifecycleState::Empty
                                           : DBusMenuLifecycleState::Ready;
    emit changed();
}

void DBusMenuClient::connectSignals()
{
    if (m_signalsConnected)
        return;
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(m_address.service, m_menuPath, QStringLiteral("com.canonical.dbusmenu"),
                QStringLiteral("LayoutUpdated"), this, SLOT(onLayoutUpdated(uint,int)));
    bus.connect(m_address.service, m_menuPath, QStringLiteral("com.canonical.dbusmenu"),
                QStringLiteral("ItemsPropertiesUpdated"), this,
                SLOT(onItemsPropertiesUpdated(QList<Astrea::StatusNotifier::DBusMenuPropertyUpdate>,QList<Astrea::StatusNotifier::DBusMenuRemovedProperties>)));
    m_signalsConnected = true;
}

void DBusMenuClient::disconnectSignals()
{
    if (!m_signalsConnected)
        return;
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.disconnect(m_address.service, m_menuPath, QStringLiteral("com.canonical.dbusmenu"),
                   QStringLiteral("LayoutUpdated"), this, SLOT(onLayoutUpdated(uint,int)));
    bus.disconnect(m_address.service, m_menuPath, QStringLiteral("com.canonical.dbusmenu"),
                   QStringLiteral("ItemsPropertiesUpdated"), this,
                   SLOT(onItemsPropertiesUpdated(QList<Astrea::StatusNotifier::DBusMenuPropertyUpdate>,QList<Astrea::StatusNotifier::DBusMenuRemovedProperties>)));
    m_signalsConnected = false;
}

void DBusMenuClient::onLayoutUpdated(quint32 revision, int parentId)
{
    if (m_stopped || revision < m_revision)
        return;
    requestLayout(parentId);
}

void DBusMenuClient::onItemsPropertiesUpdated(
    const QList<DBusMenuPropertyUpdate> &updated,
    const QList<DBusMenuRemovedProperties> &removed)
{
    if (m_stopped)
        return;
    if (!m_rootModel->updateNodeProperties(updated, removed)) {
        requestLayout();
        return;
    }
    if (m_iconStore) {
        for (const auto &update : updated) {
            const QString key = QStringLiteral("menu:%1:%2").arg(m_address.key()).arg(update.id);
            DBusMenuNode node = m_rootModel->nodeById(update.id);
            updateRemoteIcon(node);
            m_rootModel->setIconSource(update.id, node.iconSource);
        }
        for (const auto &removedProperties : removed) {
            DBusMenuNode node = m_rootModel->nodeById(removedProperties.id);
            updateRemoteIcon(node);
            m_rootModel->setIconSource(removedProperties.id, node.iconSource);
        }
    }
    emit changed();
}

void DBusMenuClient::decorateIcons(DBusMenuNode &node)
{
    updateRemoteIcon(node);
    for (DBusMenuNode &child : node.children)
        decorateIcons(child);
}

void DBusMenuClient::updateRemoteIcon(DBusMenuNode &node)
{
    if (!m_iconStore)
        return;
    const QString key = QStringLiteral("menu:%1:%2").arg(m_address.key()).arg(node.id);
    QString ignored;
    if (!node.iconData.isEmpty() && isSafeIconData(node.iconData, m_limits, &ignored))
        updateIconSource(m_iconStore, key, node.iconData, m_limits);
    else
        m_iconStore->updateAuxiliaryNamedImage(key, node.iconName);
    node.iconSource = m_iconStore->hasIcon(key) ? m_iconStore->imageSource(key) : QString();
}

} // namespace Astrea::StatusNotifier

QDBusArgument &operator<<(QDBusArgument &argument,
                          const Astrea::StatusNotifier::DBusMenuLayoutNodeWire &node)
{
    argument.beginStructure();
    argument << node.id << node.properties;
    QVariantList children;
    children.reserve(node.children.size());
    for (const auto &child : node.children)
        children.append(QVariant::fromValue(child));
    argument << children;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Astrea::StatusNotifier::DBusMenuLayoutNodeWire &node)
{
    argument.beginStructure();
    QVariantList children;
    argument >> node.id >> node.properties >> children;
    argument.endStructure();
    node.children.clear();
    for (const QVariant &childValue : children) {
        if (childValue.canConvert<Astrea::StatusNotifier::DBusMenuLayoutNodeWire>()) {
            node.children.append(childValue.value<Astrea::StatusNotifier::DBusMenuLayoutNodeWire>());
            continue;
        }
        if (childValue.canConvert<QDBusArgument>()) {
            Astrea::StatusNotifier::DBusMenuLayoutNodeWire child;
            childValue.value<QDBusArgument>() >> child;
            node.children.append(child);
        }
    }
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument,
                          const Astrea::StatusNotifier::DBusMenuLayoutReply &reply)
{
    argument.beginStructure();
    argument << reply.revision << reply.root;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                Astrea::StatusNotifier::DBusMenuLayoutReply &reply)
{
    argument.beginStructure();
    argument >> reply.revision >> reply.root;
    argument.endStructure();
    return argument;
}
