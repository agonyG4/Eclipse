#pragma once

#include "core/DockAppInfo.hpp"
#include "apps/DesktopEntryCatalog.hpp"

#include <QAbstractListModel>
#include <QVector>
#include <memory>

class DockAppModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        DesktopFileNameRole = Qt::UserRole + 1,
        DesktopIdRole,
        DisplayNameRole,
        IconNameRole,
        IconPathRole,
        IconUrlRole,
        ResolvedRole,
        LaunchingRole,
        LaunchErrorRole,
        PinnedRole,
        RunningRole,
        ActiveRole,
        WindowCountRole
    };
    Q_ENUM(Role)

    explicit DockAppModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setPins(const QStringList &pins);
    void setCatalogSnapshot(std::shared_ptr<const DesktopEntrySnapshot> snapshot);

    QString desktopFileNameAt(int row) const;
    int rowForDesktopFileName(const QString &desktopFileName) const;
    const DockAppInfo *itemAt(int row) const;

    bool setLaunching(const QString &desktopFileName, bool launching);
    bool setLaunchError(const QString &desktopFileName, const QString &error);

private:
    DockAppInfo makeItem(const QString &desktopFileName, const DockAppInfo *previous = nullptr) const;
    void updateItem(int row, const DockAppInfo &next);
    static QList<int> changedRoles(const DockAppInfo &before, const DockAppInfo &after);

    QVector<DockAppInfo> m_items;
    std::shared_ptr<const DesktopEntrySnapshot> m_catalog;
};
