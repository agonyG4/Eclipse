#include "ContextMenuModel.hpp"

#include <algorithm>
#include <functional>
#include <QFontMetrics>
#include <QtAlgorithms>

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
        {KindRole, "nodeKind"},
        {LabelRole, "label"},
        {IconRole, "icon"},
        {EnabledRole, "nodeEnabled"},
        {VisibleRole, "nodeVisible"},
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
    qDeleteAll(m_childModels);
    m_childModels.clear();
    m_root = std::move(candidate);
    endResetModel();
    ++m_presentationRevision;
    emit presentationChanged();
    return true;
}

void ContextMenuModel::clear()
{
    if (m_root->children.empty()) {
        return;
    }
    beginResetModel();
    qDeleteAll(m_childModels);
    m_childModels.clear();
    m_root->children.clear();
    endResetModel();
    ++m_presentationRevision;
    emit presentationChanged();
}

bool ContextMenuModel::canActivate(const QString &token) const
{
    if (token.isEmpty()) {
        return false;
    }
    std::function<bool(const Node *, bool, bool)> visit =
        [&](const Node *node, bool parentVisible, bool parentEnabled) {
        const bool visible = parentVisible && node->spec.visible;
        const bool enabled = parentEnabled && node->spec.enabled;
        if (node->spec.kind == NodeKind::Action && node->spec.token == token) {
            return visible && enabled;
        }
        for (const auto &child : node->children) {
            if (visit(child.get(), visible, enabled)) {
                return true;
            }
        }
        return false;
    };
    return visit(m_root.get(), true, true);
}

int ContextMenuModel::presentationContentHeight(int normalRowHeight, int separatorHeight) const
{
    const int normalHeight = qMax(0, normalRowHeight);
    const int separatorRowHeight = qMax(0, separatorHeight);
    int total = 0;
    for (const auto &node : m_root->children) {
        if (!node->spec.visible)
            continue;
        total += node->spec.kind == NodeKind::Separator
            ? separatorRowHeight : normalHeight;
    }
    return total;
}

int ContextMenuModel::presentationNaturalWidth(const QString &fontFamily, int bodyFontSize,
                                               int smallFontSize, int rowHorizontalMargin,
                                               int iconSlotWidth, int spacing, int cardPadding,
                                               int borderWidth) const
{
    QFont bodyFont;
    bodyFont.setFamily(fontFamily);
    bodyFont.setPixelSize(qMax(1, bodyFontSize));
    bodyFont.setWeight(QFont::Medium);
    QFont shortcutFont;
    shortcutFont.setFamily(fontFamily);
    shortcutFont.setPixelSize(qMax(1, smallFontSize));
    const QFontMetrics bodyMetrics(bodyFont);
    const QFontMetrics shortcutMetrics(shortcutFont);
    const int horizontalMargin = qMax(0, rowHorizontalMargin);
    const int iconWidth = qMax(0, iconSlotWidth);
    const int rowSpacing = qMax(0, spacing);
    int widestRow = 0;

    for (const auto &node : m_root->children) {
        if (!node->spec.visible || node->spec.kind == NodeKind::Separator)
            continue;
        const int labelWidth = bodyMetrics.horizontalAdvance(node->spec.label);
        const int shortcutWidth = node->spec.shortcut.isEmpty()
            ? 0 : shortcutMetrics.horizontalAdvance(node->spec.shortcut);
        const int arrowWidth = node->children.empty()
            ? 0 : bodyMetrics.horizontalAdvance(QStringLiteral("›"));
        const int rowWidth = horizontalMargin * 2 + iconWidth + labelWidth
            + shortcutWidth + arrowWidth + rowSpacing * 3;
        widestRow = qMax(widestRow, rowWidth);
    }

    return widestRow + qMax(0, cardPadding) * 2 + qMax(0, borderWidth) * 2;
}

QObject *ContextMenuModel::childModelAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_root->children.size()))
        return nullptr;
    const Node *node = m_root->children.at(static_cast<size_t>(row)).get();
    if (node->children.empty())
        return nullptr;
    if (const auto it = m_childModels.constFind(node); it != m_childModels.constEnd())
        return it.value();

    auto *childModel = new ContextMenuModel(const_cast<ContextMenuModel *>(this));
    QVector<NodeSpec> children;
    children.reserve(static_cast<int>(node->children.size()));
    for (const auto &child : node->children)
        children.append(child->spec);
    if (!childModel->setRootNodes(children)) {
        childModel->deleteLater();
        return nullptr;
    }
    m_childModels.insert(node, childModel);
    return childModel;
}

int ContextMenuModel::firstNavigable() const
{
    return nextNavigable(-1, 1);
}

int ContextMenuModel::nextNavigable(int currentRow, int delta) const
{
    const int count = static_cast<int>(m_root->children.size());
    if (count == 0 || delta == 0)
        return -1;
    const int direction = delta > 0 ? 1 : -1;
    // A negative row is the sentinel used by QML for Home/End.  Seed it on
    // the appropriate edge so End does not accidentally wrap from row 0 to
    // the penultimate item.
    int row = currentRow < 0 ? (direction > 0 ? -1 : 0) : currentRow;
    for (int step = 0; step < count; ++step) {
        row = (row + direction + count) % count;
        const NodeSpec &spec = m_root->children.at(static_cast<size_t>(row))->spec;
        const bool navigable = spec.kind == NodeKind::Action
            || spec.kind == NodeKind::Submenu;
        if (navigable && spec.visible && spec.enabled)
            return row;
    }
    return -1;
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
    std::vector<std::unique_ptr<Node>> normalized;
    normalized.reserve(nodes.size());
    std::unique_ptr<Node> pendingSeparator;
    bool hasVisibleContent = false;

    for (auto &node : nodes) {
        if (node->spec.kind == NodeKind::Separator) {
            if (node->spec.visible && !pendingSeparator)
                pendingSeparator = std::move(node);
            continue;
        }

        // Keep hidden nodes in the model for role/state fidelity, but do not
        // let them make a separator appear at the visible leading/trailing
        // edge of a menu.
        if (!node->spec.visible) {
            normalized.push_back(std::move(node));
            continue;
        }

        if (pendingSeparator && hasVisibleContent)
            normalized.push_back(std::move(pendingSeparator));
        pendingSeparator.reset();
        normalized.push_back(std::move(node));
        hasVisibleContent = true;
    }
    nodes = std::move(normalized);
}

ContextMenuModel::Node *ContextMenuModel::nodeForIndex(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<Node *>(index.internalPointer()) : nullptr;
}

} // namespace Astrea::Shell
