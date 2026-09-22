#pragma once

#include "system/SystemServiceState.hpp"
#include "system/bluetooth/BluetoothDeviceModel.hpp"

#include <QString>
#include <QVector>

#include <functional>

namespace Astrea::System {

struct BluetoothSnapshot {
    SystemServiceState state = SystemServiceState::Stopped;
    bool available = false;
    bool ready = false;
    bool adapterAvailable = false;
    QString adapterPath;
    QString adapterName;
    bool powered = false;
    bool powerPending = false;
    bool scanning = false;
    int connectedCount = 0;
    QString connectedName;
    QString errorString;
    QVector<BluetoothDevice> devices;
    bool pairing = false;
    QString pairingDevicePath;
    QString pairingDeviceName;
    QString pairingError;
    bool agentRequestActive = false;
    quint64 agentRequestId = 0;
    BluetoothAgentRequestKind agentRequestKind = BluetoothAgentRequestKind::None;
    QString agentDevicePath;
    QString agentDeviceName;
    quint32 agentPasskey = 0;
    int agentEntered = -1;
    QString agentServiceUuid;
    QString agentDisplayPin;
    QString operationError;
};

// Thin Qt transport boundary. Bluetooth policy and BlueZ calls live in the
// Rust engine; the production implementation only maps its snapshot and
// commands to this Qt-facing compatibility type.
class BluetoothBackend {
public:
    struct Callbacks {
        std::function<void(BluetoothSnapshot)> snapshotChanged;
    };

    virtual ~BluetoothBackend() = default;
    virtual bool start(const Callbacks &callbacks, QString *errorOut) = 0;
    virtual void stop() = 0;
    virtual bool setPowered(bool powered) = 0;
    virtual bool requestScan(const QString &owner) = 0;
    virtual void releaseScan(const QString &owner) = 0;
    virtual bool connectDevice(const QString &objectPath) = 0;
    virtual bool disconnectDevice(const QString &objectPath) = 0;
    virtual bool pairDevice(const QString &objectPath) = 0;
    virtual bool cancelPairing() = 0;
    virtual bool setDeviceTrusted(const QString &objectPath, bool trusted) = 0;
    virtual bool forgetDevice(const QString &objectPath) = 0;
    virtual bool submitAgentText(quint64 requestId, const QString &text) = 0;
    virtual bool confirmAgentRequest(quint64 requestId, bool accepted) = 0;
    virtual bool rejectAgentRequest(quint64 requestId) = 0;
};

} // namespace Astrea::System

Q_DECLARE_METATYPE(Astrea::System::BluetoothSnapshot)
