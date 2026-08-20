#include "system/bluetooth/BluetoothService.hpp"

#include "system/bluetooth/BluetoothDeviceModel.hpp"
#include "system/bluetooth/BluezBackend.hpp"

#include <QMetaObject>
#include <QPointer>

#include <algorithm>
#include <iterator>

namespace Astrea::System {

BluetoothService::BluetoothService(std::unique_ptr<BluetoothBackend> backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend ? std::move(backend)
                        : std::make_unique<BluezBackend>())
    , m_devicesModel(new BluetoothDeviceModel(this))
{
    m_powerTimer.setSingleShot(true);
    m_powerTimer.setInterval(3000);
    connect(&m_powerTimer, &QTimer::timeout, this, [this] {
        if (!m_powerPending)
            return;
        finishPowerPending(false, QStringLiteral("Bluetooth power update timed out"));
    });
    m_discoveryTimer.setSingleShot(true);
    m_discoveryTimer.setInterval(3000);
    connect(&m_discoveryTimer, &QTimer::timeout, this, [this] {
        if (!m_discoveryRequestInFlight)
            return;
        const bool start = m_discoveryState.lease() == BluezDiscoveryLease::StartPending;
        m_discoveryRequestId = 0;
        m_discoveryRequestInFlight = false;
        m_discoveryState.operationFinished(start, false);
        finishDiscoveryRequest(false, QStringLiteral("Bluetooth discovery update timed out"));
        if (start)
            scheduleDiscoveryRetry();
        reconcileDiscovery();
    });
    m_discoveryRetryTimer.setSingleShot(true);
    connect(&m_discoveryRetryTimer, &QTimer::timeout, this, [this] {
        if (!m_discoveryState.hasDemand() || !m_adapterAvailable || !m_powered
            || !m_available)
            return;
        reconcileDiscovery();
    });
}

BluetoothService::~BluetoothService()
{
    stop();
}

bool BluetoothService::start()
{
    if (m_state != SystemServiceState::Stopped)
        return true;
    ++m_generation;
    m_discoveryState.reset();
    m_discoveryRequestId = 0;
    cancelDiscoveryRetry();
    setState(SystemServiceState::Starting);
    setErrorString({});
    const QPointer<BluetoothService> self(this);
    const quint64 generation = m_generation;
    BluetoothBackend::Callbacks callbacks;
    callbacks.snapshotChanged = [self, generation](BluetoothSnapshot snapshot) {
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
    callbacks.operationFinished = [self, generation](BluetoothOperationResult result) {
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
        setErrorString(error.isEmpty() ? QStringLiteral("Bluetooth backend unavailable") : error);
        setState(SystemServiceState::Unavailable);
    } else {
        m_available = true;
        m_ready = true;
        emit healthChanged();
        setState(SystemServiceState::Ready);
    }
    return true;
}

void BluetoothService::stop()
{
    if (m_state == SystemServiceState::Stopped)
        return;
    ++m_generation;
    m_powerTimer.stop();
    m_discoveryTimer.stop();
    cancelDiscoveryRetry();
    if (m_discoveryState.lease() == BluezDiscoveryLease::Held
        || m_discoveryState.lease() == BluezDiscoveryLease::StartPending
        || m_discoveryState.lease() == BluezDiscoveryLease::StopPending)
        m_backend->stopDiscovery(++m_discoveryOperationId);
    m_scanOwners.clear();
    m_discoveryState.reset();
    m_backend->stop();
    m_devicesModel->replace({});
    m_available = false;
    m_ready = false;
    const bool oldScanning = m_scanning;
    const bool oldPending = m_powerPending;
    m_scanning = false;
    m_discoveryRequestInFlight = false;
    m_discoveryRequestId = 0;
    m_powerPending = false;
    emit healthChanged();
    if (oldScanning)
        emit scanningChanged();
    if (oldPending)
        emit powerPendingChanged();
    setState(SystemServiceState::Stopped);
}

bool BluetoothService::setPowered(bool powered)
{
    if (!m_backend->setPowered(powered))
        return false;
    m_powerTarget = powered;
    if (!m_powerPending) {
        m_powerPending = true;
        emit powerPendingChanged();
    }
    m_powerTimer.start();
    return true;
}

bool BluetoothService::requestScan(const QString &owner)
{
    if (owner.isEmpty())
        return false;
    const bool wasEmpty = m_scanOwners.isEmpty();
    m_scanOwners.insert(owner);
    m_discoveryState.request(owner);
    if (wasEmpty)
        reconcileDiscovery();
    return true;
}

void BluetoothService::releaseScan(const QString &owner)
{
    if (!m_scanOwners.remove(owner))
        return;
    m_discoveryState.release(owner);
    if (m_scanOwners.isEmpty())
        cancelDiscoveryRetry();
    reconcileDiscovery();
}

bool BluetoothService::connectDevice(const QString &objectPath)
{
    if (objectPath.isEmpty())
        return false;
    bool paired = false;
    for (int row = 0; row < m_devicesModel->rowCount(); ++row) {
        const QModelIndex index = m_devicesModel->index(row, 0);
        if (m_devicesModel->data(index, BluetoothDeviceModel::ObjectPathRole).toString()
            != objectPath)
            continue;
        paired = m_devicesModel->data(index, BluetoothDeviceModel::PairedRole).toBool();
        break;
    }
    if (!paired)
        return false;
    return m_backend->connectDevice(objectPath);
}

bool BluetoothService::disconnectDevice(const QString &objectPath)
{
    if (objectPath.isEmpty())
        return false;
    return m_backend->disconnectDevice(objectPath);
}

QJsonObject BluetoothService::healthJson() const
{
    QJsonObject result = serviceHealthJson(m_state, m_available, m_ready, m_errorString);
    result.insert(QStringLiteral("adapterAvailable"), m_adapterAvailable);
    result.insert(QStringLiteral("powered"), m_powered);
    result.insert(QStringLiteral("scanning"), m_scanning);
    result.insert(QStringLiteral("connectedCount"), m_connectedCount);
    return result;
}

void BluetoothService::setState(SystemServiceState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
    emit healthChanged();
}

void BluetoothService::setErrorString(const QString &errorString)
{
    if (m_errorString == errorString)
        return;
    m_errorString = errorString;
    emit errorStringChanged();
    emit healthChanged();
}

void BluetoothService::applySnapshot(const BluetoothSnapshot &snapshot)
{
    const bool oldAdapterAvailable = m_adapterAvailable;
    const bool oldPowered = m_powered;
    const bool oldPending = m_powerPending;
    const bool oldScanning = m_scanning;
    m_available = snapshot.daemonAvailable;
    m_ready = snapshot.daemonAvailable;
    m_adapterAvailable = snapshot.adapterAvailable;
    m_adapterPath = snapshot.adapterPath;
    m_adapterName = snapshot.adapterName;
    m_powered = snapshot.powered;
    if (!snapshot.daemonAvailable)
        m_powerPending = false;
    else if (m_powerPending && m_powered == m_powerTarget)
        m_powerPending = false;
    m_scanning = snapshot.scanning;
    m_discoveryState.setAdapterReady(snapshot.daemonAvailable && snapshot.adapterAvailable
                                     && snapshot.powered);
    m_discoveryState.setActualDiscovering(snapshot.scanning);
    if (!snapshot.daemonAvailable || !snapshot.adapterAvailable || !snapshot.powered) {
        m_discoveryRequestId = 0;
        m_discoveryRequestInFlight = false;
        m_discoveryTimer.stop();
        cancelDiscoveryRetry();
    }
    m_devicesModel->replace(snapshot.devices);
    m_connectedCount = 0;
    m_connectedName.clear();
    for (const BluetoothDevice &device : snapshot.devices) {
        if (!device.connected)
            continue;
        ++m_connectedCount;
        if (m_connectedName.isEmpty())
            m_connectedName = device.name;
    }
    if (oldAdapterAvailable != m_adapterAvailable)
        emit adapterChanged();
    if (oldPowered != m_powered)
        emit poweredChanged();
    if (oldPending != m_powerPending)
        emit powerPendingChanged();
    if (oldScanning != m_scanning)
        emit scanningChanged();
    emit connectedChanged();
    emit healthChanged();
    setState(snapshot.daemonAvailable ? SystemServiceState::Ready
                                      : SystemServiceState::Unavailable);
    if (!m_powerPending)
        m_powerTimer.stop();
    reconcileDiscovery();
}

void BluetoothService::reconcileDiscovery()
{
    if (m_discoveryState.wantsStop() && !m_discoveryRequestInFlight) {
        const quint64 requestId = ++m_discoveryOperationId;
        if (m_backend->stopDiscovery(requestId)) {
            m_discoveryState.stopRequested();
            m_discoveryRequestId = requestId;
            m_discoveryRequestInFlight = true;
            m_discoveryTimer.start();
        }
        return;
    }
    if (m_discoveryState.wantsStart() && !m_discoveryRequestInFlight
        && !m_discoveryRetryTimer.isActive()) {
        const quint64 requestId = ++m_discoveryOperationId;
        if (m_backend->startDiscovery(requestId)) {
            m_discoveryState.startRequested();
            m_discoveryRequestId = requestId;
            m_discoveryRequestInFlight = true;
            m_discoveryTimer.start();
        } else {
            scheduleDiscoveryRetry();
        }
    }
}

void BluetoothService::handleOperationFinished(const BluetoothOperationResult &result)
{
    if (result.kind == BluetoothOperationKind::Power) {
        if (!result.success)
            finishPowerPending(false, result.error);
        return;
    }
    if (result.kind == BluetoothOperationKind::StartDiscovery
        || result.kind == BluetoothOperationKind::StopDiscovery) {
        if (!m_discoveryRequestInFlight || result.requestId != m_discoveryRequestId)
            return;
        const bool start = result.kind == BluetoothOperationKind::StartDiscovery;
        m_discoveryRequestId = 0;
        m_discoveryState.operationFinished(start, result.success);
        finishDiscoveryRequest(result.success, result.error);
        if (result.success) {
            setErrorString({});
            setState(SystemServiceState::Ready);
            cancelDiscoveryRetry();
            reconcileDiscovery();
        } else if (start) {
            scheduleDiscoveryRetry();
        } else {
            reconcileDiscovery();
        }
        return;
    }
    if (!result.success) {
        if (!result.error.isEmpty())
            setErrorString(result.error);
        setState(SystemServiceState::Degraded);
    }
}

void BluetoothService::finishPowerPending(bool success, const QString &errorString)
{
    m_powerTimer.stop();
    if (!success) {
        setErrorString(errorString.isEmpty() ? QStringLiteral("Bluetooth power update failed")
                                             : errorString);
        setState(SystemServiceState::Degraded);
    }
    if (!m_powerPending)
        return;
    m_powerPending = false;
    emit powerPendingChanged();
}

void BluetoothService::finishDiscoveryRequest(bool success, const QString &errorString)
{
    m_discoveryTimer.stop();
    m_discoveryRequestInFlight = false;
    if (!success) {
        setErrorString(errorString.isEmpty() ? QStringLiteral("Bluetooth discovery update failed")
                                             : errorString);
        setState(SystemServiceState::Degraded);
    }
}

void BluetoothService::scheduleDiscoveryRetry()
{
    if (!m_discoveryState.hasDemand() || !m_adapterAvailable || !m_powered
        || !m_available || m_discoveryRequestInFlight || m_discoveryRetryTimer.isActive())
        return;
    static constexpr int delays[] = {500, 1000, 2000, 5000};
    const int index = std::min(m_discoveryRetryAttempt,
                               static_cast<int>(std::size(delays) - 1));
    m_discoveryRetryTimer.start(delays[index]);
    ++m_discoveryRetryAttempt;
}

void BluetoothService::cancelDiscoveryRetry()
{
    m_discoveryRetryTimer.stop();
    m_discoveryRetryAttempt = 0;
}

} // namespace Astrea::System
