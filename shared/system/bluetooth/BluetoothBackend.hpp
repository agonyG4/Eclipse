#pragma once

#include "system/bluetooth/BluetoothDeviceModel.hpp"

#include <QString>
#include <QVector>

#include <functional>

namespace Astrea::System {

struct BluetoothSnapshot {
    bool daemonAvailable = false;
    bool adapterAvailable = false;
    QString adapterPath;
    QString adapterName;
    bool powered = false;
    bool powerPending = false;
    bool scanning = false;
    QVector<BluetoothDevice> devices;
};

enum class BluetoothOperationKind {
    Power,
    StartDiscovery,
    StopDiscovery,
    Connect,
    Disconnect,
};

struct BluetoothOperationResult {
    BluetoothOperationKind kind = BluetoothOperationKind::Power;
    quint64 requestId = 0;
    bool success = false;
    QString error;
};

class BluetoothBackend {
public:
    struct Callbacks {
        std::function<void(BluetoothSnapshot)> snapshotChanged;
        std::function<void(QString)> errorChanged;
        std::function<void(BluetoothOperationResult)> operationFinished;
    };

    virtual ~BluetoothBackend() = default;
    virtual bool start(const Callbacks &callbacks, QString *errorOut) = 0;
    virtual void stop() = 0;
    virtual bool setPowered(bool powered, quint64 requestId) = 0;
    virtual bool startDiscovery(quint64 requestId) = 0;
    virtual bool stopDiscovery(quint64 requestId) = 0;
    virtual bool connectDevice(const QString &objectPath) = 0;
    virtual bool disconnectDevice(const QString &objectPath) = 0;
};

} // namespace Astrea::System

Q_DECLARE_METATYPE(Astrea::System::BluetoothSnapshot)
