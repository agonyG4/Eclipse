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

private:
    void publishSnapshot();

    RustBluetoothEngine m_engine;
    Callbacks m_callbacks;
};

} // namespace Astrea::System
