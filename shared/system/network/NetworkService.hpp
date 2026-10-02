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
    Q_PROPERTY(bool wifiHardwareEnabled READ wifiHardwareEnabled NOTIFY wifiHardwareEnabledChanged)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY wifiEnabledChanged)
    Q_PROPERTY(bool wifiPending READ wifiPending NOTIFY wifiPendingChanged)
    Q_PROPERTY(bool wifiScanning READ wifiScanning NOTIFY wifiScanningChanged)
    Q_PROPERTY(NetworkConnectionType connectionType READ connectionType NOTIFY connectionChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionName READ connectionName NOTIFY connectionChanged)
    Q_PROPERTY(QString interfaceName READ interfaceName NOTIFY connectionChanged)
    Q_PROPERTY(QString hardwareAddress READ hardwareAddress NOTIFY connectionChanged)
    Q_PROPERTY(quint32 mtu READ mtu NOTIFY connectionChanged)
    Q_PROPERTY(bool linkCarrier READ linkCarrier NOTIFY connectionChanged)
    Q_PROPERTY(quint32 linkSpeedMbps READ linkSpeedMbps NOTIFY connectionChanged)
    Q_PROPERTY(QStringList ipv4Addresses READ ipv4Addresses NOTIFY connectionChanged)
    Q_PROPERTY(QString ipv4Gateway READ ipv4Gateway NOTIFY connectionChanged)
    Q_PROPERTY(QStringList ipv6Addresses READ ipv6Addresses NOTIFY connectionChanged)
    Q_PROPERTY(QString ipv6Gateway READ ipv6Gateway NOTIFY connectionChanged)
    Q_PROPERTY(QStringList activeIpv4DnsServers READ activeIpv4DnsServers NOTIFY dnsChanged)
    Q_PROPERTY(QStringList activeIpv6DnsServers READ activeIpv6DnsServers NOTIFY dnsChanged)
    Q_PROPERTY(NetworkDnsMode dnsMode READ dnsMode NOTIFY dnsChanged)
    Q_PROPERTY(QStringList configuredIpv4DnsServers READ configuredIpv4DnsServers NOTIFY dnsChanged)
    Q_PROPERTY(QStringList configuredIpv6DnsServers READ configuredIpv6DnsServers NOTIFY dnsChanged)
    Q_PROPERTY(bool dnsEditable READ dnsEditable NOTIFY dnsChanged)
    Q_PROPERTY(bool dnsPending READ dnsPending NOTIFY dnsChanged)
    Q_PROPERTY(bool dnsOperationSlow READ dnsOperationSlow NOTIFY dnsChanged)
    Q_PROPERTY(QString dnsErrorString READ dnsErrorString NOTIFY dnsChanged)
    Q_PROPERTY(bool dnsNeedsReconnect READ dnsNeedsReconnect NOTIFY dnsChanged)
    Q_PROPERTY(quint64 dnsAuthorityRevision READ dnsAuthorityRevision NOTIFY dnsChanged)
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
    bool wifiHardwareEnabled() const { return m_wifiHardwareEnabled; }
    bool wifiEnabled() const { return m_wifiEnabled; }
    bool wifiPending() const { return m_wifiPending; }
    bool wifiScanning() const { return m_wifiScanning; }
    NetworkConnectionType connectionType() const { return m_connectionType; }
    bool connected() const { return m_connected; }
    QString connectionName() const { return m_connectionName; }
    QString interfaceName() const { return m_interfaceName; }
    QString hardwareAddress() const { return m_hardwareAddress; }
    quint32 mtu() const { return m_mtu; }
    bool linkCarrier() const { return m_linkCarrier; }
    quint32 linkSpeedMbps() const { return m_linkSpeedMbps; }
    QStringList ipv4Addresses() const { return m_ipv4Addresses; }
    QString ipv4Gateway() const { return m_ipv4Gateway; }
    QStringList ipv6Addresses() const { return m_ipv6Addresses; }
    QString ipv6Gateway() const { return m_ipv6Gateway; }
    QStringList activeIpv4DnsServers() const { return m_activeIpv4DnsServers; }
    QStringList activeIpv6DnsServers() const { return m_activeIpv6DnsServers; }
    NetworkDnsMode dnsMode() const { return m_dnsMode; }
    QStringList configuredIpv4DnsServers() const { return m_configuredIpv4DnsServers; }
    QStringList configuredIpv6DnsServers() const { return m_configuredIpv6DnsServers; }
    bool dnsEditable() const { return m_dnsEditable; }
    bool dnsPending() const { return m_dnsPending; }
    bool dnsOperationSlow() const { return m_dnsOperationSlow; }
    QString dnsErrorString() const { return m_dnsErrorString; }
    bool dnsNeedsReconnect() const { return m_dnsNeedsReconnect; }
    quint64 dnsAuthorityRevision() const { return m_dnsAuthorityRevision; }
    quint64 downloadBytesPerSecond() const { return m_downloadBytesPerSecond; }
    quint64 uploadBytesPerSecond() const { return m_uploadBytesPerSecond; }
    QString downloadRate() const { return formatRate(m_downloadBytesPerSecond); }
    QString uploadRate() const { return formatRate(m_uploadBytesPerSecond); }
    WifiNetworkModel *wifiModel() const { return m_wifiModel; }

    bool start();
    void stop();
    Q_INVOKABLE bool setWifiEnabled(bool enabled);
    Q_INVOKABLE bool requestWifiScan();
    Q_INVOKABLE bool applyDns(NetworkDnsMode mode, const QStringList &servers);
    Q_INVOKABLE QVariantMap validateDnsServers(const QStringList &servers) const;
    QJsonObject healthJson() const;

    static QString formatRate(double bytesPerSecond);

