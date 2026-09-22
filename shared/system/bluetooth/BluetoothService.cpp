#include "system/bluetooth/BluetoothService.hpp"

#include "system/bluetooth/BluetoothDeviceModel.hpp"
#include "system/bluetooth/RustBluetoothBackend.hpp"

namespace Astrea::System {

BluetoothService::BluetoothService(std::unique_ptr<BluetoothBackend> backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend ? std::move(backend) : std::make_unique<RustBluetoothBackend>())
    , m_devicesModel(new BluetoothDeviceModel(this))
{}

BluetoothService::~BluetoothService()
{
    stop();
}

bool BluetoothService::start()
{
    if (m_state != SystemServiceState::Stopped)
        return true;

    setErrorString({});
    setState(SystemServiceState::Starting);
    BluetoothBackend::Callbacks callbacks;
    callbacks.snapshotChanged = [this](BluetoothSnapshot snapshot) {
        applySnapshot(snapshot);
    };
    QString error;
    if (!m_backend->start(callbacks, &error)) {
        setErrorString(error.isEmpty() ? QStringLiteral("Bluetooth backend unavailable") : error);
        setState(SystemServiceState::Unavailable);
        return true;
    }
    return true;
}

void BluetoothService::stop()
{
    if (m_state == SystemServiceState::Stopped)
        return;
    m_backend->stop();
    applySnapshot(BluetoothSnapshot{});
}

bool BluetoothService::setPowered(bool powered)
{
    return m_backend->setPowered(powered);
}

bool BluetoothService::requestScan(const QString &owner)
{
    return m_backend->requestScan(owner);
}

void BluetoothService::releaseScan(const QString &owner)
{
    m_backend->releaseScan(owner);
}

bool BluetoothService::connectDevice(const QString &objectPath)
{
    if (objectPath.isEmpty())
        return false;
    return m_backend->connectDevice(objectPath);
}

bool BluetoothService::disconnectDevice(const QString &objectPath)
{
    if (objectPath.isEmpty())
        return false;
    return m_backend->disconnectDevice(objectPath);
}

bool BluetoothService::pairDevice(const QString &objectPath)
{
    if (objectPath.isEmpty())
        return false;
    return m_backend->pairDevice(objectPath);
}

bool BluetoothService::cancelPairing()
{
    return m_backend->cancelPairing();
}

bool BluetoothService::setDeviceTrusted(const QString &objectPath, bool trusted)
{
    if (objectPath.isEmpty())
        return false;
    return m_backend->setDeviceTrusted(objectPath, trusted);
}

bool BluetoothService::forgetDevice(const QString &objectPath)
{
    if (objectPath.isEmpty())
        return false;
    return m_backend->forgetDevice(objectPath);
}

bool BluetoothService::submitAgentText(quint64 requestId, const QString &text)
{
    return m_backend->submitAgentText(requestId, text);
}

bool BluetoothService::confirmAgentRequest(quint64 requestId, bool accepted)
{
    return m_backend->confirmAgentRequest(requestId, accepted);
}

bool BluetoothService::rejectAgentRequest(quint64 requestId)
{
    return m_backend->rejectAgentRequest(requestId);
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

void BluetoothService::applySnapshot(const BluetoothSnapshot &snapshot)
{
    const bool adapterPropertiesChanged = m_adapterAvailable != snapshot.adapterAvailable
        || m_adapterPath != snapshot.adapterPath || m_adapterName != snapshot.adapterName;
    const bool poweredValueChanged = m_powered != snapshot.powered;
    const bool powerPendingValueChanged = m_powerPending != snapshot.powerPending;
    const bool scanningValueChanged = m_scanning != snapshot.scanning;
    const bool connectedValuesChanged = m_connectedCount != snapshot.connectedCount
        || m_connectedName != snapshot.connectedName;
    const bool pairingValuesChanged = m_pairing != snapshot.pairing
        || m_pairingDevicePath != snapshot.pairingDevicePath
        || m_pairingDeviceName != snapshot.pairingDeviceName
        || m_pairingError != snapshot.pairingError;
    const bool agentValuesChanged = m_agentRequestActive != snapshot.agentRequestActive
        || m_agentRequestId != snapshot.agentRequestId
        || m_agentRequestKind != snapshot.agentRequestKind
        || m_agentDevicePath != snapshot.agentDevicePath
        || m_agentDeviceName != snapshot.agentDeviceName
        || m_agentPasskey != snapshot.agentPasskey || m_agentEntered != snapshot.agentEntered
        || m_agentServiceUuid != snapshot.agentServiceUuid
        || m_agentDisplayPin != snapshot.agentDisplayPin;
    const bool operationValuesChanged = m_operationError != snapshot.operationError;
    const bool healthValuesChanged = m_state != snapshot.state || m_available != snapshot.available
        || m_ready != snapshot.ready || m_errorString != snapshot.errorString
        || m_adapterAvailable != snapshot.adapterAvailable || m_powered != snapshot.powered
        || m_scanning != snapshot.scanning || m_connectedCount != snapshot.connectedCount;

    m_available = snapshot.available;
    m_ready = snapshot.ready;
    m_adapterAvailable = snapshot.adapterAvailable;
    m_adapterPath = snapshot.adapterPath;
    m_adapterName = snapshot.adapterName;
    m_powered = snapshot.powered;
    m_powerPending = snapshot.powerPending;
    m_scanning = snapshot.scanning;
    m_connectedCount = snapshot.connectedCount;
    m_connectedName = snapshot.connectedName;
    m_pairing = snapshot.pairing;
    m_pairingDevicePath = snapshot.pairingDevicePath;
    m_pairingDeviceName = snapshot.pairingDeviceName;
    m_pairingError = snapshot.pairingError;
    m_agentRequestActive = snapshot.agentRequestActive;
    m_agentRequestId = snapshot.agentRequestId;
    m_agentRequestKind = snapshot.agentRequestKind;
    m_agentDevicePath = snapshot.agentDevicePath;
    m_agentDeviceName = snapshot.agentDeviceName;
    m_agentPasskey = snapshot.agentPasskey;
    m_agentEntered = snapshot.agentEntered;
    m_agentServiceUuid = snapshot.agentServiceUuid;
    m_agentDisplayPin = snapshot.agentDisplayPin;
    m_operationError = snapshot.operationError;
    m_devicesModel->replace(snapshot.devices);

    setErrorString(snapshot.errorString, false);
    setState(snapshot.state, false);
    if (adapterPropertiesChanged)
        emit adapterChanged();
    if (poweredValueChanged)
        emit poweredChanged();
    if (powerPendingValueChanged)
        emit powerPendingChanged();
    if (scanningValueChanged)
        emit scanningChanged();
    if (connectedValuesChanged)
        emit connectedChanged();
    if (pairingValuesChanged)
        emit pairingChanged();
    if (agentValuesChanged)
        emit agentRequestChanged();
    if (operationValuesChanged)
        emit operationChanged();
    if (healthValuesChanged)
        emit healthChanged();
}

void BluetoothService::setState(SystemServiceState state, bool notifyHealth)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
    if (notifyHealth)
        emit healthChanged();
}

void BluetoothService::setErrorString(const QString &errorString, bool notifyHealth)
{
    if (m_errorString == errorString)
        return;
    m_errorString = errorString;
    emit errorStringChanged();
    if (notifyHealth)
        emit healthChanged();
}

} // namespace Astrea::System
