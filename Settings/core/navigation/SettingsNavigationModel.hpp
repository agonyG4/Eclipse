#pragma once

#include "core/navigation/SettingsNavigationCatalog.hpp"

#include <QAbstractListModel>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

class SettingsNavigationModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY selectedIdChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        SubtitleRole,
        IconNameRole,
        KindRole,
        EnabledRole,
        SelectedRole,
        LabelRole,
        LabelKeyRole,
        SymRole,
        IconSourceRole,
        IconKeyRole,
        PageSourceRole,
        SectionKeyRole,
        ParentSectionRole,
        ExpandedRole,
    };
    Q_ENUM(Role)

    explicit SettingsNavigationModel(const SettingsNavigationCatalog &catalog,
                                     QObject *parent = nullptr);
    explicit SettingsNavigationModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString selectedId() const;
    bool setSelectedId(const QString &id);
    Q_INVOKABLE bool toggleSection(const QString &sectionId);

    Q_INVOKABLE QVariantMap get(int row) const;

    QString titleForId(const QString &id) const;
    QUrl pageSourceForId(const QString &id) const;
    bool containsSelectableId(const QString &id) const;

signals:
    void selectedIdChanged();

private:
    void rebuildVisibleRows();
    bool isSelectable(const SettingsNavigationEntry &entry) const;
    int sectionIndexForId(const QString &sectionId) const;
    int firstSelectableSourceIndex() const;
    int sourceIndexForId(const QString &id) const;
    int visibleRowForSourceIndex(int sourceIndex) const;

    QVector<SettingsNavigationEntry> m_entries;
    QVector<int> m_visibleRows;
    QString m_selectedId;
};
