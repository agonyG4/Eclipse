#pragma once

#include <QAbstractItemModel>
#include <QVector>

#include <memory>

namespace Astrea::Shell {

class ContextMenuModel final : public QAbstractItemModel {
    Q_OBJECT

public:
    enum class NodeKind {
        Action,
        Separator,
        Submenu,
    };
    Q_ENUM(NodeKind)

    enum class CheckType {
        None,
        Check,
        Radio,
    };
    Q_ENUM(CheckType)

    enum Role {
        TokenRole = Qt::UserRole + 1,
        KindRole,
        LabelRole,
        IconRole,
        EnabledRole,
        VisibleRole,
        ShortcutRole,
        CheckStateRole,
        CheckTypeRole,
        DestructiveRole,
        HasChildrenRole,
    };
    Q_ENUM(Role)

    struct NodeSpec {
        NodeKind kind = NodeKind::Action;
        QString token;
        QString label;
        QString icon;
        bool enabled = true;
        bool visible = true;
        QString shortcut;
        Qt::CheckState checkState = Qt::Unchecked;
        CheckType checkType = CheckType::None;
        bool destructive = false;
        QVector<NodeSpec> children;
    };

    explicit ContextMenuModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool setRootNodes(const QVector<NodeSpec> &nodes);
    void clear();
    QString lastError() const { return m_lastError; }
    bool canActivate(const QString &token) const;

    static constexpr int MaximumDepth = 8;
    static constexpr int MaximumNodes = 256;
    static constexpr int MaximumTokenLength = 128;
    static constexpr int MaximumLabelLength = 512;

private:
    struct Node {
        NodeSpec spec;
        Node *parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
    };

    bool appendNode(Node *parent, const NodeSpec &spec, int depth,
                    QHash<QString, bool> &tokens, int &nodeCount);
    static void normalizeSeparators(std::vector<std::unique_ptr<Node>> &nodes);
    Node *nodeForIndex(const QModelIndex &index) const;

    std::unique_ptr<Node> m_root;
    QString m_lastError;
};

} // namespace Astrea::Shell
