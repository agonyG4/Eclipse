#pragma once

#include <QJsonObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace Astrea::System {

Q_NAMESPACE
QML_NAMED_ELEMENT(System)

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

enum class BluetoothAgentRequestKind {
    None,
    PinCodeInput,
    PasskeyInput,
    PasskeyConfirmation,
    Authorization,
    ServiceAuthorization,
    DisplayPinCode,
    DisplayPasskey,
};
Q_ENUM_NS(BluetoothAgentRequestKind)

QString systemServiceStateName(SystemServiceState state);
QJsonObject serviceHealthJson(SystemServiceState state, bool available, bool ready,
                              const QString &errorString = {});

} // namespace Astrea::System
