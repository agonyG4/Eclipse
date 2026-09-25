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

// Numeric values are shared with the Rust CXX-Qt category-code mapping.
enum class BluetoothServiceCategory {
    Unknown = 0,
    Audio = 1,
    MediaControl = 2,
    Calls = 3,
    HumanInterface = 4,
    FileTransfer = 5,
    Contacts = 6,
    Network = 7,
    Battery = 8,
    Serial = 9,
};
Q_ENUM_NS(BluetoothServiceCategory)

QString systemServiceStateName(SystemServiceState state);
QJsonObject serviceHealthJson(SystemServiceState state, bool available, bool ready,
                              const QString &errorString = {});

} // namespace Astrea::System
