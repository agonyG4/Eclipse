#include "system/network/NetworkManagerState.hpp"

#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QHostAddress>
#include <QSet>

#include <algorithm>
#include <tuple>

namespace Astrea::System {

namespace {

constexpr uint ethernetType = 1;
constexpr uint wifiType = 2;
constexpr uint activatedState = 2;
constexpr uint accessPointPrivacyFlag = 0x00000001;
constexpr uint interfaceLowerUpFlag = 0x00000002;
constexpr uint interfaceCarrierFlag = 0x00010000;

QVariant unwrapDbusVariant(QVariant value)
{
    while (value.metaType() == QMetaType::fromType<QDBusVariant>())
        value = value.value<QDBusVariant>().variant();
    return value;
}

QVector<QVariantMap> decodeDictionaryArray(const QVariant &value)
{
    QVector<QVariantMap> result;
    const QVariant unwrapped = unwrapDbusVariant(value);
    if (unwrapped.metaType() == QMetaType::fromType<QVariantList>()) {
        for (const QVariant &entry : unwrapped.toList()) {
            const QVariant item = unwrapDbusVariant(entry);
            if (item.metaType() == QMetaType::fromType<QVariantMap>())
                result.append(item.toMap());
        }
        return result;
    }
    if (unwrapped.metaType() != QMetaType::fromType<QDBusArgument>())
        return result;

    QDBusArgument argument = unwrapped.value<QDBusArgument>();
    if (argument.currentType() != QDBusArgument::ArrayType)
        return result;
    argument.beginArray();
    while (!argument.atEnd()) {
        if (argument.currentType() != QDBusArgument::MapType) {
            QVariant ignored;
            argument >> ignored;
            continue;
        }
        QVariantMap entry;
        argument >> entry;
        result.append(std::move(entry));
    }
    argument.endArray();
    return result;
}

QStringList projectAddresses(const QVariant &value, QAbstractSocket::NetworkLayerProtocol family)
{
    QStringList result;
    QSet<QString> seen;
    const int maxPrefix = family == QAbstractSocket::IPv4Protocol ? 32 : 128;
    for (const QVariantMap &entry : decodeDictionaryArray(value)) {
        const QString rawAddress = unwrapDbusVariant(entry.value(QStringLiteral("address")))
                                       .toString();
        bool prefixOk = false;
        const uint prefix = unwrapDbusVariant(entry.value(QStringLiteral("prefix")))
                                .toUInt(&prefixOk);
        QHostAddress address;
        if (!prefixOk || prefix > static_cast<uint>(maxPrefix)
            || !address.setAddress(rawAddress) || address.protocol() != family)
            continue;
        const QString projected = QStringLiteral("%1/%2").arg(address.toString()).arg(prefix);
        if (seen.contains(projected))
            continue;
        seen.insert(projected);
        result.append(projected);
    }
    return result;
}

QString projectGateway(const QVariant &value, QAbstractSocket::NetworkLayerProtocol family)
{
    const QString rawAddress = unwrapDbusVariant(value).toString();
    if (rawAddress.isEmpty())
        return {};
    QHostAddress address;
    return address.setAddress(rawAddress) && address.protocol() == family
        ? address.toString() : QString();
}

QStringList projectDnsServers(const QVariant &value,
                              QAbstractSocket::NetworkLayerProtocol family)
{
    QStringList result;
    QSet<QString> seen;
    for (const QVariantMap &entry : decodeDictionaryArray(value)) {
        const QString raw = unwrapDbusVariant(entry.value(QStringLiteral("address"))).toString();
        QHostAddress address;
        if (raw.isEmpty() || !address.setAddress(raw) || address.protocol() != family)
            continue;
        const QString canonical = address.toString();
        if (seen.contains(canonical))
            continue;
        seen.insert(canonical);
        result.append(canonical);
    }
    return result;
}

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
    device.wiredInterfacePresent = false;
    device.wiredProperties.clear();
}

void NetworkManagerState::setWirelessProperties(const QString &path, QVariantMap properties)
{
    NetworkDeviceState &device = m_devices[path];
    device.objectPath = path;
    device.wirelessProperties = std::move(properties);
    device.wirelessInterfacePresent = true;
}

bool NetworkManagerState::applyWiredPropertiesReply(const QString &activeConnectionPath,
                                                    quint64 primaryEpoch,
                                                    const QString &devicePath,
                                                    QVariantMap properties)
{
    if (activeConnectionPath != m_primaryPath || primaryEpoch != m_primaryEpoch
        || m_primaryProperties.value(QStringLiteral("State")).toUInt() != activatedState
        || physicalDeviceForPrimary() != devicePath)
        return false;
    auto device = m_devices.find(devicePath);
    if (device == m_devices.end()
        || device->deviceProperties.value(QStringLiteral("DeviceType")).toUInt() != ethernetType)
        return false;
    device->wiredProperties = std::move(properties);
    device->wiredInterfacePresent = true;
    return true;
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
    } else if (interfaceName == QLatin1String("org.freedesktop.NetworkManager.Device.Wired")) {
        if (path != physicalDeviceForPrimary()
            || device.deviceProperties.value(QStringLiteral("DeviceType")).toUInt() != ethernetType)
            return;
        device.wiredInterfacePresent = true;
        for (const QString &key : invalidated)
            device.wiredProperties.remove(key);
        for (auto it = changed.cbegin(); it != changed.cend(); ++it)
            device.wiredProperties.insert(it.key(), it.value());
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
    ++m_primaryEpoch;
    m_primaryPath = path;
    m_primaryProperties.clear();
    clearPrimaryRuntimeDetails();
    clearPrimaryProfile();
    return m_primaryEpoch;
}

bool NetworkManagerState::applyPrimaryReply(const QString &path, quint64 epoch,
                                             QVariantMap properties)
{
    if (path != m_primaryPath || epoch != m_primaryEpoch)
        return false;
    m_primaryProperties = std::move(properties);
    synchronizePrimaryIpConfigPaths();
    const QString profilePath = primaryProfilePath();
    if (m_primaryProfile.objectPath != profilePath) {
        clearPrimaryProfile();
        m_primaryProfile.objectPath = profilePath;
    }
    if (m_primaryProperties.value(QStringLiteral("State")).toUInt() != activatedState)
        clearPrimaryRuntimeDetails();
    return true;
}

void NetworkManagerState::updatePrimaryProperties(const QVariantMap &changed,
                                                  const QStringList &invalidated)
{
    const uint previousState = m_primaryProperties.value(QStringLiteral("State")).toUInt();
    const QString previousProfilePath = primaryProfilePath();
    applyProperties(m_primaryProperties, changed, invalidated);
    for (const QString &key : invalidated) {
        if (key == QLatin1String("Ip4Config") || key == QLatin1String("Ip6Config")
            || key == QLatin1String("Connection"))
            m_primaryProperties.remove(key);
    }
    synchronizePrimaryIpConfigPaths();
    const QString currentProfilePath = primaryProfilePath();
    if (previousProfilePath != currentProfilePath) {
        clearPrimaryProfile();
        m_primaryProfile.objectPath = currentProfilePath;
    }
    const uint currentState = m_primaryProperties.value(QStringLiteral("State")).toUInt();
    if (previousState == activatedState && currentState != activatedState) {
        ++m_primaryEpoch;
        clearPrimaryRuntimeDetails();
    }
}

bool NetworkManagerState::applyPrimaryIpConfigReply(const QString &activeConnectionPath,
                                                    quint64 primaryEpoch,
                                                    const QString &configPath, bool ipv4,
                                                    QVariantMap properties)
{
    PrimaryIpConfigState &config = ipv4 ? m_primaryIpv4Config : m_primaryIpv6Config;
    if (activeConnectionPath != m_primaryPath || primaryEpoch != m_primaryEpoch
        || m_primaryProperties.value(QStringLiteral("State")).toUInt() != activatedState
        || configPath.isEmpty() || configPath == QLatin1String("/")
        || config.objectPath != configPath)
        return false;
    config.properties = std::move(properties);
    return true;
}

bool NetworkManagerState::updatePrimaryIpConfigProperties(const QString &configPath,
                                                          const QString &interfaceName,
                                                          const QVariantMap &changed,
                                                          const QStringList &invalidated)
{
    PrimaryIpConfigState *config = nullptr;
    if (interfaceName == QLatin1String("org.freedesktop.NetworkManager.IP4Config"))
        config = &m_primaryIpv4Config;
    else if (interfaceName == QLatin1String("org.freedesktop.NetworkManager.IP6Config"))
        config = &m_primaryIpv6Config;
    if (!config || configPath != config->objectPath || configPath.isEmpty()
        || m_primaryProperties.value(QStringLiteral("State")).toUInt() != activatedState)
        return false;
    for (const QString &key : invalidated) {
        if (key == QLatin1String("AddressData") || key == QLatin1String("Gateway")
            || key == QLatin1String("NameserverData") || key == QLatin1String("DnsPriority"))
            config->properties.remove(key);
    }
    for (auto it = changed.cbegin(); it != changed.cend(); ++it)
        config->properties.insert(it.key(), it.value());
    return true;
}

bool NetworkManagerState::applyPrimaryProfileReply(
    const QString &activeConnectionPath, quint64 primaryEpoch, const QString &profilePath,
    NetworkConnectionSettings settings, quint64 versionId)
{
    if (activeConnectionPath != m_primaryPath || primaryEpoch != m_primaryEpoch
        || profilePath.isEmpty() || profilePath == QLatin1String("/")
        || profilePath != primaryProfilePath() || profilePath != m_primaryProfile.objectPath)
        return false;
    m_primaryProfile.settings = std::move(settings);
    m_primaryProfile.versionId = versionId;
    m_primaryProfile.available = true;
    return true;
}

void NetworkManagerState::invalidatePrimaryProfile()
{
    const QString path = m_primaryProfile.objectPath;
    m_primaryProfile = {};
    m_primaryProfile.objectPath = path;
}

void NetworkManagerState::clearPrimaryProfile()
{
    m_primaryProfile = {};
}

void NetworkManagerState::clearPrimary()
{
    if (!m_primaryPath.isEmpty())
        ++m_primaryEpoch;
    m_primaryPath.clear();
    m_primaryProperties.clear();
    clearPrimaryRuntimeDetails();
    clearPrimaryProfile();
}

void NetworkManagerState::synchronizePrimaryIpConfigPaths()
{
    const auto update = [this](PrimaryIpConfigState &config, const QString &key) {
        QString path = objectPath(m_primaryProperties.value(key));
        if (path == QLatin1String("/"))
            path.clear();
        if (config.objectPath == path)
            return;
        config.objectPath = std::move(path);
        config.properties.clear();
    };
    update(m_primaryIpv4Config, QStringLiteral("Ip4Config"));
    update(m_primaryIpv6Config, QStringLiteral("Ip6Config"));
}

void NetworkManagerState::clearPrimaryRuntimeDetails()
{
    m_primaryIpv4Config = {};
    m_primaryIpv6Config = {};
    for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
        it->wiredProperties.clear();
        it->wiredInterfacePresent = false;
    }
}

