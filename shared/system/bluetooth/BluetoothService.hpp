#pragma once

#include "system/SystemServiceState.hpp"
#include "system/bluetooth/BluetoothBackend.hpp"

#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QTimer>

#include <memory>

namespace Astrea::System {

class BluetoothDeviceModel;

class BluetoothService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(SystemServiceState state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool available READ available NOTIFY healthChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY healthChanged)
    Q_PROPERTY(bool adapterAvailable READ adapterAvailable NOTIFY adapterChanged)
    Q_PROPERTY(QString adapterPath READ adapterPath NOTIFY adapterChanged)
    Q_PROPERTY(QString adapterName READ adapterName NOTIFY adapterChanged)
    Q_PROPERTY(bool powered READ powered NOTIFY poweredChanged)
    Q_PROPERTY(bool powerPending READ powerPending NOTIFY powerPendingChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int connectedCount READ connectedCount NOTIFY connectedChanged)
    Q_PROPERTY(QString connectedName READ connectedName NOTIFY connectedChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(BluetoothDeviceModel *devicesModel READ devicesModel CONSTANT)

public:
    explicit BluetoothService(std::unique_ptr<BluetoothBackend> backend = {},
                              QObject *parent = nullptr);
    ~BluetoothService() override;

    SystemServiceState state() const { return m_state; }
    bool available() const { return m_available; }
    bool ready() const { return m_ready; }
    bool adapterAvailable() const { return m_adapterAvailable; }
    QString adapterPath() const { return m_adapterPath; }
    QString adapterName() const { return m_adapterName; }
    bool powered() const { return m_powered; }
    bool powerPending() const { return m_powerPending; }
    bool scanning() const { return m_scanning; }
    int connectedCount() const { return m_connectedCount; }
    QString connectedName() const { return m_connectedName; }
    QString errorString() const { return m_errorString; }
    BluetoothDeviceModel *devicesModel() const { return m_devicesModel; }

    bool start();
    void stop();
    Q_INVOKABLE bool setPowered(bool powered);
    Q_INVOKABLE bool requestScan(const QString &owner);
    Q_INVOKABLE void releaseScan(const QString &owner);
    Q_INVOKABLE bool connectDevice(const QString &objectPath);
    Q_INVOKABLE bool disconnectDevice(const QString &objectPath);
    QJsonObject healthJson() const;

signals:
    void stateChanged();
    void healthChanged();
    void adapterChanged();
    void poweredChanged();
    void powerPendingChanged();
    void scanningChanged();
    void connectedChanged();
    void errorStringChanged();

private:
    void setState(SystemServiceState state);
    void setErrorString(const QString &errorString);
    void applySnapshot(const BluetoothSnapshot &snapshot);
    void reconcileDiscovery();
    void handleOperationFinished(const QString &operation, bool success,
                                 const QString &errorString);
    void finishPowerPending(bool success, const QString &errorString = {});
    void finishDiscoveryRequest(bool success, const QString &errorString = {});

    std::unique_ptr<BluetoothBackend> m_backend;
    BluetoothDeviceModel *m_devicesModel = nullptr;
    QSet<QString> m_scanOwners;
    SystemServiceState m_state = SystemServiceState::Stopped;
    bool m_available = false;
    bool m_ready = false;
    bool m_adapterAvailable = false;
    QString m_adapterPath;
    QString m_adapterName;
    bool m_powered = false;
    bool m_powerPending = false;
    bool m_scanning = false;
    bool m_powerTarget = false;
    bool m_discoveryRequestInFlight = false;
    int m_connectedCount = 0;
    QString m_connectedName;
    QString m_errorString;
    quint64 m_generation = 0;
    QTimer m_powerTimer;
    QTimer m_discoveryTimer;
};

} // namespace Astrea::System
