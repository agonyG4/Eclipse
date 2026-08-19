#pragma once

#include <QJsonObject>
#include <QString>

namespace Astrea::System {

Q_NAMESPACE

enum class SystemServiceState {
    Stopped,
    Starting,
    Ready,
    Unavailable,
    Degraded,
};
Q_ENUM_NS(SystemServiceState)

enum class NetworkConnectionType {
    None,
    Wifi,
    Wired,
    Other,
};
Q_ENUM_NS(NetworkConnectionType)

QString systemServiceStateName(SystemServiceState state);
QJsonObject serviceHealthJson(SystemServiceState state, bool available, bool ready,
                              const QString &errorString = {});

} // namespace Astrea::System
