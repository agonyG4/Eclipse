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
    m_backend->stop();
    m_wifiModel->replace({});
    m_available = false;
    m_ready = false;
    const bool oldWifiAvailable = m_wifiAvailable;
    const bool oldPending = m_wifiPending;
    m_wifiPending = false;
    m_wifiAvailable = false;
    m_wifiScanning = false;
    emit healthChanged();
    if (oldWifiAvailable)
        emit wifiAvailabilityChanged();
    if (oldPending)
        emit wifiPendingChanged();
    emit wifiScanningChanged();
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

QJsonObject NetworkService::healthJson() const
{
    QJsonObject result = serviceHealthJson(m_state, m_available, m_ready, m_errorString);
    result.insert(QStringLiteral("wifiAvailable"), m_wifiAvailable);
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
    const bool healthChangedValue = m_available != snapshot.daemonAvailable;
    m_available = snapshot.daemonAvailable;
    m_ready = snapshot.daemonAvailable;
    const bool oldWifiAvailable = m_wifiAvailable;
    const bool oldWifiScanning = m_wifiScanning;
    const bool oldWifiEnabled = m_wifiEnabled;
    const bool oldWifiPending = m_wifiPending;
    m_wifiAvailable = snapshot.wifiAvailable;
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
    m_downloadBytesPerSecond = snapshot.downloadBytesPerSecond;
    m_uploadBytesPerSecond = snapshot.uploadBytesPerSecond;
    m_wifiModel->replace(snapshot.wifiNetworks);
    if (healthChangedValue)
        emit healthChanged();
    if (oldWifiAvailable != m_wifiAvailable)
        emit wifiAvailabilityChanged();
    if (oldWifiEnabled != m_wifiEnabled)
        emit wifiEnabledChanged();
    if (oldWifiPending != m_wifiPending)
        emit wifiPendingChanged();
    if (oldWifiScanning != m_wifiScanning)
        emit wifiScanningChanged();
    emit connectionChanged();
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
    if (!result.success) {
        if (!result.error.isEmpty())
            setErrorString(result.error);
        setState(SystemServiceState::Degraded);
    }
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
