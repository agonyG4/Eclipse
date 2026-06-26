#include "core/AltTabWindowModel.hpp"
#include <QUrl>

AltTabWindowModel::AltTabWindowModel(QObject *parent)
    : QAbstractListModel(parent) {}

int AltTabWindowModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_windows.size();
}

QVariant AltTabWindowModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_windows.size())
        return {};
    const auto &w = m_windows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case DisplayNameRole: return w.displayName;
    case WindowIdRole: return w.windowId.value;
    case PidRole: return static_cast<qlonglong>(w.pid);
    case ClassNameRole: return w.className;
    case InitialClassRole: return w.initialClass;
    case TitleRole: return w.title;
    case WorkspaceIdRole: return w.workspaceId.value;
    case WorkspaceNameRole: return w.workspaceName;
    case OutputRole: return w.outputId.value;
    case FocusOrderRole: return w.focusHistoryId;
    case IconNameRole: return w.iconName;
    case IconPathRole: return w.iconPath;
    case IconUrlRole: {
        if (!w.iconPath.isEmpty())
            return QUrl::fromLocalFile(w.iconPath).toString();
        if (!w.iconName.isEmpty())
            return QStringLiteral("image://astrea-icon/") + w.iconName;
        return {};
    }
    case SelectedRole: return index.row() == m_selectedIndex;
    case HiddenRole: return w.isHidden;
    case MinimizedRole: return w.isMinimized;
    case ActiveRole: return w.isActive;
    default: return {};
    }
}

QHash<int, QByteArray> AltTabWindowModel::roleNames() const {
    return {
        {WindowIdRole, "address"},
        {TitleRole, "title"},
        {DisplayNameRole, "displayName"},
        {ClassNameRole, "className"},
        {InitialClassRole, "initialClass"},
        {PidRole, "pid"},
        {WorkspaceIdRole, "workspaceId"},
        {WorkspaceNameRole, "workspaceName"},
        {OutputRole, "output"},
        {FocusOrderRole, "focusOrder"},
        {IconNameRole, "iconName"},
        {IconPathRole, "iconPath"},
        {IconUrlRole, "iconUrl"},
        {SelectedRole, "selected"},
        {HiddenRole, "hidden"},
        {MinimizedRole, "minimized"},
        {ActiveRole, "active"},
    };
}

static QVector<int> collectChangedRoles(WindowInfo &dst, const WindowInfo &src) {
    QVector<int> roles;
    if (dst.title != src.title) { dst.title = src.title; roles << AltTabWindowModel::TitleRole; }
    if (dst.displayName != src.displayName) { dst.displayName = src.displayName; roles << AltTabWindowModel::DisplayNameRole; }
    if (dst.className != src.className) { dst.className = src.className; roles << AltTabWindowModel::ClassNameRole; }
    if (dst.initialClass != src.initialClass) { dst.initialClass = src.initialClass; roles << AltTabWindowModel::InitialClassRole; }
    if (dst.pid != src.pid) { dst.pid = src.pid; roles << AltTabWindowModel::PidRole; }
    if (dst.workspaceId.value != src.workspaceId.value) { dst.workspaceId = src.workspaceId; roles << AltTabWindowModel::WorkspaceIdRole; }
    if (dst.workspaceName != src.workspaceName) { dst.workspaceName = src.workspaceName; roles << AltTabWindowModel::WorkspaceNameRole; }
    if (dst.outputId.value != src.outputId.value) { dst.outputId = src.outputId; roles << AltTabWindowModel::OutputRole; }
    if (dst.focusHistoryId != src.focusHistoryId) { dst.focusHistoryId = src.focusHistoryId; roles << AltTabWindowModel::FocusOrderRole; }
    if (dst.iconName != src.iconName) { dst.iconName = src.iconName; roles << AltTabWindowModel::IconNameRole << AltTabWindowModel::IconUrlRole; }
    if (dst.iconPath != src.iconPath) { dst.iconPath = src.iconPath; roles << AltTabWindowModel::IconPathRole << AltTabWindowModel::IconUrlRole; }
    if (dst.isHidden != src.isHidden) { dst.isHidden = src.isHidden; roles << AltTabWindowModel::HiddenRole; }
    if (dst.isMinimized != src.isMinimized) { dst.isMinimized = src.isMinimized; roles << AltTabWindowModel::MinimizedRole; }
    if (dst.isActive != src.isActive) { dst.isActive = src.isActive; roles << AltTabWindowModel::ActiveRole; }
    return roles;
}

static bool windowsEqual(const WindowInfo &a, const WindowInfo &b) {
    return a.windowId == b.windowId && a.title == b.title && a.className == b.className
        && a.initialClass == b.initialClass && a.pid == b.pid
        && a.workspaceId.value == b.workspaceId.value && a.workspaceName == b.workspaceName
        && a.outputId.value == b.outputId.value
        && a.focusHistoryId == b.focusHistoryId && a.iconName == b.iconName
        && a.iconPath == b.iconPath
        && a.isHidden == b.isHidden && a.isMinimized == b.isMinimized && a.isActive == b.isActive;
}