QString NetworkManagerState::primaryProfilePath() const
{
    QString path = objectPath(m_primaryProperties.value(QStringLiteral("Connection")));
    if (path == QLatin1String("/"))
        path.clear();
    return path;
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
    QVector<QString> primaryDevices;
    for (const QString &path : objectPaths(m_primaryProperties.value(QStringLiteral("Devices")))) {
        const auto device = m_devices.constFind(path);
        if (device == m_devices.cend())
            continue;
        primaryDevices.append(path);
        const uint type = device->deviceProperties.value(QStringLiteral("DeviceType")).toUInt();
        if (type == ethernetType || type == wifiType)
            physical.append(path);
    }
    if (!physical.isEmpty())
        return sorted(physical).constFirst();
    return primaryDevices.isEmpty() ? QString() : sorted(primaryDevices).constFirst();
}

NetworkSnapshot NetworkManagerState::snapshot() const
{
    NetworkSnapshot result;
    result.daemonAvailable = true;
    result.wifiAvailable = !selectedWifiDevicePath().isEmpty();
    result.wifiHardwareEnabled = m_managerProperties.value(
        QStringLiteral("WirelessHardwareEnabled")).toBool();
    result.wifiEnabled = m_managerProperties.value(QStringLiteral("WirelessEnabled")).toBool();
    result.wifiScanning = false;
    result.connectionType = NetworkConnectionType::None;

    const QString physicalPath = physicalDeviceForPrimary();
    const auto primaryState = m_primaryProperties;
    result.activeConnectionPath = m_primaryPath;
    result.profilePath = primaryProfilePath();
    result.devicePath = physicalPath;
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
        if (result.connected && device != m_devices.cend()) {
            result.interfaceName = device->deviceProperties.value(QStringLiteral("Interface"))
                                        .toString();
            result.hardwareAddress = device->deviceProperties.value(
                QStringLiteral("HwAddress")).toString();
            result.mtu = device->deviceProperties.value(QStringLiteral("Mtu")).toUInt();
            if (type == ethernetType) {
                const uint flags = device->deviceProperties.value(
                    QStringLiteral("InterfaceFlags")).toUInt();
                const bool carrier = (flags & interfaceCarrierFlag) != 0;
                const bool lowerLayerUp = (flags & interfaceLowerUpFlag) != 0;
                result.linkCarrier = carrier || lowerLayerUp;
                if (device->wiredInterfacePresent)
                    result.linkSpeedMbps = device->wiredProperties.value(
                        QStringLiteral("Speed")).toUInt();
            }
            if (m_primaryIpv4Config.objectPath
                == objectPath(primaryState.value(QStringLiteral("Ip4Config")))) {
                result.ipv4Addresses = projectAddresses(
                    m_primaryIpv4Config.properties.value(QStringLiteral("AddressData")),
                    QAbstractSocket::IPv4Protocol);
                result.ipv4Gateway = projectGateway(
                    m_primaryIpv4Config.properties.value(QStringLiteral("Gateway")),
                    QAbstractSocket::IPv4Protocol);
                result.activeIpv4DnsServers = projectDnsServers(
                    m_primaryIpv4Config.properties.value(QStringLiteral("NameserverData")),
                    QAbstractSocket::IPv4Protocol);
            }
            if (m_primaryIpv6Config.objectPath
                == objectPath(primaryState.value(QStringLiteral("Ip6Config")))) {
                result.ipv6Addresses = projectAddresses(
                    m_primaryIpv6Config.properties.value(QStringLiteral("AddressData")),
                    QAbstractSocket::IPv6Protocol);
                result.ipv6Gateway = projectGateway(
                    m_primaryIpv6Config.properties.value(QStringLiteral("Gateway")),
                    QAbstractSocket::IPv6Protocol);
                result.activeIpv6DnsServers = projectDnsServers(
                    m_primaryIpv6Config.properties.value(QStringLiteral("NameserverData")),
                    QAbstractSocket::IPv6Protocol);
            }
        }
    }

    if (m_primaryProfile.available
        && m_primaryProfile.objectPath == result.profilePath) {
        const NetworkDnsProjection dns = projectDns(m_primaryProfile.settings);
        result.dnsMode = dns.mode;
        result.configuredIpv4DnsServers = dns.configuredIpv4DnsServers;
        result.configuredIpv6DnsServers = dns.configuredIpv6DnsServers;
        result.dnsEditable = result.connected
            && (result.connectionType == NetworkConnectionType::Wired
                || result.connectionType == NetworkConnectionType::Wifi)
            && dns.dnsEditable && m_primaryProfile.versionId != 0;
    }

    const QString activePath = activeAccessPointPath();
    for (auto it = m_accessPoints.cbegin(); it != m_accessPoints.cend(); ++it) {
        const QVariantMap &properties = it->properties;
        WifiNetwork network;
        network.ssid = QString::fromUtf8(properties.value(QStringLiteral("Ssid")).toByteArray());
        network.strength = properties.value(QStringLiteral("Strength")).toInt();
        network.frequencyMHz = properties.value(QStringLiteral("Frequency")).toInt();
        network.bssid = properties.value(QStringLiteral("HwAddress")).toString();
        network.accessPointPath = it->objectPath;
        network.secured = (properties.value(QStringLiteral("Flags")).toUInt()
                           & accessPointPrivacyFlag) != 0
            || properties.value(QStringLiteral("WpaFlags")).toUInt() != 0
            || properties.value(QStringLiteral("RsnFlags")).toUInt() != 0;
        network.active = it.key() == activePath;
        result.wifiNetworks.append(std::move(network));
    }
    return result;
}

