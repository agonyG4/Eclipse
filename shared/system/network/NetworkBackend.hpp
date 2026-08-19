#pragma once

#include "system/SystemServiceState.hpp"
#include "system/network/WifiNetworkModel.hpp"

#include <QString>
#include <QVector>

#include <functional>

namespace Astrea::System {

struct NetworkSnapshot {
    bool daemonAvailable = false;
    bool wifiAvailable = false;
    bool wifiEnabled = false;
    bool wifiPending = false;
    bool wifiScanning = false;
    NetworkConnectionType connectionType = NetworkConnectionType::None;
    bool connected = false;
    QString connectionName;
    QString interfaceName;
    quint64 downloadBytesPerSecond = 0;
    quint64 uploadBytesPerSecond = 0;
    QVector<WifiNetwork> wifiNetworks;
};

class NetworkBackend {
public:
    struct Callbacks {
        std::function<void(NetworkSnapshot)> snapshotChanged;
        std::function<void(QString)> errorChanged;
        std::function<void(QString, bool, QString)> operationFinished;
    };

    virtual ~NetworkBackend() = default;
    virtual bool start(const Callbacks &callbacks, QString *errorOut) = 0;
    virtual void stop() = 0;
    virtual bool setWifiEnabled(bool enabled) = 0;
    virtual bool requestWifiScan() = 0;
};

} // namespace Astrea::System

Q_DECLARE_METATYPE(Astrea::System::NetworkSnapshot)