void AltTabWindowModel::setWindows(const QVector<WindowInfo> &newWindows) {
    if (newWindows.isEmpty()) {
        clear();
        return;
    }

    const int oldSelectedIndex = m_selectedIndex;
    WindowId oldSelectedId;
    if (m_selectedIndex >= 0 && m_selectedIndex < m_windows.size()) {
        oldSelectedId = m_windows[m_selectedIndex].windowId;
    }

    bool identical = (m_windows.size() == newWindows.size());
    if (identical) {
        for (int i = 0; i < m_windows.size(); ++i) {
            if (!windowsEqual(m_windows[i], newWindows[i])) {
                identical = false;
                break;
            }
        }
    }

    if (identical) {
        bool emitted = false;
        for (int i = 0; i < m_windows.size(); ++i) {
            QVector<int> roles = collectChangedRoles(m_windows[i], newWindows[i]);
            if (!roles.isEmpty()) {
                emit dataChanged(index(i), index(i), roles);
                emitted = true;
            }
        }
        return;
    }

    const int oldCount = m_windows.size();

    // Remove windows that are no longer present
    for (int i = m_windows.size() - 1; i >= 0; --i) {
        bool found = false;
        for (const auto &nw : newWindows) {
            if (nw.windowId == m_windows[i].windowId) {
                found = true;
                break;
            }
        }
        if (!found) {
            beginRemoveRows({}, i, i);
            m_windows.removeAt(i);
            endRemoveRows();
        }
    }

    // Insert or update windows to match newWindows order/existence
    for (int i = 0; i < newWindows.size(); ++i) {
        const auto &nw = newWindows[i];
        int currentIdx = -1;
        for (int j = 0; j < m_windows.size(); ++j) {
            if (m_windows[j].windowId == nw.windowId) {
                currentIdx = j;
                break;
            }
        }

        if (currentIdx < 0) {
            beginInsertRows({}, i, i);
            m_windows.insert(i, nw);
            endInsertRows();
        } else if (currentIdx != i) {
            beginRemoveRows({}, currentIdx, currentIdx);
            m_windows.removeAt(currentIdx);
            endRemoveRows();

            beginInsertRows({}, i, i);
            m_windows.insert(i, nw);
            endInsertRows();
        } else {
            QVector<int> roles = collectChangedRoles(m_windows[i], nw);
            if (!roles.isEmpty())
                emit dataChanged(index(i), index(i), roles);
        }
    }

    int newSelectedIndex = -1;
    if (!oldSelectedId.isEmpty()) {
        for (int i = 0; i < m_windows.size(); ++i) {
            if (m_windows[i].windowId == oldSelectedId) {
                newSelectedIndex = i;
                break;
            }
        }
    }

    if (newSelectedIndex == -1 && !m_windows.isEmpty()) {
        newSelectedIndex = qBound(0, oldSelectedIndex, m_windows.size() - 1);
    }
    setSelectedIndex(newSelectedIndex);

    if (oldCount != m_windows.size())
        emit countChanged();
}

void AltTabWindowModel::updateWindow(const WindowInfo &window) {
    for (int i = 0; i < m_windows.size(); ++i) {
        if (m_windows[i].windowId == window.windowId) {
            m_windows[i] = window;
            emit dataChanged(index(i), index(i), {IconNameRole, IconPathRole, IconUrlRole, DisplayNameRole});
            return;
        }
    }
    const int oldCount = m_windows.size();
    beginInsertRows({}, m_windows.size(), m_windows.size());
    m_windows.append(window);
    endInsertRows();
    if (oldCount != m_windows.size())
        emit countChanged();
}

void AltTabWindowModel::removeWindow(const QString &address) {
    for (int i = 0; i < m_windows.size(); ++i) {
        if (m_windows[i].windowId.value == address) {
            beginRemoveRows({}, i, i);
            m_windows.removeAt(i);
            endRemoveRows();
            if (m_selectedIndex >= m_windows.size())
                setSelectedIndex(m_windows.size() - 1);
            emit countChanged();
            return;
        }
    }
}

void AltTabWindowModel::clear() {
    if (m_windows.isEmpty())
        return;
    beginResetModel();
    m_windows.clear();
    m_selectedIndex = -1;
    endResetModel();
    emit countChanged();
    emit selectedIndexChanged();
}

WindowInfo AltTabWindowModel::at(int row) const {
    if (row < 0 || row >= m_windows.size())
        return {};
    return m_windows.at(row);
}

int AltTabWindowModel::indexOf(const QString &address) const {
    for (int i = 0; i < m_windows.size(); ++i) {
        if (m_windows[i].windowId.value == address)
            return i;
    }
    return -1;
}

void AltTabWindowModel::setSelectedIndex(int index) {
    const int old = m_selectedIndex;
    if (old >= 0 && old < m_windows.size())
        emit dataChanged(createIndex(old, 0), createIndex(old, 0), {SelectedRole});
    m_selectedIndex = qBound(-1, index, m_windows.size() - 1);
    if (m_selectedIndex >= 0 && m_selectedIndex < m_windows.size())
        emit dataChanged(createIndex(m_selectedIndex, 0), createIndex(m_selectedIndex, 0), {SelectedRole});
    if (old != m_selectedIndex)
        emit selectedIndexChanged();
}
