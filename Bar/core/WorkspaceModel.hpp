#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

struct WorkspaceItem {
    QString id;
    bool active = false;
    bool occupied = false;
    bool urgent = false;
    QString outputId;
};

class WorkspaceModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        ActiveRole,
        OccupiedRole,
        UrgentRole,
        OutputIdRole,
    };
    Q_ENUM(Role)

    explicit WorkspaceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void replaceWorkspaces(QVector<WorkspaceItem> workspaces);
    QVector<WorkspaceItem> workspaceItems() const { return m_items; }

signals:
    void countChanged();

private:
    QVector<WorkspaceItem> m_items;
};
