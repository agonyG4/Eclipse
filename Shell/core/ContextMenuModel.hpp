#pragma once

#include <QAbstractItemModel>
#include <QHash>
#include <QVector>

#include <memory>

namespace Astrea::Shell {

class ContextMenuModel final : public QAbstractItemModel {
    Q_OBJECT
    Q_PROPERTY(quint64 presentationRevision READ presentationRevision NOTIFY presentationChanged)

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
    quint64 presentationRevision() const { return m_presentationRevision; }
    bool canActivate(const QString &token) const;
    Q_INVOKABLE int presentationContentHeight(int normalRowHeight, int separatorHeight) const;
    Q_INVOKABLE int presentationNaturalWidth(const QString &fontFamily, int bodyFontSize,
                                             int smallFontSize, int rowHorizontalMargin,
                                             int iconSlotWidth, int spacing, int cardPadding,
                                             int borderWidth) const;
    Q_INVOKABLE QObject *childModelAt(int row) const;
    Q_INVOKABLE int firstNavigable() const;
    Q_INVOKABLE int nextNavigable(int currentRow, int delta) const;

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

signals:
    void presentationChanged();

private:
    std::unique_ptr<Node> m_root;
    mutable QHash<const Node *, ContextMenuModel *> m_childModels;
    QString m_lastError;
    quint64 m_presentationRevision = 0;
};

} // namespace Astrea::Shell
