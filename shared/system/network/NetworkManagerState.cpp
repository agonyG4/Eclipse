#include "system/network/NetworkManagerState.hpp"

#include <QDBusArgument>
#include <QDBusObjectPath>

#include <algorithm>
#include <tuple>

namespace Astrea::System {

namespace {

constexpr uint ethernetType = 1;
constexpr uint wifiType = 2;
constexpr uint activatedState = 2;

QVector<QString> sorted(const QVector<QString> &paths)
{
    QVector<QString> result = paths;
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace

void NetworkManagerState::reset()
{
    m_managerProperties.clear();
    m_devices.clear();
    m_accessPoints.clear();
    clearPrimary();
}

void NetworkManagerState::applyProperties(QVariantMap &target, const QVariantMap &changed,
                                          const QStringList &invalidated)
{
    for (auto it = changed.cbegin(); it != changed.cend(); ++it)
        target.insert(it.key(), it.value());
    // Preserve the last authoritative value until the targeted GetAll refresh arrives.
    // An invalidated property is never converted into false, zero, or an empty value.
    Q_UNUSED(invalidated)
}

void NetworkManagerState::setManagerProperties(QVariantMap properties)
{
    m_managerProperties = std::move(properties);
}

void NetworkManagerState::updateManagerProperties(const QVariantMap &changed,
                                                  const QStringList &invalidated)
{
    applyProperties(m_managerProperties, changed, invalidated);
}

void NetworkManagerState::upsertDevice(const QString &path, QVariantMap properties)
{
    NetworkDeviceState &device = m_devices[path];
    device.objectPath = path;
    device.deviceProperties = std::move(properties);
    device.wirelessInterfacePresent = device.deviceProperties.value(QStringLiteral("DeviceType"))
                                      .toUInt() == wifiType;
}

void NetworkManagerState::setWirelessProperties(const QString &path, QVariantMap properties)
{
    NetworkDeviceState &device = m_devices[path];
    device.objectPath = path;
    device.wirelessProperties = std::move(properties);
    device.wirelessInterfacePresent = true;
}

void NetworkManagerState::updateDeviceProperties(const QString &path, const QString &interfaceName,
                                                 const QVariantMap &changed,
                                                 const QStringList &invalidated)
{
    NetworkDeviceState &device = m_devices[path];
    device.objectPath = path;
    if (interfaceName == QLatin1String("org.freedesktop.NetworkManager.Device.Wireless")) {
        device.wirelessInterfacePresent = true;
        applyProperties(device.wirelessProperties, changed, invalidated);
    } else {
        applyProperties(device.deviceProperties, changed, invalidated);
        device.wirelessInterfacePresent = device.deviceProperties.value(
            QStringLiteral("DeviceType")).toUInt() == wifiType;
    }
}

void NetworkManagerState::removeDevice(const QString &path)
{
    m_devices.remove(path);
    if (m_primaryPath.isEmpty())
        return;
    const QVector<QString> activeDevices = objectPaths(m_primaryProperties.value(
        QStringLiteral("Devices")));
    if (!activeDevices.contains(path))
        return;
    ++m_primaryEpoch;
    m_primaryProperties.remove(QStringLiteral("Devices"));
}

void NetworkManagerState::upsertAccessPoint(const QString &path, QVariantMap properties)
{
    m_accessPoints[path] = NetworkAccessPointState{path, std::move(properties)};
}

void NetworkManagerState::clearAccessPoints()
{
    m_accessPoints.clear();
}

void NetworkManagerState::updateAccessPoint(const QString &path, const QVariantMap &changed,
                                            const QStringList &invalidated)
{
    NetworkAccessPointState &point = m_accessPoints[path];
    point.objectPath = path;
    applyProperties(point.properties, changed, invalidated);
}

void NetworkManagerState::removeAccessPoint(const QString &path)
{
    m_accessPoints.remove(path);
}

void NetworkManagerState::setPrimaryProperties(const QString &path, QVariantMap properties)
{
    const quint64 epoch = beginPrimaryRequest(path);
    applyPrimaryReply(path, epoch, std::move(properties));
}

quint64 NetworkManagerState::beginPrimaryRequest(const QString &path)
{
    if (m_primaryPath != path)
        ++m_primaryEpoch;
    m_primaryPath = path;
    m_primaryProperties.clear();
    return m_primaryEpoch;
}

bool NetworkManagerState::applyPrimaryReply(const QString &path, quint64 epoch,
                                             QVariantMap properties)
{
    if (path != m_primaryPath || epoch != m_primaryEpoch)
        return false;
    m_primaryProperties = std::move(properties);
    return true;
}

void NetworkManagerState::updatePrimaryProperties(const QVariantMap &changed,
                                                  const QStringList &invalidated)
{
    applyProperties(m_primaryProperties, changed, invalidated);
}

void NetworkManagerState::clearPrimary()
{
    if (!m_primaryPath.isEmpty())
        ++m_primaryEpoch;
    m_primaryPath.clear();
    m_primaryProperties.clear();
}

QString NetworkManagerState::objectPath(const QVariant &value)
{
    if (value.canConvert<QDBusObjectPath>())
        return value.value<QDBusObjectPath>().path();
    return value.toString();
}

QVector<QString> NetworkManagerState::objectPaths(const QVariant &value)
{
    QVector<QString> result;
    if (value.canConvert<QDBusArgument>()) {
        QDBusArgument argument = value.value<QDBusArgument>();
        argument.beginArray();
        while (!argument.atEnd()) {
            QDBusObjectPath path;
            argument >> path;
            result.append(path.path());
        }
        argument.endArray();
    } else {
        for (const QVariant &item : value.toList())
            result.append(objectPath(item));
    }
    return result;
}

QString NetworkManagerState::selectedWifiDevicePath() const
{
    QVector<QString> primaryWifi;
    for (const QString &path : objectPaths(m_primaryProperties.value(QStringLiteral("Devices")))) {
        const auto device = m_devices.constFind(path);
        if (device != m_devices.cend()
            && device->wirelessInterfacePresent
            && device->deviceProperties.value(QStringLiteral("DeviceType")).toUInt() == wifiType)
            primaryWifi.append(path);
    }
    if (!primaryWifi.isEmpty())
        return sorted(primaryWifi).constFirst();

    QVector<QString> wifi;
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
        if (it->wirelessInterfacePresent
            && it->deviceProperties.value(QStringLiteral("DeviceType")).toUInt() == wifiType)
            wifi.append(it.key());
    }
    return wifi.isEmpty() ? QString() : sorted(wifi).constFirst();
}

QString NetworkManagerState::activeAccessPointPath() const
{
    const QString wifiPath = selectedWifiDevicePath();
    if (wifiPath.isEmpty())
        return {};
    const auto device = m_devices.constFind(wifiPath);
    return device == m_devices.cend() ? QString()
                                      : objectPath(device->wirelessProperties.value(
                                            QStringLiteral("ActiveAccessPoint")));
}

QString NetworkManagerState::physicalDeviceForPrimary() const
{
    QVector<QString> physical;
    for (const QString &path : objectPaths(m_primaryProperties.value(QStringLiteral("Devices")))) {
        const auto device = m_devices.constFind(path);
        if (device == m_devices.cend())
            continue;
        const uint type = device->deviceProperties.value(QStringLiteral("DeviceType")).toUInt();
        if (type == ethernetType || type == wifiType)
            physical.append(path);
    }
    return physical.isEmpty() ? QString() : sorted(physical).constFirst();
}

NetworkSnapshot NetworkManagerState::snapshot() const
{
    NetworkSnapshot result;
    result.daemonAvailable = true;
    result.wifiAvailable = !selectedWifiDevicePath().isEmpty();
    result.wifiEnabled = m_managerProperties.value(QStringLiteral("WirelessEnabled")).toBool();
    result.wifiScanning = false;
    result.connectionType = NetworkConnectionType::None;

    const QString physicalPath = physicalDeviceForPrimary();
    const auto primaryState = m_primaryProperties;
    if (!m_primaryPath.isEmpty()) {
        const auto device = m_devices.constFind(physicalPath);
        const uint type = device == m_devices.cend()
            ? 0 : device->deviceProperties.value(QStringLiteral("DeviceType")).toUInt();
        if (type == wifiType)
            result.connectionType = NetworkConnectionType::Wifi;
        else if (type == ethernetType)
            result.connectionType = NetworkConnectionType::Wired;
        else
            result.connectionType = NetworkConnectionType::Other;
        result.connected = primaryState.value(QStringLiteral("State")).toUInt() == activatedState
            && !physicalPath.isEmpty();
        result.connectionName = primaryState.value(QStringLiteral("Id")).toString();
        if (result.connected && device != m_devices.cend())
            result.interfaceName = device->deviceProperties.value(QStringLiteral("Interface"))
                                        .toString();
    }

    const QString activePath = activeAccessPointPath();
    for (auto it = m_accessPoints.cbegin(); it != m_accessPoints.cend(); ++it) {
        const QVariantMap &properties = it->properties;
        WifiNetwork network;
        network.ssid = QString::fromUtf8(properties.value(QStringLiteral("Ssid")).toByteArray());
        network.strength = properties.value(QStringLiteral("Strength")).toInt();
        network.frequencyMHz = properties.value(QStringLiteral("Frequency")).toInt();
        network.bssid = properties.value(QStringLiteral("HwAddress")).toString();
        network.secured = properties.value(QStringLiteral("Flags")).toUInt() != 0
            || properties.value(QStringLiteral("WpaFlags")).toUInt() != 0
            || properties.value(QStringLiteral("RsnFlags")).toUInt() != 0;
        network.active = it.key() == activePath;
        result.wifiNetworks.append(std::move(network));
    }
    return result;
}

bool NetworkScanState::request(quint64 generation, const QString &devicePath, qint64 lastScan,
                               qint64 nowMs)
{
    if (m_phase != NetworkScanPhase::Idle) {
        m_queuedDemand = true;
        return true;
    }
    m_phase = NetworkScanPhase::RequestPending;
    m_queuedDemand = false;
    m_generation = generation;
    m_devicePath = devicePath;
    m_baselineLastScan = lastScan;
    m_cooldownUntilMs = -1;
    Q_UNUSED(nowMs)
    return true;
}

bool NetworkScanState::requestFinished(bool success, qint64 nowMs)
{
    if (m_phase != NetworkScanPhase::RequestPending)
        return false;
    if (!success) {
        if (m_queuedDemand) {
            m_phase = NetworkScanPhase::Cooldown;
            m_cooldownUntilMs = nowMs + 3000;
        } else {
            m_phase = NetworkScanPhase::Idle;
            m_devicePath.clear();
        }
        return false;
    }
    m_phase = NetworkScanPhase::WaitingForLastScan;
    Q_UNUSED(nowMs)
    return true;
}

bool NetworkScanState::lastScanAdvanced(quint64 generation, const QString &devicePath,
                                        qint64 lastScan, qint64 nowMs)
{
    if (m_phase != NetworkScanPhase::WaitingForLastScan || generation != m_generation
        || devicePath != m_devicePath || lastScan <= m_baselineLastScan)
        return false;
    m_phase = NetworkScanPhase::Cooldown;
    m_cooldownUntilMs = nowMs + 3000;
    return true;
}

bool NetworkScanState::cooldownExpired(qint64 nowMs)
{
    if (m_phase != NetworkScanPhase::Cooldown || nowMs < m_cooldownUntilMs)
        return false;
    if (m_queuedDemand) {
        m_phase = NetworkScanPhase::Idle;
        m_queuedDemand = false;
        m_devicePath.clear();
        m_baselineLastScan = -1;
        m_cooldownUntilMs = -1;
        return true;
    }
    m_phase = NetworkScanPhase::Idle;
    m_devicePath.clear();
    m_cooldownUntilMs = -1;
    return false;
}

void NetworkScanState::timeout(qint64 nowMs)
{
    if (m_phase != NetworkScanPhase::RequestPending
        && m_phase != NetworkScanPhase::WaitingForLastScan)
        return;
    if (m_queuedDemand) {
        m_phase = NetworkScanPhase::Cooldown;
        m_cooldownUntilMs = nowMs + 3000;
        return;
    }
    m_phase = NetworkScanPhase::Idle;
    m_devicePath.clear();
    m_baselineLastScan = -1;
}

void NetworkScanState::invalidate()
{
    m_phase = NetworkScanPhase::Idle;
    m_queuedDemand = false;
    m_generation = 0;
    m_devicePath.clear();
    m_baselineLastScan = -1;
    m_cooldownUntilMs = -1;
}

} // namespace Astrea::System
