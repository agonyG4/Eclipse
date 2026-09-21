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
