#pragma once

#include "system/bluetooth/BluetoothBackend.hpp"

#include <astrea_system_backend/src/bluetooth/bridge.cxxqt.h>

#include <QObject>

namespace Astrea::System {

class RustBluetoothBackend final : public QObject, public BluetoothBackend {
public:
    explicit RustBluetoothBackend(QObject *parent = nullptr);
    ~RustBluetoothBackend() override;

    bool start(const Callbacks &callbacks, QString *errorOut) override;
    void stop() override;
    bool setPowered(bool powered) override;
    bool requestScan(const QString &owner) override;
    void releaseScan(const QString &owner) override;
    bool connectDevice(const QString &objectPath) override;
    bool disconnectDevice(const QString &objectPath) override;
    bool pairDevice(const QString &objectPath) override;
    bool cancelPairing() override;
    bool setDeviceTrusted(const QString &objectPath, bool trusted) override;
    bool forgetDevice(const QString &objectPath) override;
    bool submitAgentText(quint64 requestId, const QString &text) override;
    bool confirmAgentRequest(quint64 requestId, bool accepted) override;
    bool rejectAgentRequest(quint64 requestId) override;

private:
    void publishSnapshot();

    RustBluetoothEngine m_engine;
    Callbacks m_callbacks;
};

} // namespace Astrea::System
