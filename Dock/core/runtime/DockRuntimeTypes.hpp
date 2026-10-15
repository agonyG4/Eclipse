#pragma once

#include <QHash>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Astrea::Dock {

struct DockRuntimeWindow {
    QString id;
    QString appId;
    QString title;
    quint32 pid = 0;
    bool active = false;
    bool minimized = false;
    bool maximized = false;
    bool fullscreen = false;
    quint64 focusSerial = 0;
    quint64 revision = 0;
};

struct DockRuntimeSnapshot {
    QVector<DockRuntimeWindow> windows;
    quint64 revision = 0;
    quint32 total = 0;
    bool truncated = false;
    quint64 generation = 0;
    bool authoritative = false;
};

enum class DockRuntimeAction {
    Activate,
    Close,
};

enum class DockRuntimeActionResult {
    Accepted,
    NoChange,
    Unavailable,
};

enum class DockRuntimeActionError {
    UnsupportedProtocol,
    NotActionReady,
    Disconnected,
    LocalCapacityExceeded,
    WindowNotLive,
    InvalidRequest,
};

struct DockApplicationRuntimeState {
    QString taskKey;
    QString appId;
    QString desktopFileName;
    QString desktopId;
    QString displayName;
    QString iconName;
    QString iconPath;
    bool identityPending = false;
    bool running = false;
    bool active = false;
    int windowCount = 0;
    QVector<QString> windowIds;
    QVector<quint64> focusSerials;
};

struct DockApplicationRuntimeProjection {
    QHash<QString, DockApplicationRuntimeState> states;
    QStringList encounterOrder;
};

} // namespace Astrea::Dock

Q_DECLARE_METATYPE(Astrea::Dock::DockRuntimeWindow)
Q_DECLARE_METATYPE(Astrea::Dock::DockRuntimeSnapshot)
Q_DECLARE_METATYPE(Astrea::Dock::DockRuntimeAction)
Q_DECLARE_METATYPE(Astrea::Dock::DockRuntimeActionResult)
Q_DECLARE_METATYPE(Astrea::Dock::DockRuntimeActionError)
Q_DECLARE_METATYPE(Astrea::Dock::DockApplicationRuntimeState)
Q_DECLARE_METATYPE(Astrea::Dock::DockApplicationRuntimeProjection)
