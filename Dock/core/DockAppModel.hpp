#pragma once

#include "core/DockAppInfo.hpp"
#include "apps/ApplicationIdentity.hpp"
#include "apps/DesktopEntryCatalog.hpp"
#include "platform/typhon/DockApplicationStateProjector.hpp"

#include <QAbstractListModel>
#include <QVector>
#include <memory>

class DockAppModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        DesktopFileNameRole = Qt::UserRole + 1,
        TaskKeyRole,
        RuntimeAppIdRole,
        DesktopIdRole,
        DisplayNameRole,
        IconNameRole,
        IconPathRole,
        IconUrlRole,
        ResolvedRole,
        IdentityPendingRole,
        LaunchingRole,
        LaunchErrorRole,
        PinnedRole,
        RuntimeKnownRole,
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
    QString taskKeyAt(int row) const;
    int rowForDesktopFileName(const QString &desktopFileName) const;
    int rowForTaskKey(const QString &taskKey) const;
    QString runtimeTaskKeyForDesktopFileName(const QString &desktopFileName) const;
    const DockAppInfo *itemAt(int row) const;

    bool setLaunching(const QString &desktopFileName, bool launching);
    bool setLaunchError(const QString &desktopFileName, const QString &error);
    void applyRuntimeProjection(
        const Astrea::Typhon::DockApplicationRuntimeProjection &projection,
        bool authoritative = true);
    void clearRuntimeProjection();
    bool applyIdentityEnrichment(const QString &taskKey, const AppIdentity &identity);

private:
    DockAppInfo makeItem(const QString &taskKey, const DockAppInfo *previous = nullptr) const;
    void updateItem(int row, const DockAppInfo &next);
    void reconcileRows();
    static QList<int> changedRoles(const DockAppInfo &before, const DockAppInfo &after);
    static QString desktopTaskKey(const QString &desktopFileName);
    static QString desktopFileNameForTaskKey(const QString &taskKey);
    bool isPinnedTaskKey(const QString &taskKey) const;

    QVector<DockAppInfo> m_items;
    QStringList m_pins;
    QHash<QString, Astrea::Typhon::DockApplicationRuntimeState> m_runtimeStates;
    QHash<QString, QString> m_runtimeDesktopFileNames;
    QHash<QString, AppIdentity> m_identityEnrichments;
    QStringList m_dynamicOrder;
    QStringList m_runtimeEncounterOrder;
    bool m_runtimeAuthoritative = false;
    std::shared_ptr<const DesktopEntrySnapshot> m_catalog;
};
