#pragma once

#include "core/navigation/SettingsNavigationCatalog.hpp"

#include <QAbstractListModel>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class SettingsNavigationModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        SubtitleRole,
        IconNameRole,
        KindRole,
        EnabledRole,
        LabelRole,
        LabelKeyRole,
        SymRole,
        IconSourceRole,
        IconKeyRole,
        PageSourceRole,
        SidebarVisibleRole,
        ParentIdRole,
    };
    Q_ENUM(Role)

    explicit SettingsNavigationModel(const SettingsNavigationCatalog &catalog,
                                     QObject *parent = nullptr);
    explicit SettingsNavigationModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    const SettingsNavigationEntry *entryForId(const QString &id) const;
    QVector<SettingsNavigationEntry> childrenForId(const QString &id) const;
    QString firstNavigableSidebarDestination() const;
    QString sidebarAncestorForId(const QString &id) const;

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QVariantMap descriptorForId(const QString &id) const;
    Q_INVOKABLE QVariantList childDescriptorsForId(const QString &id) const;

    QString titleForId(const QString &id) const;
    QUrl pageSourceForId(const QString &id) const;
    bool containsNavigableId(const QString &id) const;

private:
    bool isNavigable(const SettingsNavigationEntry &entry) const;
    int sourceIndexForId(const QString &id) const;
    QVariantMap descriptorForEntry(const SettingsNavigationEntry &entry) const;

    QVector<SettingsNavigationEntry> m_entries;
    QVector<int> m_sidebarRows;
};
