#pragma once

#include "system/SystemServiceState.hpp"
#include "system/network/NetworkBackend.hpp"

#include <QJsonObject>
#include <QObject>
#include <QTimer>

#include <memory>

namespace Astrea::System {

class WifiNetworkModel;

class NetworkService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(SystemServiceState state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool available READ available NOTIFY healthChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY healthChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(bool wifiAvailable READ wifiAvailable NOTIFY wifiAvailabilityChanged)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY wifiEnabledChanged)
    Q_PROPERTY(bool wifiPending READ wifiPending NOTIFY wifiPendingChanged)
    Q_PROPERTY(bool wifiScanning READ wifiScanning NOTIFY wifiScanningChanged)
    Q_PROPERTY(NetworkConnectionType connectionType READ connectionType NOTIFY connectionChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionName READ connectionName NOTIFY connectionChanged)
    Q_PROPERTY(QString interfaceName READ interfaceName NOTIFY connectionChanged)
    Q_PROPERTY(quint64 downloadBytesPerSecond READ downloadBytesPerSecond NOTIFY trafficChanged)
    Q_PROPERTY(quint64 uploadBytesPerSecond READ uploadBytesPerSecond NOTIFY trafficChanged)
    Q_PROPERTY(QString downloadRate READ downloadRate NOTIFY trafficChanged)
    Q_PROPERTY(QString uploadRate READ uploadRate NOTIFY trafficChanged)
    Q_PROPERTY(WifiNetworkModel *wifiModel READ wifiModel CONSTANT)

public:
    explicit NetworkService(std::unique_ptr<NetworkBackend> backend = {}, QObject *parent = nullptr);
    ~NetworkService() override;

    SystemServiceState state() const { return m_state; }
    bool available() const { return m_available; }
    bool ready() const { return m_ready; }
    QString errorString() const { return m_errorString; }
    bool wifiAvailable() const { return m_wifiAvailable; }
    bool wifiEnabled() const { return m_wifiEnabled; }
    bool wifiPending() const { return m_wifiPending; }
    bool wifiScanning() const { return m_wifiScanning; }
    NetworkConnectionType connectionType() const { return m_connectionType; }
    bool connected() const { return m_connected; }
    QString connectionName() const { return m_connectionName; }
    QString interfaceName() const { return m_interfaceName; }
    quint64 downloadBytesPerSecond() const { return m_downloadBytesPerSecond; }
    quint64 uploadBytesPerSecond() const { return m_uploadBytesPerSecond; }
    QString downloadRate() const { return formatRate(m_downloadBytesPerSecond); }
    QString uploadRate() const { return formatRate(m_uploadBytesPerSecond); }
    WifiNetworkModel *wifiModel() const { return m_wifiModel; }

    bool start();
    void stop();
    Q_INVOKABLE bool setWifiEnabled(bool enabled);
    Q_INVOKABLE bool requestWifiScan();
    QJsonObject healthJson() const;

    static QString formatRate(double bytesPerSecond);

signals:
    void stateChanged();
    void healthChanged();
    void errorStringChanged();
    void wifiAvailabilityChanged();
    void wifiEnabledChanged();
    void wifiPendingChanged();
    void wifiScanningChanged();
    void connectionChanged();
    void trafficChanged();

private:
    void setState(SystemServiceState state);
    void setErrorString(const QString &errorString);
    void applySnapshot(const NetworkSnapshot &snapshot);
    void handleOperationFinished(const QString &operation, bool success,
                                const QString &errorString);
    void finishWifiPending(bool success, const QString &errorString = {});

    std::unique_ptr<NetworkBackend> m_backend;
    WifiNetworkModel *m_wifiModel = nullptr;
    SystemServiceState m_state = SystemServiceState::Stopped;
    bool m_available = false;
    bool m_ready = false;
    QString m_errorString;
    bool m_wifiAvailable = false;
    bool m_wifiEnabled = false;
    bool m_wifiPending = false;
    bool m_wifiTarget = false;
    bool m_wifiScanning = false;
    NetworkConnectionType m_connectionType = NetworkConnectionType::None;
    bool m_connected = false;
    QString m_connectionName;
    QString m_interfaceName;
    quint64 m_downloadBytesPerSecond = 0;
    quint64 m_uploadBytesPerSecond = 0;
    quint64 m_generation = 0;
    QTimer m_wifiTimer;
};

} // namespace Astrea::System
