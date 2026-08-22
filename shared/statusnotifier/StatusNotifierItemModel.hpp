#pragma once

#include "statusnotifier/StatusNotifierTypes.hpp"

#include <QAbstractListModel>
#include <QVector>

namespace Astrea::StatusNotifier {

class StatusNotifierItemModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        KeyRole = Qt::UserRole + 1,
        IdRole,
        TitleRole,
        CategoryRole,
        StatusRole,
        IconSourceRole,
        TooltipTitleRole,
        TooltipDescriptionRole,
        HasMenuRole,
        OnlyMenuRole,
        ReadyRole,
    };
    Q_ENUM(Role)

    explicit StatusNotifierItemModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void upsert(const ItemSnapshot &snapshot, const QString &iconSource);
    bool removeKey(const QString &key);
    void clear();
    ItemSnapshot item(const QString &key) const;
    bool contains(const QString &key) const;
    QStringList keys() const;

signals:
    void itemRemoved(const QString &key);

private:
    struct Row {
        ItemSnapshot snapshot;
        QString iconSource;
    };

    QVector<Row> m_rows;
};

} // namespace Astrea::StatusNotifier
