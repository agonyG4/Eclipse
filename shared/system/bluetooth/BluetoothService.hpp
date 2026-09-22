#pragma once

#include "system/SystemServiceState.hpp"
#include "system/bluetooth/BluetoothBackend.hpp"

#include <QJsonObject>
#include <QObject>
#include <QString>

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
    Q_PROPERTY(bool pairing READ pairing NOTIFY pairingChanged)
    Q_PROPERTY(QString pairingDevicePath READ pairingDevicePath NOTIFY pairingChanged)
    Q_PROPERTY(QString pairingDeviceName READ pairingDeviceName NOTIFY pairingChanged)
    Q_PROPERTY(QString pairingError READ pairingError NOTIFY pairingChanged)
    Q_PROPERTY(bool agentRequestActive READ agentRequestActive NOTIFY agentRequestChanged)
    Q_PROPERTY(quint64 agentRequestId READ agentRequestId NOTIFY agentRequestChanged)
    Q_PROPERTY(BluetoothAgentRequestKind agentRequestKind READ agentRequestKind NOTIFY agentRequestChanged)
    Q_PROPERTY(QString agentDevicePath READ agentDevicePath NOTIFY agentRequestChanged)
    Q_PROPERTY(QString agentDeviceName READ agentDeviceName NOTIFY agentRequestChanged)
    Q_PROPERTY(quint32 agentPasskey READ agentPasskey NOTIFY agentRequestChanged)
    Q_PROPERTY(int agentEntered READ agentEntered NOTIFY agentRequestChanged)
    Q_PROPERTY(QString agentServiceUuid READ agentServiceUuid NOTIFY agentRequestChanged)
    Q_PROPERTY(QString agentDisplayPin READ agentDisplayPin NOTIFY agentRequestChanged)
    Q_PROPERTY(QString operationError READ operationError NOTIFY operationChanged)
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
    bool pairing() const { return m_pairing; }
    QString pairingDevicePath() const { return m_pairingDevicePath; }
    QString pairingDeviceName() const { return m_pairingDeviceName; }
    QString pairingError() const { return m_pairingError; }
    bool agentRequestActive() const { return m_agentRequestActive; }
    quint64 agentRequestId() const { return m_agentRequestId; }
    BluetoothAgentRequestKind agentRequestKind() const { return m_agentRequestKind; }
    QString agentDevicePath() const { return m_agentDevicePath; }
    QString agentDeviceName() const { return m_agentDeviceName; }
    quint32 agentPasskey() const { return m_agentPasskey; }
    int agentEntered() const { return m_agentEntered; }
    QString agentServiceUuid() const { return m_agentServiceUuid; }
    QString agentDisplayPin() const { return m_agentDisplayPin; }
    QString operationError() const { return m_operationError; }
    BluetoothDeviceModel *devicesModel() const { return m_devicesModel; }

    bool start();
    void stop();
    Q_INVOKABLE bool setPowered(bool powered);
    Q_INVOKABLE bool requestScan(const QString &owner);
    Q_INVOKABLE void releaseScan(const QString &owner);
    Q_INVOKABLE bool connectDevice(const QString &objectPath);
    Q_INVOKABLE bool disconnectDevice(const QString &objectPath);
    Q_INVOKABLE bool pairDevice(const QString &objectPath);
    Q_INVOKABLE bool cancelPairing();
    Q_INVOKABLE bool setDeviceTrusted(const QString &objectPath, bool trusted);
    Q_INVOKABLE bool forgetDevice(const QString &objectPath);
    Q_INVOKABLE bool submitAgentText(quint64 requestId, const QString &text);
    Q_INVOKABLE bool confirmAgentRequest(quint64 requestId, bool accepted);
    Q_INVOKABLE bool rejectAgentRequest(quint64 requestId);
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
    void pairingChanged();
    void agentRequestChanged();
    void operationChanged();

private:
    void applySnapshot(const BluetoothSnapshot &snapshot);
    void setState(SystemServiceState state, bool notifyHealth = true);
    void setErrorString(const QString &errorString, bool notifyHealth = true);

    std::unique_ptr<BluetoothBackend> m_backend;
    BluetoothDeviceModel *m_devicesModel = nullptr;
    SystemServiceState m_state = SystemServiceState::Stopped;
    bool m_available = false;
    bool m_ready = false;
    bool m_adapterAvailable = false;
    QString m_adapterPath;
    QString m_adapterName;
    bool m_powered = false;
    bool m_powerPending = false;
    bool m_scanning = false;
    int m_connectedCount = 0;
    QString m_connectedName;
    QString m_errorString;
    bool m_pairing = false;
    QString m_pairingDevicePath;
    QString m_pairingDeviceName;
    QString m_pairingError;
    bool m_agentRequestActive = false;
    quint64 m_agentRequestId = 0;
    BluetoothAgentRequestKind m_agentRequestKind = BluetoothAgentRequestKind::None;
    QString m_agentDevicePath;
    QString m_agentDeviceName;
    quint32 m_agentPasskey = 0;
    int m_agentEntered = -1;
    QString m_agentServiceUuid;
    QString m_agentDisplayPin;
    QString m_operationError;
};

} // namespace Astrea::System
