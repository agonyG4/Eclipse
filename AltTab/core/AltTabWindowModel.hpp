#pragma once

#include "core/WindowInfo.hpp"
#include <QAbstractListModel>
#include <QVector>

class AltTabWindowModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        WindowIdRole = Qt::UserRole + 1,
        TitleRole,
        DisplayNameRole,
        ClassNameRole,
        InitialClassRole,
        PidRole,
        WorkspaceIdRole,
        WorkspaceNameRole,
        OutputRole,
        FocusOrderRole,
        IconNameRole,
        IconPathRole,
        IconUrlRole,
        SelectedRole,
        HiddenRole,
        MinimizedRole,
        ActiveRole,
    };

    explicit AltTabWindowModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setWindows(const QVector<WindowInfo> &windows);
    void updateWindow(const WindowInfo &window);
    void removeWindow(const QString &address);
    void clear();
    int count() const { return m_windows.size(); }
    WindowInfo at(int row) const;
    int indexOf(const QString &address) const;
    void setSelectedIndex(int index);
    int selectedIndex() const { return m_selectedIndex; }

signals:
    void countChanged();
    void selectedIndexChanged();

private:
    QVector<WindowInfo> m_windows;
    int m_selectedIndex = -1;
};
