#include "system/network/NetworkService.hpp"

#include "system/network/NetworkManagerBackend.hpp"
#include "system/network/WifiNetworkModel.hpp"

#include <QMetaObject>
#include <QPointer>

#include <algorithm>

namespace Astrea::System {

NetworkService::NetworkService(std::unique_ptr<NetworkBackend> backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend ? std::move(backend)
                        : std::make_unique<NetworkManagerBackend>())
    , m_wifiModel(new WifiNetworkModel(this))
{
    m_wifiTimer.setSingleShot(true);
    m_wifiTimer.setInterval(3000);
    connect(&m_wifiTimer, &QTimer::timeout, this, [this] {
        if (m_wifiPending)
            finishWifiPending(false, QStringLiteral("Wi-Fi power update timed out"));
    });
    m_dnsSlowTimer.setSingleShot(true);
    m_dnsSlowTimer.setInterval(5000);
    connect(&m_dnsSlowTimer, &QTimer::timeout, this, [this] {
        if (!m_dnsPending || m_dnsOperationSlow)
            return;
        m_dnsOperationSlow = true;
        emit dnsChanged();
    });
}

NetworkService::~NetworkService()
{
    stop();
}

bool NetworkService::start()
{
    if (m_state != SystemServiceState::Stopped)
        return true;
    ++m_generation;
    m_wifiRequestId = 0;
    m_dnsRequestId = 0;
    setState(SystemServiceState::Starting);
    setErrorString({});
    const QPointer<NetworkService> self(this);
    const quint64 generation = m_generation;
    NetworkBackend::Callbacks callbacks;
    callbacks.snapshotChanged = [self, generation](NetworkSnapshot snapshot) {
        if (!self)
            return;
        QMetaObject::invokeMethod(self, [self, generation,
                                          snapshot = std::move(snapshot)]() mutable {
            if (self && self->m_generation == generation
                && self->m_state != SystemServiceState::Stopped)
                self->applySnapshot(std::move(snapshot));
        }, Qt::QueuedConnection);
    };
    callbacks.errorChanged = [self, generation](QString errorString) {
        if (!self)
            return;
        QMetaObject::invokeMethod(self, [self, generation,
                                          errorString = std::move(errorString)] {
            if (!self || self->m_generation != generation
                || self->m_state == SystemServiceState::Stopped)
                return;
            self->setErrorString(errorString);
            self->setState(SystemServiceState::Degraded);
        }, Qt::QueuedConnection);
    };
    callbacks.operationFinished = [self, generation](NetworkOperationResult result) {
        if (!self)
            return;
        QMetaObject::invokeMethod(self, [self, generation, result = std::move(result)] {
            if (self && self->m_generation == generation
                && self->m_state != SystemServiceState::Stopped)
                self->handleOperationFinished(result);
        }, Qt::QueuedConnection);
    };
    QString error;
    if (!m_backend->start(callbacks, &error)) {
        setErrorString(error.isEmpty() ? QStringLiteral("Network backend unavailable") : error);
        setState(SystemServiceState::Unavailable);
    } else {
        m_available = true;
        m_ready = true;
        emit healthChanged();
        setState(SystemServiceState::Ready);
    }
    return true;
}

void NetworkService::stop()
{
    if (m_state == SystemServiceState::Stopped)
        return;
    ++m_generation;
    m_wifiRequestId = 0;
    m_wifiTimer.stop();
    m_dnsSlowTimer.stop();
    m_backend->stop();
    m_wifiModel->replace({});
    m_available = false;
    m_ready = false;
    const bool oldWifiAvailable = m_wifiAvailable;
    const bool oldWifiHardwareEnabled = m_wifiHardwareEnabled;
    const bool oldPending = m_wifiPending;
    const bool dnsIdentityChanged = !m_activeConnectionPath.isEmpty()
        || !m_profilePath.isEmpty() || !m_devicePath.isEmpty();
    if (dnsIdentityChanged)
        ++m_dnsAuthorityRevision;
    const bool oldDnsState = m_dnsPending || m_dnsOperationSlow || m_dnsEditable || m_dnsNeedsReconnect
        || !m_dnsErrorString.isEmpty() || !m_profilePath.isEmpty()
        || dnsIdentityChanged
        || !m_activeIpv4DnsServers.isEmpty() || !m_activeIpv6DnsServers.isEmpty()
        || !m_configuredIpv4DnsServers.isEmpty() || !m_configuredIpv6DnsServers.isEmpty();
    m_wifiPending = false;
    m_dnsPending = false;
    m_dnsOperationSlow = false;
    m_dnsRequestId = 0;
    m_wifiAvailable = false;
    m_wifiHardwareEnabled = false;
    m_wifiScanning = false;
    m_connected = false;
    m_connectionType = NetworkConnectionType::None;
    m_connectionName.clear();
    m_interfaceName.clear();
    m_hardwareAddress.clear();
    m_mtu = 0;
    m_linkCarrier = false;
    m_linkSpeedMbps = 0;
    m_ipv4Addresses.clear();
    m_ipv4Gateway.clear();
    m_ipv6Addresses.clear();
    m_ipv6Gateway.clear();
    m_activeIpv4DnsServers.clear();
    m_activeIpv6DnsServers.clear();
    m_dnsMode = NetworkDnsMode::Advanced;
    m_configuredIpv4DnsServers.clear();
    m_configuredIpv6DnsServers.clear();
    m_dnsEditable = false;
    m_dnsErrorString.clear();
    m_dnsNeedsReconnect = false;
    m_activeConnectionPath.clear();
    m_profilePath.clear();
    m_devicePath.clear();
    m_downloadBytesPerSecond = 0;
    m_uploadBytesPerSecond = 0;
    emit healthChanged();
    if (oldWifiAvailable)
        emit wifiAvailabilityChanged();
    if (oldWifiHardwareEnabled)
        emit wifiHardwareEnabledChanged();
    if (oldPending)
        emit wifiPendingChanged();
    if (oldDnsState)
        emit dnsChanged();
    emit wifiScanningChanged();
    emit connectionChanged();
    emit trafficChanged();
    setState(SystemServiceState::Stopped);
}

bool NetworkService::setWifiEnabled(bool enabled)
{
    const quint64 requestId = ++m_wifiOperationId;
    if (!m_backend->setWifiEnabled(enabled, requestId))
        return false;
    m_wifiTarget = enabled;
    m_wifiRequestId = requestId;
    if (!m_wifiPending) {
        m_wifiPending = true;
        emit wifiPendingChanged();
    }
    m_wifiTimer.start();
    return true;
}

bool NetworkService::requestWifiScan()
{
    return m_backend->requestWifiScan();
}

bool NetworkService::applyDns(NetworkDnsMode mode, const QStringList &servers)
{
    if (m_dnsPending)
        return false;
    if (!m_ready || !m_connected || !m_dnsEditable || m_activeConnectionPath.isEmpty()
        || m_profilePath.isEmpty() || m_devicePath.isEmpty()) {
        setDnsErrorString(QStringLiteral("DNS settings are unavailable for this connection"));
        emit dnsChanged();
        return false;
    }
    if (mode == NetworkDnsMode::Advanced) {
        setDnsErrorString(QStringLiteral("Choose Automatic or Custom DNS to apply settings"));
        emit dnsChanged();
        return false;
    }
    QStringList canonicalServers;
    if (mode == NetworkDnsMode::Custom) {
        const NetworkDnsValidation validation = canonicalizeDnsServers(servers);
        if (!validation.valid) {
            setDnsErrorString(validation.error);
            emit dnsChanged();
            return false;
        }
        canonicalServers = validation.servers;
    }

    const quint64 requestId = ++m_dnsOperationId;
    QString error;
    if (!m_backend->applyDns(mode, canonicalServers, requestId, &error)) {
        setDnsErrorString(error.isEmpty() ? QStringLiteral("Could not start DNS update") : error);
        emit dnsChanged();
        return false;
    }
    m_dnsRequestId = requestId;
    m_dnsPending = true;
    m_dnsOperationSlow = false;
    setDnsErrorString({});
    m_dnsSlowTimer.start();
    emit dnsChanged();
    return true;
}

QVariantMap NetworkService::validateDnsServers(const QStringList &servers) const
{
    const NetworkDnsValidation validation = canonicalizeDnsServers(servers);
    return {{QStringLiteral("valid"), validation.valid},
            {QStringLiteral("servers"), validation.servers},
            {QStringLiteral("ipv4Servers"), validation.ipv4Servers},
            {QStringLiteral("ipv6Servers"), validation.ipv6Servers},
            {QStringLiteral("error"), validation.error}};
}

QJsonObject NetworkService::healthJson() const
{
    QJsonObject result = serviceHealthJson(m_state, m_available, m_ready, m_errorString);
    result.insert(QStringLiteral("wifiAvailable"), m_wifiAvailable);
    result.insert(QStringLiteral("wifiHardwareEnabled"), m_wifiHardwareEnabled);
    result.insert(QStringLiteral("wifiEnabled"), m_wifiEnabled);
    result.insert(QStringLiteral("wifiScanning"), m_wifiScanning);
    result.insert(QStringLiteral("connected"), m_connected);
    result.insert(QStringLiteral("connectionType"), static_cast<int>(m_connectionType));
    result.insert(QStringLiteral("downloadRate"), downloadRate());
    result.insert(QStringLiteral("uploadRate"), uploadRate());
    return result;
}

QString NetworkService::formatRate(double bytesPerSecond)
{
    const double value = std::max(0.0, bytesPerSecond);
    if (value < 1000.0)
        return QStringLiteral("%1 B/s").arg(qRound64(value));
    if (value < 1'000'000.0)
        return QStringLiteral("%1 KB/s").arg(value / 1000.0, 0, 'f', 1);
    if (value < 1'000'000'000.0)
        return QStringLiteral("%1 MB/s").arg(value / 1'000'000.0, 0, 'f', 1);
    return QStringLiteral("%1 GB/s").arg(value / 1'000'000'000.0, 0, 'f', 1);
}

void NetworkService::setState(SystemServiceState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
    emit healthChanged();
}

void NetworkService::setErrorString(const QString &errorString)
{
    if (m_errorString == errorString)
        return;
    m_errorString = errorString;
    emit errorStringChanged();
    emit healthChanged();
}

void NetworkService::applySnapshot(const NetworkSnapshot &snapshot)
{
    const bool dnsIdentityChanged = m_activeConnectionPath != snapshot.activeConnectionPath
        || m_profilePath != snapshot.profilePath || m_devicePath != snapshot.devicePath;
    if (dnsIdentityChanged)
        ++m_dnsAuthorityRevision;
    m_activeConnectionPath = snapshot.activeConnectionPath;
    m_profilePath = snapshot.profilePath;
    m_devicePath = snapshot.devicePath;
    if (dnsIdentityChanged) {
        m_dnsNeedsReconnect = false;
        setDnsErrorString({});
    }
    const bool healthChangedValue = m_available != snapshot.daemonAvailable;
    m_available = snapshot.daemonAvailable;
    m_ready = snapshot.daemonAvailable;
    const bool oldWifiAvailable = m_wifiAvailable;
    const bool oldWifiScanning = m_wifiScanning;
    const bool oldWifiHardwareEnabled = m_wifiHardwareEnabled;
    const bool oldWifiEnabled = m_wifiEnabled;
    const bool oldWifiPending = m_wifiPending;
    m_wifiAvailable = snapshot.wifiAvailable;
    m_wifiHardwareEnabled = snapshot.wifiHardwareEnabled;
    m_wifiEnabled = snapshot.wifiEnabled;
    if (!snapshot.daemonAvailable || !snapshot.wifiAvailable) {
        m_wifiRequestId = 0;
        m_wifiPending = false;
        m_wifiTimer.stop();
    } else if (m_wifiPending && m_wifiEnabled == m_wifiTarget) {
        m_wifiTimer.stop();
        m_wifiRequestId = 0;
        m_wifiPending = false;
    }
    m_wifiScanning = snapshot.wifiScanning;
    m_connectionType = snapshot.connectionType;
    m_connected = snapshot.connected;
    m_connectionName = snapshot.connectionName;
    m_interfaceName = snapshot.interfaceName;
    const bool hasRuntimeConnection = snapshot.daemonAvailable && snapshot.connected;
    m_hardwareAddress = hasRuntimeConnection ? snapshot.hardwareAddress : QString();
    m_mtu = hasRuntimeConnection ? snapshot.mtu : 0;
    m_linkCarrier = hasRuntimeConnection && snapshot.linkCarrier;
    m_linkSpeedMbps = hasRuntimeConnection ? snapshot.linkSpeedMbps : 0;
    m_ipv4Addresses = hasRuntimeConnection ? snapshot.ipv4Addresses : QStringList();
    m_ipv4Gateway = hasRuntimeConnection ? snapshot.ipv4Gateway : QString();
    m_ipv6Addresses = hasRuntimeConnection ? snapshot.ipv6Addresses : QStringList();
    m_ipv6Gateway = hasRuntimeConnection ? snapshot.ipv6Gateway : QString();
    m_activeIpv4DnsServers = hasRuntimeConnection ? snapshot.activeIpv4DnsServers : QStringList();
    m_activeIpv6DnsServers = hasRuntimeConnection ? snapshot.activeIpv6DnsServers : QStringList();
    m_dnsMode = snapshot.dnsMode;
    m_configuredIpv4DnsServers = snapshot.configuredIpv4DnsServers;
    m_configuredIpv6DnsServers = snapshot.configuredIpv6DnsServers;
    m_dnsEditable = snapshot.daemonAvailable && snapshot.connected && snapshot.dnsEditable;
    m_downloadBytesPerSecond = snapshot.downloadBytesPerSecond;
    m_uploadBytesPerSecond = snapshot.uploadBytesPerSecond;
    m_wifiModel->replace(snapshot.wifiNetworks);
    if (healthChangedValue)
        emit healthChanged();
    if (oldWifiAvailable != m_wifiAvailable)
        emit wifiAvailabilityChanged();
    if (oldWifiHardwareEnabled != m_wifiHardwareEnabled)
        emit wifiHardwareEnabledChanged();
    if (oldWifiEnabled != m_wifiEnabled)
        emit wifiEnabledChanged();
    if (oldWifiPending != m_wifiPending)
        emit wifiPendingChanged();
    if (oldWifiScanning != m_wifiScanning)
        emit wifiScanningChanged();
    emit connectionChanged();
    emit dnsChanged();
    emit trafficChanged();
    setState(snapshot.daemonAvailable ? SystemServiceState::Ready
                                      : SystemServiceState::Unavailable);
    if (!m_wifiPending)
        m_wifiTimer.stop();
}

void NetworkService::handleOperationFinished(const NetworkOperationResult &result)
{
    if (result.kind == NetworkOperationKind::WifiEnabled) {
        if (!m_wifiPending || result.requestId != m_wifiRequestId)
            return;
        if (!result.success)
            finishWifiPending(false, result.error);
        return;
    }
    if (result.kind == NetworkOperationKind::DnsConfiguration) {
        if (!m_dnsPending || result.requestId != m_dnsRequestId)
            return;
        finishDnsPending(result);
    }
}

void NetworkService::finishDnsPending(const NetworkOperationResult &result)
{
    m_dnsSlowTimer.stop();
    m_dnsRequestId = 0;
    m_dnsPending = false;
    m_dnsOperationSlow = false;
    QString error = result.error;
    switch (result.dnsOutcome) {
    case NetworkDnsOutcome::Applied:
        m_dnsNeedsReconnect = false;
        error.clear();
        break;
    case NetworkDnsOutcome::SavedNeedsReconnect:
        m_dnsNeedsReconnect = result.dnsAuthorityCurrent;
        error.clear();
        break;
    case NetworkDnsOutcome::Conflict:
        error = QStringLiteral("The connection changed while editing. Reload the current settings and try again.");
        break;
    case NetworkDnsOutcome::Indeterminate:
        if (error.isEmpty())
            error = QStringLiteral("Astrea could not confirm whether the DNS settings were saved. Reload the current connection settings before trying again.");
        break;
    case NetworkDnsOutcome::Failed:
        if (error.isEmpty())
            error = QStringLiteral("Could not save DNS settings");
        break;
    }
    setDnsErrorString(error);
    emit dnsOperationFinished(result.dnsOutcome, error);
    emit dnsChanged();
}

void NetworkService::setDnsErrorString(const QString &errorString)
{
    m_dnsErrorString = errorString;
}

void NetworkService::finishWifiPending(bool success, const QString &errorString)
{
    m_wifiTimer.stop();
    m_wifiRequestId = 0;
    if (!success) {
        setErrorString(errorString.isEmpty() ? QStringLiteral("Wi-Fi power update failed")
                                             : errorString);
        setState(SystemServiceState::Degraded);
    }
    if (!m_wifiPending)
        return;
    m_wifiPending = false;
    emit wifiPendingChanged();
}

} // namespace Astrea::System
