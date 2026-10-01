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

enum class NetworkDnsMode {
    Automatic,
    Custom,
    Advanced,
};
Q_ENUM_NS(NetworkDnsMode)

enum class NetworkDnsOutcome {
    Applied,
    SavedNeedsReconnect,
    Conflict,
    Failed,
};
Q_ENUM_NS(NetworkDnsOutcome)

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

enum class BluetoothDeviceKind {
    Unknown = 0,
    Audio = 1,
    Headset = 2,
    Headphones = 3,
    Speaker = 4,
    Keyboard = 5,
    Mouse = 6,
    Gamepad = 7,
    Phone = 8,
    Computer = 9,
    Tablet = 10,
    Printer = 11,
    Camera = 12,
    Network = 13,
};
Q_ENUM_NS(BluetoothDeviceKind)

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