signals:
    void stateChanged();
    void healthChanged();
    void errorStringChanged();
    void wifiAvailabilityChanged();
    void wifiHardwareEnabledChanged();
    void wifiEnabledChanged();
    void wifiPendingChanged();
    void wifiScanningChanged();
    void connectionChanged();
    void trafficChanged();
    void dnsChanged();
    void dnsOperationFinished(NetworkDnsOutcome outcome, const QString &error);

private:
    void setState(SystemServiceState state);
    void setErrorString(const QString &errorString);
    void applySnapshot(const NetworkSnapshot &snapshot);
    void handleOperationFinished(const NetworkOperationResult &result);
    void finishWifiPending(bool success, const QString &errorString = {});
    void finishDnsPending(const NetworkOperationResult &result);
    void setDnsErrorString(const QString &errorString);

    std::unique_ptr<NetworkBackend> m_backend;
    WifiNetworkModel *m_wifiModel = nullptr;
    SystemServiceState m_state = SystemServiceState::Stopped;
    bool m_available = false;
    bool m_ready = false;
    QString m_errorString;
    bool m_wifiAvailable = false;
    bool m_wifiHardwareEnabled = false;
    bool m_wifiEnabled = false;
    bool m_wifiPending = false;
    bool m_wifiTarget = false;
    bool m_wifiScanning = false;
    NetworkConnectionType m_connectionType = NetworkConnectionType::None;
    bool m_connected = false;
    QString m_connectionName;
    QString m_interfaceName;
    QString m_hardwareAddress;
    quint32 m_mtu = 0;
    bool m_linkCarrier = false;
    quint32 m_linkSpeedMbps = 0;
    QStringList m_ipv4Addresses;
    QString m_ipv4Gateway;
    QStringList m_ipv6Addresses;
    QString m_ipv6Gateway;
    QStringList m_activeIpv4DnsServers;
    QStringList m_activeIpv6DnsServers;
    NetworkDnsMode m_dnsMode = NetworkDnsMode::Advanced;
    QStringList m_configuredIpv4DnsServers;
    QStringList m_configuredIpv6DnsServers;
    bool m_dnsEditable = false;
    bool m_dnsPending = false;
    bool m_dnsOperationSlow = false;
    QString m_dnsErrorString;
    bool m_dnsNeedsReconnect = false;
    quint64 m_dnsAuthorityRevision = 0;
    QString m_activeConnectionPath;
    QString m_profilePath;
    QString m_devicePath;
    quint64 m_downloadBytesPerSecond = 0;
    quint64 m_uploadBytesPerSecond = 0;
    quint64 m_generation = 0;
    quint64 m_wifiOperationId = 0;
    quint64 m_wifiRequestId = 0;
    QTimer m_wifiTimer;
    QTimer m_dnsSlowTimer;
    quint64 m_dnsOperationId = 0;
    quint64 m_dnsRequestId = 0;
};

} // namespace Astrea::System
