#include "system/bluetooth/RustBluetoothBackend.hpp"

#include <QVariantMap>

namespace Astrea::System {

RustBluetoothBackend::RustBluetoothBackend(QObject *parent)
    : QObject(parent)
{
    QObject::connect(&m_engine, &RustBluetoothEngine::snapshotChanged,
                     this, [this] { publishSnapshot(); });
}

RustBluetoothBackend::~RustBluetoothBackend()
{
    stop();
}

bool RustBluetoothBackend::start(const Callbacks &callbacks, QString *errorOut)
{
    m_callbacks = callbacks;
    if (m_engine.start())
        return true;
    if (errorOut)
        *errorOut = m_engine.errorString();
    return false;
}

void RustBluetoothBackend::stop()
{
    m_engine.stop();
    m_callbacks = {};
}

bool RustBluetoothBackend::setPowered(bool powered)
{
    return m_engine.setPowered(powered);
}

bool RustBluetoothBackend::requestScan(const QString &owner)
{
    return m_engine.requestScan(owner);
}

void RustBluetoothBackend::releaseScan(const QString &owner)
{
    m_engine.releaseScan(owner);
}

bool RustBluetoothBackend::connectDevice(const QString &objectPath)
{
    return m_engine.connectDevice(objectPath);
}

bool RustBluetoothBackend::disconnectDevice(const QString &objectPath)
{
    return m_engine.disconnectDevice(objectPath);
}

bool RustBluetoothBackend::pairDevice(const QString &objectPath)
{
    return m_engine.pairDevice(objectPath);
}

bool RustBluetoothBackend::cancelPairing()
{
    return m_engine.cancelPairing();
}

bool RustBluetoothBackend::setDeviceTrusted(const QString &objectPath, bool trusted)
{
    return m_engine.setDeviceTrusted(objectPath, trusted);
}

bool RustBluetoothBackend::forgetDevice(const QString &objectPath)
{
    return m_engine.forgetDevice(objectPath);
}

bool RustBluetoothBackend::submitAgentText(quint64 requestId, const QString &text)
{
    return m_engine.submitAgentText(requestId, text);
}

bool RustBluetoothBackend::confirmAgentRequest(quint64 requestId, bool accepted)
{
    return m_engine.confirmAgentRequest(requestId, accepted);
}

bool RustBluetoothBackend::rejectAgentRequest(quint64 requestId)
{
    return m_engine.rejectAgentRequest(requestId);
}

void RustBluetoothBackend::publishSnapshot()
{
    if (!m_callbacks.snapshotChanged)
        return;

    BluetoothSnapshot snapshot;
    snapshot.state = static_cast<SystemServiceState>(m_engine.state());
    snapshot.available = m_engine.available();
    snapshot.ready = m_engine.ready();
    snapshot.adapterAvailable = m_engine.adapterAvailable();
    snapshot.adapterPath = m_engine.adapterPath();
    snapshot.adapterName = m_engine.adapterName();
    snapshot.powered = m_engine.powered();
    snapshot.powerPending = m_engine.powerPending();
    snapshot.scanning = m_engine.scanning();
    snapshot.connectedCount = m_engine.connectedCount();
    snapshot.connectedName = m_engine.connectedName();
    snapshot.errorString = m_engine.errorString();
    snapshot.pairing = m_engine.pairing();
    snapshot.pairingDevicePath = m_engine.pairingDevicePath();
    snapshot.pairingDeviceName = m_engine.pairingDeviceName();
    snapshot.pairingError = m_engine.pairingError();
    snapshot.agentRequestActive = m_engine.agentRequestActive();
    snapshot.agentRequestId = m_engine.agentRequestId();
    snapshot.agentRequestKind = static_cast<BluetoothAgentRequestKind>(m_engine.agentRequestKind());
    snapshot.agentDevicePath = m_engine.agentDevicePath();
    snapshot.agentDeviceName = m_engine.agentDeviceName();
    snapshot.agentPasskey = m_engine.agentPasskey();
    snapshot.agentEntered = m_engine.agentEntered();
    snapshot.agentServiceUuid = m_engine.agentServiceUuid();
    snapshot.agentDisplayPin = m_engine.agentDisplayPin();
    snapshot.operationError = m_engine.operationError();

    const QList<QVariant> deviceRows = m_engine.devices();
    snapshot.devices.reserve(deviceRows.size());
    for (const QVariant &row : deviceRows) {
        const QVariantMap properties = row.toMap();
        BluetoothDevice device;
        device.id = properties.value(QStringLiteral("id")).toString();
        device.objectPath = properties.value(QStringLiteral("objectPath")).toString();
        device.address = properties.value(QStringLiteral("address")).toString();
        device.name = properties.value(QStringLiteral("name")).toString();
        device.paired = properties.value(QStringLiteral("paired")).toBool();
        device.trusted = properties.value(QStringLiteral("trusted")).toBool();
        device.connected = properties.value(QStringLiteral("connected")).toBool();
        device.discovered = properties.value(QStringLiteral("discovered")).toBool();
        device.icon = properties.value(QStringLiteral("icon")).toString();
        device.rssi = properties.value(QStringLiteral("rssi")).toInt();
        device.batteryPercent = properties.value(QStringLiteral("batteryPercent")).toInt();
        snapshot.devices.append(std::move(device));
    }
    m_callbacks.snapshotChanged(std::move(snapshot));
}

} // namespace Astrea::System
