#include "ContextMenuModel.hpp"

#include <algorithm>
#include <functional>

namespace Astrea::Shell {

ContextMenuModel::ContextMenuModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_root(std::make_unique<Node>())
{
}

int ContextMenuModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() && parent.column() != 0) {
        return 0;
    }
    const Node *node = parent.isValid() ? nodeForIndex(parent) : m_root.get();
    return node ? static_cast<int>(node->children.size()) : 0;
}

int ContextMenuModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QModelIndex ContextMenuModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0 || row < 0) {
        return {};
    }
    const Node *parentNode = parent.isValid() ? nodeForIndex(parent) : m_root.get();
    if (!parentNode || row >= static_cast<int>(parentNode->children.size())) {
        return {};
    }
    return createIndex(row, column, parentNode->children.at(static_cast<size_t>(row)).get());
}

QModelIndex ContextMenuModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) {
        return {};
    }
    const Node *node = nodeForIndex(child);
    if (!node || !node->parent || node->parent == m_root.get()) {
        return {};
    }
    const Node *parentNode = node->parent;
    const auto it = std::find_if(parentNode->parent->children.cbegin(),
                                 parentNode->parent->children.cend(),
                                 [parentNode](const auto &candidate) {
                                     return candidate.get() == parentNode;
                                 });
    if (it == parentNode->parent->children.cend()) {
        return {};
    }
    return createIndex(static_cast<int>(std::distance(parentNode->parent->children.cbegin(), it)),
                       0, const_cast<Node *>(parentNode));
}

QVariant ContextMenuModel::data(const QModelIndex &index, int role) const
{
    const Node *node = nodeForIndex(index);
    if (!node) {
        return {};
    }
    switch (role) {
    case Qt::DisplayRole:
    case LabelRole:
        return node->spec.label;
    case TokenRole:
        return node->spec.token;
    case KindRole:
        return static_cast<int>(node->spec.kind);
    case IconRole:
        return node->spec.icon;
    case EnabledRole:
        return node->spec.enabled;
    case VisibleRole:
        return node->spec.visible;
    case ShortcutRole:
        return node->spec.shortcut;
    case CheckStateRole:
        return static_cast<int>(node->spec.checkState);
    case CheckTypeRole:
        return static_cast<int>(node->spec.checkType);
    case DestructiveRole:
        return node->spec.destructive;
    case HasChildrenRole:
        return !node->children.empty();
    default:
        return {};
    }
}

QHash<int, QByteArray> ContextMenuModel::roleNames() const
{
    return {
        {TokenRole, "token"},
        {KindRole, "kind"},
        {LabelRole, "label"},
        {IconRole, "icon"},
        {EnabledRole, "enabled"},
        {VisibleRole, "visible"},
        {ShortcutRole, "shortcut"},
        {CheckStateRole, "checkState"},
        {CheckTypeRole, "checkType"},
        {DestructiveRole, "destructive"},
        {HasChildrenRole, "hasChildren"},
    };
}

bool ContextMenuModel::setRootNodes(const QVector<NodeSpec> &nodes)
{
    auto candidate = std::make_unique<Node>();
    QHash<QString, bool> tokens;
    int nodeCount = 0;
    m_lastError.clear();

    for (const NodeSpec &spec : nodes) {
        if (!appendNode(candidate.get(), spec, 1, tokens, nodeCount)) {
            if (m_lastError.isEmpty()) {
                m_lastError = QStringLiteral("invalid context menu node");
            }
            return false;
        }
    }
    normalizeSeparators(candidate->children);

    beginResetModel();
    m_root = std::move(candidate);
    endResetModel();
    return true;
}

void ContextMenuModel::clear()
{
    if (m_root->children.empty()) {
        return;
    }
    beginResetModel();
    m_root->children.clear();
    endResetModel();
}

bool ContextMenuModel::canActivate(const QString &token) const
{
    if (token.isEmpty()) {
        return false;
    }
    std::function<bool(const Node *)> visit = [&](const Node *node) {
        if (node->spec.kind == NodeKind::Action && node->spec.token == token) {
            return node->spec.visible && node->spec.enabled;
        }
        for (const auto &child : node->children) {
            if (visit(child.get())) {
                return true;
            }
        }
        return false;
    };
    return visit(m_root.get());
}

bool ContextMenuModel::appendNode(Node *parent, const NodeSpec &spec, int depth,
                                  QHash<QString, bool> &tokens, int &nodeCount)
{
    if (nodeCount >= MaximumNodes) {
        m_lastError = QStringLiteral("context menu exceeds maximum nodes");
        return false;
    }
    if (depth > MaximumDepth) {
        m_lastError = QStringLiteral("context menu exceeds maximum depth");
        return false;
    }
    if (spec.token.size() > MaximumTokenLength || spec.label.size() > MaximumLabelLength) {
        m_lastError = QStringLiteral("context menu identifier or label is too long");
        return false;
    }
    if (spec.kind == NodeKind::Action && spec.token.isEmpty()) {
        m_lastError = QStringLiteral("context menu action has no token");
        return false;
    }
    if (spec.kind != NodeKind::Submenu && !spec.children.isEmpty()) {
        m_lastError = QStringLiteral("only submenu nodes may have children");
        return false;
    }
    if (spec.kind == NodeKind::Action && tokens.contains(spec.token)) {
        m_lastError = QStringLiteral("context menu action token is duplicated");
        return false;
    }

    auto node = std::make_unique<Node>();
    node->spec = spec;
    node->parent = parent;
    ++nodeCount;
    if (spec.kind == NodeKind::Action) {
        tokens.insert(spec.token, true);
    }
    for (const NodeSpec &child : spec.children) {
        if (!appendNode(node.get(), child, depth + 1, tokens, nodeCount)) {
            return false;
        }
    }
    normalizeSeparators(node->children);
    parent->children.push_back(std::move(node));
    return true;
}

void ContextMenuModel::normalizeSeparators(std::vector<std::unique_ptr<Node>> &nodes)
{
    while (!nodes.empty() && nodes.front()->spec.kind == NodeKind::Separator) {
        nodes.erase(nodes.begin());
    }
    while (!nodes.empty() && nodes.back()->spec.kind == NodeKind::Separator) {
        nodes.pop_back();
    }
    auto it = nodes.begin();
    while (it != nodes.end()) {
        if ((*it)->spec.kind == NodeKind::Separator) {
            auto next = std::next(it);
            while (next != nodes.end() && (*next)->spec.kind == NodeKind::Separator) {
                next = nodes.erase(next);
            }
        }
        ++it;
    }
}

ContextMenuModel::Node *ContextMenuModel::nodeForIndex(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<Node *>(index.internalPointer()) : nullptr;
}

} // namespace Astrea::Shell