void NetworkWirelessRefreshState::begin(quint64 daemonGeneration,
                                         quint64 refreshGeneration,
                                         const QStringList &devicePaths)
{
    m_daemonGeneration = daemonGeneration;
    m_refreshGeneration = refreshGeneration;
    m_pendingDevicePaths.clear();
    for (const QString &devicePath : devicePaths)
        m_pendingDevicePaths.insert(devicePath);
}

bool NetworkWirelessRefreshState::acceptReply(quint64 daemonGeneration,
                                               quint64 refreshGeneration,
                                               const QString &devicePath)
{
    if (daemonGeneration != m_daemonGeneration
        || refreshGeneration != m_refreshGeneration)
        return false;
    const auto it = m_pendingDevicePaths.find(devicePath);
    if (it == m_pendingDevicePaths.end())
        return false;
    m_pendingDevicePaths.erase(it);
    return true;
}

bool NetworkScanState::request(quint64 generation, const QString &devicePath, qint64 lastScan,
                               qint64 nowMs, quint64 requestId)
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
    m_requestId = requestId;
    Q_UNUSED(nowMs)
    return true;
}

bool NetworkScanState::requestFinished(quint64 generation, const QString &devicePath,
                                       quint64 requestId, bool success, qint64 nowMs)
{
    if (m_phase != NetworkScanPhase::RequestPending || generation != m_generation
        || devicePath != m_devicePath || requestId != m_requestId)
        return false;
    if (!success) {
        if (m_queuedDemand) {
            m_phase = NetworkScanPhase::Cooldown;
            m_cooldownUntilMs = nowMs + 3000;
            m_requestId = 0;
        } else {
            m_phase = NetworkScanPhase::Idle;
            m_devicePath.clear();
            m_requestId = 0;
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
        m_requestId = 0;
        return true;
    }
    m_phase = NetworkScanPhase::Idle;
    m_devicePath.clear();
    m_requestId = 0;
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
        m_requestId = 0;
        return;
    }
    m_phase = NetworkScanPhase::Idle;
    m_devicePath.clear();
    m_baselineLastScan = -1;
    m_requestId = 0;
}

void NetworkScanState::invalidate()
{
    m_phase = NetworkScanPhase::Idle;
    m_queuedDemand = false;
    m_generation = 0;
    m_devicePath.clear();
    m_baselineLastScan = -1;
    m_cooldownUntilMs = -1;
    m_requestId = 0;
}

} // namespace Astrea::System
