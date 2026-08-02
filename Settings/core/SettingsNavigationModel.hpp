#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVariantMap>
#include <QVector>

struct SettingsNavigationEntry {
    enum class Kind {
        Page,
        Group,
        Spacer,
    };

    QString id;
    QString label;
    QString labelKey;
    QString subtitle;
    QString sym;
    QString iconSource;
    QString iconKey;
    int pageIndex = -1;
    Kind kind = Kind::Page;
    bool enabled = true;
};

class SettingsNavigationModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
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
        PageIndexRole,
    };
    Q_ENUM(Role)

    explicit SettingsNavigationModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const;
    void setFilterText(const QString &filterText);

    QString selectedId() const;
    bool setSelectedId(const QString &id);

    Q_INVOKABLE QVariantMap get(int row) const;

    QString titleForId(const QString &id) const;
    bool containsSelectableId(const QString &id) const;

signals:
    void filterTextChanged();
    void selectedIdChanged();

private:
    void rebuildVisibleRows();
    int sourceIndexForId(const QString &id) const;
    int visibleRowForSourceIndex(int sourceIndex) const;

    QVector<SettingsNavigationEntry> m_entries;
    QVector<int> m_visibleRows;
    QString m_filterText;
    QString m_selectedId;
};
