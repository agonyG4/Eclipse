#include "system/network/NetworkManagerBackend.hpp"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QFile>
#include <QTimer>

#include <algorithm>
#include <tuple>

namespace Astrea::System {

class NetworkPropertyWatcher final : public QObject {
    Q_OBJECT

public:
    NetworkPropertyWatcher(QString objectPath, QObject *parent)
        : QObject(parent)
        , path(std::move(objectPath))
    {
    }

    QString path;

signals:
    void propertiesUpdated(const QString &path, const QString &interfaceName,
                           const QVariantMap &changed, const QStringList &invalidated);
    void accessPointAddedSignal(const QString &path);
    void accessPointRemovedSignal(const QString &path);

public slots:
    void propertiesChanged(const QString &interfaceName, const QVariantMap &changed,
                            const QStringList &invalidated)
    {
        emit propertiesUpdated(path, interfaceName, changed, invalidated);
    }

    void activeStateChanged(uint state, uint)
    {
        emit propertiesUpdated(path,
                               QStringLiteral("org.freedesktop.NetworkManager.Connection.Active"),
                               {{QStringLiteral("State"), state}}, {});
    }

    void accessPointAdded(const QDBusObjectPath &point)
    {
        emit accessPointAddedSignal(point.path());
    }

    void accessPointRemoved(const QDBusObjectPath &point)
    {
        emit accessPointRemovedSignal(point.path());
    }
};

namespace {

constexpr auto serviceName = "org.freedesktop.NetworkManager";
constexpr auto managerPath = "/org/freedesktop/NetworkManager";
constexpr auto managerInterface = "org.freedesktop.NetworkManager";
constexpr auto propertiesInterface = "org.freedesktop.DBus.Properties";
constexpr auto deviceInterface = "org.freedesktop.NetworkManager.Device";
constexpr uint ethernetDeviceType = 1;
constexpr auto wiredInterface = "org.freedesktop.NetworkManager.Device.Wired";
constexpr auto activeConnectionInterface = "org.freedesktop.NetworkManager.Connection.Active";
constexpr auto ip4ConfigInterface = "org.freedesktop.NetworkManager.IP4Config";
constexpr auto ip6ConfigInterface = "org.freedesktop.NetworkManager.IP6Config";
constexpr auto wirelessInterface = "org.freedesktop.NetworkManager.Device.Wireless";
constexpr auto accessPointInterface = "org.freedesktop.NetworkManager.AccessPoint";

QString objectPathString(const QVariant &value)
{
    return value.value<QDBusObjectPath>().path();
}

QVariantMap propertyMapFromDbus(const QVariant &value)
{
    if (value.metaType() != QMetaType::fromType<QDBusArgument>())
        return value.toMap();
    QDBusArgument argument = value.value<QDBusArgument>();
    if (argument.currentType() != QDBusArgument::MapType)
        return {};
    QVariantMap properties;
    argument >> properties;
    return properties;
}

void reportAsyncError(QDBusPendingCallWatcher *watcher,
                      const NetworkBackend::Callbacks &callbacks)
{
    const QDBusMessage reply = watcher->reply();
    if (reply.type() == QDBusMessage::ErrorMessage && callbacks.errorChanged)
        callbacks.errorChanged(reply.errorMessage());
}

} // namespace

NetworkManagerBackend::NetworkManagerBackend(QObject *parent)
    : QObject(parent)
{
}

NetworkManagerBackend::~NetworkManagerBackend()
{
    stop();
}

bool NetworkManagerBackend::start(const Callbacks &callbacks, QString *errorOut)
{
    if (m_running)
        return true;
    m_callbacks = callbacks;
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        if (errorOut)
            *errorOut = QStringLiteral("System D-Bus is unavailable");
        publishUnavailable();
        return false;
    }

    m_running = true;
    ++m_generation;
    m_state.reset();
    m_scanState.invalidate();
    m_scanClock.start();
    m_trafficTimer = new QTimer(this);
    m_trafficTimer->setInterval(1000);
    connect(m_trafficTimer, &QTimer::timeout, this, &NetworkManagerBackend::sampleTraffic);
    m_trafficClock.start();
    m_trafficTimer->start();
    m_scanCooldownTimer = new QTimer(this);
    m_scanCooldownTimer->setSingleShot(true);
    connect(m_scanCooldownTimer, &QTimer::timeout, this, [this] {
        if (!m_running)
            return;
        if (m_scanState.phase() == NetworkScanPhase::Cooldown) {
            if (m_scanState.cooldownExpired(m_scanClock.elapsed()))
                requestWifiScan();
        } else if (m_scanState.active()) {
            m_scanState.timeout(m_scanClock.elapsed());
            m_snapshot.wifiScanning = false;
            publishReconciledSnapshot();
            if (m_scanState.phase() == NetworkScanPhase::Cooldown && m_scanCooldownTimer)
                m_scanCooldownTimer->start(3000);
        }
    });

    m_serviceWatcher = new QDBusServiceWatcher(QString::fromLatin1(serviceName), bus,
                                               QDBusServiceWatcher::WatchForRegistration
                                                   | QDBusServiceWatcher::WatchForUnregistration,
                                               this);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceRegistered,
            this, [this](const QString &) {
                if (!m_running)
                    return;
                ++m_generation;
                m_state.reset();
                m_scanState.invalidate();
                m_managerProperties.clear();
                m_deviceProperties.clear();
                m_wifiDevicePath.clear();
                rebuildPropertyWatchers();
                probe();
            });
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, [this](const QString &) {
                if (m_running)
                    publishUnavailable();
            });

    bus.connect(QString::fromLatin1(serviceName), QString::fromLatin1(managerPath),
                QString::fromLatin1(propertiesInterface), QStringLiteral("PropertiesChanged"),
                this, SLOT(managerPropertiesChanged(QString,QVariantMap,QStringList)));
    bus.connect(QString::fromLatin1(serviceName), QString::fromLatin1(managerPath),
                QString::fromLatin1(managerInterface), QStringLiteral("DeviceAdded"),
                this, SLOT(deviceAdded(QDBusObjectPath)));
    bus.connect(QString::fromLatin1(serviceName), QString::fromLatin1(managerPath),
                QString::fromLatin1(managerInterface), QStringLiteral("DeviceRemoved"),
                this, SLOT(deviceRemoved(QDBusObjectPath)));
    probe();
    return true;
}

void NetworkManagerBackend::stop()
{
    if (!m_running && !m_serviceWatcher)
        return;
    m_running = false;
    ++m_generation;
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.disconnect(QString::fromLatin1(serviceName), QString::fromLatin1(managerPath),
                   QString::fromLatin1(propertiesInterface), QStringLiteral("PropertiesChanged"),
                   this, nullptr);
    bus.disconnect(QString::fromLatin1(serviceName), QString::fromLatin1(managerPath),
                   QString::fromLatin1(managerInterface), QStringLiteral("DeviceAdded"),
                   this, nullptr);
    bus.disconnect(QString::fromLatin1(serviceName), QString::fromLatin1(managerPath),
                   QString::fromLatin1(managerInterface), QStringLiteral("DeviceRemoved"),
                   this, nullptr);
    rebuildPropertyWatchers();
    m_scanState.invalidate();
    if (m_scanCooldownTimer) {
        m_scanCooldownTimer->stop();
        delete m_scanCooldownTimer;
    }
    m_scanCooldownTimer = nullptr;
    if (m_trafficTimer) {
        m_trafficTimer->stop();
        delete m_trafficTimer;
    }
    m_trafficTimer = nullptr;
    m_haveTrafficSample = false;
    m_trafficInterface.clear();
    delete m_serviceWatcher;
    m_serviceWatcher = nullptr;
    m_wifiDevicePath.clear();
    m_activeAccessPointPath.clear();
    m_managerProperties.clear();
    m_deviceProperties.clear();
    m_state.reset();
    m_pendingAccessPointProperties = 0;
    ++m_accessPointRefreshGeneration;
    m_snapshot = {};
    m_callbacks = {};
}

bool NetworkManagerBackend::setWifiEnabled(bool enabled, quint64 requestId)
{
    if (!m_running)
        return false;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), QString::fromLatin1(managerPath),
        QString::fromLatin1(propertiesInterface), QStringLiteral("Set"));
    call << QString::fromLatin1(managerInterface) << QStringLiteral("WirelessEnabled")
         << QVariant::fromValue(QDBusVariant(QVariant(enabled)));
    const quint64 generation = m_generation;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call),
                                                 this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation, requestId](QDBusPendingCallWatcher *finished) {
        if (generation == m_generation && m_running) {
            reportAsyncError(finished, m_callbacks);
            const QDBusMessage reply = finished->reply();
            if (m_callbacks.operationFinished)
                m_callbacks.operationFinished(NetworkOperationResult{
                    NetworkOperationKind::WifiEnabled, requestId,
                    reply.type() != QDBusMessage::ErrorMessage,
                    reply.type() == QDBusMessage::ErrorMessage ? reply.errorMessage() : QString()});
        }
        finished->deleteLater();
    });
    return true;
}

bool NetworkManagerBackend::requestWifiScan()
{
    if (!m_running || m_wifiDevicePath.isEmpty() || !m_snapshot.wifiAvailable)
        return false;
    const qint64 now = m_scanClock.elapsed();
    const auto device = m_state.devices().constFind(m_wifiDevicePath);
    const qint64 lastScan = device == m_state.devices().cend()
        ? -1 : device->wirelessProperties.value(QStringLiteral("LastScan")).toLongLong();
    const quint64 requestId = ++m_scanOperationId;
    m_scanState.request(m_generation, m_wifiDevicePath, lastScan, now, requestId);
    if (m_scanState.phase() != NetworkScanPhase::RequestPending) {
        if (m_scanState.phase() == NetworkScanPhase::Cooldown && m_scanCooldownTimer)
            m_scanCooldownTimer->start(static_cast<int>(std::max<qint64>(1,
                m_scanState.cooldownUntilMs() - now)));
        m_snapshot.wifiScanning = m_scanState.active();
        publishReconciledSnapshot();
        return true;
    }
    m_snapshot.wifiScanning = true;
    publishReconciledSnapshot();
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), m_wifiDevicePath,
        QString::fromLatin1(wirelessInterface), QStringLiteral("RequestScan"));
    call << QVariantMap{};
    const quint64 generation = m_generation;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call),
                                                 this);
    const QString devicePath = m_wifiDevicePath;
    if (m_scanCooldownTimer)
        m_scanCooldownTimer->start(5000);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation, devicePath, requestId](QDBusPendingCallWatcher *finished) {
        if (generation != m_generation || !m_running) {
            finished->deleteLater();
            return;
        }
        reportAsyncError(finished, m_callbacks);
        const bool success = finished->reply().type() != QDBusMessage::ErrorMessage;
        const qint64 now = m_scanClock.elapsed();
        if (m_scanState.phase() != NetworkScanPhase::RequestPending
            || m_scanState.generation() != generation
            || m_scanState.devicePath() != devicePath
            || m_scanState.requestId() != requestId) {
            finished->deleteLater();
            return;
        }
        m_scanState.requestFinished(generation, devicePath, requestId, success, now);
        if (!success) {
            if (m_scanCooldownTimer)
                m_scanCooldownTimer->stop();
            m_snapshot.wifiScanning = false;
        }
        else if (m_scanState.phase() == NetworkScanPhase::WaitingForLastScan) {
            const auto device = m_state.devices().constFind(devicePath);
            const qint64 lastScan = device == m_state.devices().cend()
                ? -1 : device->wirelessProperties.value(QStringLiteral("LastScan")).toLongLong();
            if (m_scanState.lastScanAdvanced(m_generation, devicePath, lastScan, now)) {
                if (m_scanCooldownTimer)
                    m_scanCooldownTimer->start(3000);
            } else if (m_scanCooldownTimer) {
                m_scanCooldownTimer->start(5000);
            }
        }
        finished->deleteLater();
        if (success && devicePath == m_wifiDevicePath) {
            refreshAccessPoints();
        } else if (!success && m_scanState.phase() == NetworkScanPhase::Cooldown) {
            if (m_scanCooldownTimer)
                m_scanCooldownTimer->start(3000);
            publishReconciledSnapshot();
        } else {
            publishReconciledSnapshot();
        }
    });
    if (m_scanCooldownTimer)
        m_scanCooldownTimer->start(5000);
    return true;
}

void NetworkManagerBackend::probe()
{
    if (!m_running)
        return;
    const quint64 generation = m_generation;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), QString::fromLatin1(managerPath),
        QString::fromLatin1(propertiesInterface), QStringLiteral("GetAll"));
    call << QString::fromLatin1(managerInterface);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call),
                                                 this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher *finished) {
        if (generation != m_generation || !m_running) {
            finished->deleteLater();
            return;
        }
        const QDBusMessage reply = finished->reply();
        if (reply.type() == QDBusMessage::ErrorMessage) {
            if (m_callbacks.errorChanged)
                m_callbacks.errorChanged(reply.errorMessage());
            publishUnavailable();
        } else if (!reply.arguments().isEmpty()) {
            publishFromProperties(propertyMapFromDbus(reply.arguments().constFirst()));
        } else {
            publishFromProperties({});
        }
        finished->deleteLater();
    });
}

void NetworkManagerBackend::refreshDevices()
{
    if (!m_running)
        return;
    const quint64 generation = m_generation;
    const quint64 refreshGeneration = ++m_refreshGeneration;
    m_activeConnectionPath.clear();
    m_activeAccessPointPath.clear();
    m_wifiDevicePath.clear();
    m_state.clearPrimary();
    m_primaryEpoch = m_state.primaryEpoch();
    m_state.clearAccessPoints();
    ++m_wiredPropertiesRequestId;
    ++m_ipv4ConfigRequestId;
    ++m_ipv6ConfigRequestId;
    m_deviceProperties.clear();
    rebuildPropertyWatchers();
    m_snapshot = m_state.snapshot();
    m_snapshot.wifiScanning = m_scanState.active();
    publishReconciledSnapshot();
    m_pendingDeviceProperties = 0;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), QString::fromLatin1(managerPath),
        QString::fromLatin1(managerInterface), QStringLiteral("GetDevices"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call),
                                                 this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation, refreshGeneration](QDBusPendingCallWatcher *finished) {
        if (generation != m_generation || refreshGeneration != m_refreshGeneration
            || !m_running) {
            finished->deleteLater();
            return;
        }
        const QDBusMessage reply = finished->reply();
        QVector<QString> paths;
        if (reply.type() != QDBusMessage::ErrorMessage && !reply.arguments().isEmpty()) {
            QDBusArgument argument = reply.arguments().constFirst().value<QDBusArgument>();
            argument.beginArray();
            while (!argument.atEnd()) {
                QDBusObjectPath path;
                argument >> path;
                paths.append(path.path());
            }
            argument.endArray();
        }
        std::sort(paths.begin(), paths.end());
        m_state.clearAccessPoints();
        m_pendingDeviceProperties = paths.size();
        if (m_pendingDeviceProperties == 0) {
            rebuildPropertyWatchers();
            reconcileDevices(generation);
            finished->deleteLater();
            return;
        }
        for (const QString &path : paths) {
            QDBusMessage propertiesCall = QDBusMessage::createMethodCall(
                QString::fromLatin1(serviceName), path,
                QString::fromLatin1(propertiesInterface), QStringLiteral("GetAll"));
            propertiesCall << QString::fromLatin1(deviceInterface);
            auto *propertiesWatcher = new QDBusPendingCallWatcher(
                QDBusConnection::systemBus().asyncCall(propertiesCall), this);
            connect(propertiesWatcher, &QDBusPendingCallWatcher::finished, this,
                    [this, generation, refreshGeneration, path](QDBusPendingCallWatcher *deviceWatcher) {
                if (generation == m_generation && refreshGeneration == m_refreshGeneration
                    && m_running) {
                    const QDBusMessage deviceReply = deviceWatcher->reply();
                    if (deviceReply.type() != QDBusMessage::ErrorMessage
                        && !deviceReply.arguments().isEmpty())
                        m_deviceProperties.insert(
                            path, propertyMapFromDbus(deviceReply.arguments().constFirst()));
                    if (--m_pendingDeviceProperties == 0) {
                        rebuildPropertyWatchers();
                        reconcileDevices(generation);
                    }
                }
                deviceWatcher->deleteLater();
            });
        }
        finished->deleteLater();
    });
}

void NetworkManagerBackend::reconcileDevices(quint64 generation)
{
    if (!m_running || generation != m_generation)
        return;
    m_state.reset();
    m_state.setManagerProperties(m_managerProperties);
    for (auto it = m_deviceProperties.cbegin(); it != m_deviceProperties.cend(); ++it)
        m_state.upsertDevice(it.key(), it.value());
    QStringList wifiPaths;
    for (auto it = m_deviceProperties.cbegin(); it != m_deviceProperties.cend(); ++it) {
        if (it.value().value(QStringLiteral("DeviceType")).toUInt() == 2)
            wifiPaths.append(it.key());
    }
    std::sort(wifiPaths.begin(), wifiPaths.end());
    const quint64 refreshGeneration = m_refreshGeneration;
    m_wirelessRefresh.begin(generation, refreshGeneration, wifiPaths);
    if (!wifiPaths.isEmpty()) {
        for (const QString &path : wifiPaths) {
            QDBusMessage wirelessCall = QDBusMessage::createMethodCall(
                QString::fromLatin1(serviceName), path,
                QString::fromLatin1(propertiesInterface), QStringLiteral("GetAll"));
            wirelessCall << QString::fromLatin1(wirelessInterface);
            auto *wirelessWatcher = new QDBusPendingCallWatcher(
                QDBusConnection::systemBus().asyncCall(wirelessCall), this);
            connect(wirelessWatcher, &QDBusPendingCallWatcher::finished, this,
                    [this, generation, refreshGeneration, path](QDBusPendingCallWatcher *finished) {
                if (generation != m_generation || refreshGeneration != m_refreshGeneration
                    || !m_running
                    || !m_wirelessRefresh.acceptReply(generation, refreshGeneration, path)) {
                    finished->deleteLater();
                    return;
                }
                const QDBusMessage reply = finished->reply();
                if (reply.type() != QDBusMessage::ErrorMessage && !reply.arguments().isEmpty())
                    m_state.setWirelessProperties(
                        path, propertyMapFromDbus(reply.arguments().constFirst()));
                if (m_wirelessRefresh.complete())
                    finishDeviceReconciliation(generation);
                finished->deleteLater();
            });
        }
        return;
    }
    finishDeviceReconciliation(generation);
}

void NetworkManagerBackend::finishDeviceReconciliation(quint64 generation)
{
    if (!m_running || generation != m_generation)
        return;
    const bool wifiDeviceChanged = reconcileSelectedWifiDevice();
    m_snapshot = m_state.snapshot();
    m_snapshot.wifiScanning = m_scanState.active();
    const QString primaryPath = objectPathString(
        m_managerProperties.value(QStringLiteral("PrimaryConnection")));
    if (!primaryPath.isEmpty() && primaryPath != QLatin1String("/")) {
        refreshActiveConnection(primaryPath);
        return;
    }
    publishReconciledSnapshot();
    if (!wifiDeviceChanged && m_snapshot.wifiAvailable)
        refreshAccessPoints();
}

bool NetworkManagerBackend::reconcileSelectedWifiDevice()
{
    if (!m_running)
        return false;
    const QString next = m_state.selectedWifiDevicePath();
    if (next == m_wifiDevicePath) {
        m_activeAccessPointPath = m_state.activeAccessPointPath();
        return false;
    }

    ++m_accessPointRefreshGeneration;
    m_scanState.invalidate();
    if (m_scanCooldownTimer)
        m_scanCooldownTimer->stop();
    m_state.clearAccessPoints();
    m_wifiDevicePath = next;
    m_activeAccessPointPath = m_state.activeAccessPointPath();
    m_snapshot = m_state.snapshot();
    m_snapshot.wifiScanning = false;
    rebuildPropertyWatchers();
    publishReconciledSnapshot();
    if (!m_wifiDevicePath.isEmpty() && m_snapshot.wifiAvailable)
        refreshAccessPoints();
    return true;
}

void NetworkManagerBackend::refreshActiveConnection(const QString &objectPath)
{
    if (!m_running || objectPath.isEmpty())
        return;
    const quint64 generation = m_generation;
    const quint64 refreshGeneration = m_refreshGeneration;
    const quint64 primaryEpoch = m_state.beginPrimaryRequest(objectPath);
    m_primaryEpoch = primaryEpoch;
    m_activeConnectionPath = objectPath;
    ++m_wiredPropertiesRequestId;
    ++m_ipv4ConfigRequestId;
    ++m_ipv6ConfigRequestId;
    m_snapshot = m_state.snapshot();
    m_snapshot.wifiScanning = m_scanState.active();
    rebuildPropertyWatchers();
    publishReconciledSnapshot();
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), objectPath,
        QString::fromLatin1(propertiesInterface), QStringLiteral("GetAll"));
    call << QString::fromLatin1(activeConnectionInterface);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation, refreshGeneration, primaryEpoch,
             objectPath](QDBusPendingCallWatcher *finished) {
        if (generation != m_generation || refreshGeneration != m_refreshGeneration
            || primaryEpoch != m_primaryEpoch || objectPath != m_activeConnectionPath
            || !m_running) {
            finished->deleteLater();
            return;
        }
        const QDBusMessage reply = finished->reply();
        bool wifiDeviceChanged = false;
        bool primaryReplyApplied = false;
        if (reply.type() == QDBusMessage::ErrorMessage) {
            if (m_callbacks.errorChanged)
                m_callbacks.errorChanged(reply.errorMessage());
        } else if (!reply.arguments().isEmpty()) {
            const QVariantMap properties = propertyMapFromDbus(reply.arguments().constFirst());
            if (!m_state.applyPrimaryReply(objectPath, primaryEpoch, properties)) {
                finished->deleteLater();
                return;
            }
            primaryReplyApplied = true;
            wifiDeviceChanged = reconcileSelectedWifiDevice();
            m_snapshot = m_state.snapshot();
            m_snapshot.wifiScanning = m_scanState.active();
        }
        if (!wifiDeviceChanged) {
            publishReconciledSnapshot();
            if (m_snapshot.wifiAvailable)
                refreshAccessPoints();
        }
        if (primaryReplyApplied) {
            rebuildPropertyWatchers();
            refreshPrimaryRuntimeDetails();
        }
        finished->deleteLater();
    });
}

void NetworkManagerBackend::refreshPrimaryRuntimeDetails(bool refreshWired,
                                                         bool refreshIpv4,
                                                         bool refreshIpv6)
{
    if (!m_running || m_activeConnectionPath.isEmpty()
        || m_state.primaryPath() != m_activeConnectionPath
        || m_state.primaryEpoch() != m_primaryEpoch)
        return;

    const QString devicePath = m_state.primaryDevicePath();
    const auto device = m_state.devices().constFind(devicePath);
    const bool connected = !devicePath.isEmpty()
        && m_state.snapshot().connected;
    if (!connected)
        return;

    if (refreshWired && device != m_state.devices().cend()
        && device->deviceProperties.value(QStringLiteral("DeviceType")).toUInt()
            == ethernetDeviceType)
        refreshWiredProperties(devicePath);
    if (refreshIpv4 && !m_state.primaryIpv4Config().objectPath.isEmpty())
        refreshPrimaryIpConfig(true);
    if (refreshIpv6 && !m_state.primaryIpv6Config().objectPath.isEmpty())
        refreshPrimaryIpConfig(false);
}

void NetworkManagerBackend::refreshWiredProperties(const QString &devicePath)
{
    if (!m_running || devicePath.isEmpty() || devicePath != m_state.primaryDevicePath()
        || m_state.primaryPath() != m_activeConnectionPath
        || m_state.primaryEpoch() != m_primaryEpoch)
        return;
    const auto device = m_state.devices().constFind(devicePath);
    if (device == m_state.devices().cend()
        || device->deviceProperties.value(QStringLiteral("DeviceType")).toUInt()
            != ethernetDeviceType
        || !m_state.snapshot().connected)
        return;

    const quint64 generation = m_generation;
    const quint64 refreshGeneration = m_refreshGeneration;
    const quint64 primaryEpoch = m_primaryEpoch;
    const quint64 requestId = ++m_wiredPropertiesRequestId;
    const QString activeConnectionPath = m_activeConnectionPath;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), devicePath,
        QString::fromLatin1(propertiesInterface), QStringLiteral("GetAll"));
    call << QString::fromLatin1(wiredInterface);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation, refreshGeneration, primaryEpoch, requestId,
             activeConnectionPath, devicePath](QDBusPendingCallWatcher *finished) {
        if (generation != m_generation || refreshGeneration != m_refreshGeneration
            || primaryEpoch != m_primaryEpoch || requestId != m_wiredPropertiesRequestId
            || activeConnectionPath != m_activeConnectionPath || !m_running
            || devicePath != m_state.primaryDevicePath()) {
            finished->deleteLater();
            return;
        }
        const QDBusMessage reply = finished->reply();
        QVariantMap properties;
        if (reply.type() != QDBusMessage::ErrorMessage && !reply.arguments().isEmpty())
            properties = propertyMapFromDbus(reply.arguments().constFirst());
        if (m_state.applyWiredPropertiesReply(activeConnectionPath, primaryEpoch,
                                              devicePath, std::move(properties))) {
            m_snapshot = m_state.snapshot();
            m_snapshot.wifiScanning = m_scanState.active();
            publishReconciledSnapshot();
        }
        finished->deleteLater();
    });
}

void NetworkManagerBackend::refreshPrimaryIpConfig(bool ipv4)
{
    if (!m_running || m_activeConnectionPath.isEmpty()
        || m_state.primaryPath() != m_activeConnectionPath
        || m_state.primaryEpoch() != m_primaryEpoch
        || !m_state.snapshot().connected)
        return;
    const PrimaryIpConfigState &config = ipv4 ? m_state.primaryIpv4Config()
                                               : m_state.primaryIpv6Config();
    if (config.objectPath.isEmpty() || config.objectPath == QLatin1String("/"))
        return;

    const quint64 generation = m_generation;
    const quint64 refreshGeneration = m_refreshGeneration;
    const quint64 primaryEpoch = m_primaryEpoch;
    const QString activeConnectionPath = m_activeConnectionPath;
    const QString configPath = config.objectPath;
    quint64 requestId = 0;
    if (ipv4)
        requestId = ++m_ipv4ConfigRequestId;
    else
        requestId = ++m_ipv6ConfigRequestId;
    const QString interfaceName = QString::fromLatin1(
        ipv4 ? ip4ConfigInterface : ip6ConfigInterface);
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), configPath,
        QString::fromLatin1(propertiesInterface), QStringLiteral("GetAll"));
    call << interfaceName;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, ipv4, generation, refreshGeneration, primaryEpoch, requestId,
             activeConnectionPath, configPath, interfaceName](QDBusPendingCallWatcher *finished) {
        const quint64 currentRequestId = ipv4 ? m_ipv4ConfigRequestId : m_ipv6ConfigRequestId;
        const PrimaryIpConfigState &currentConfig = ipv4 ? m_state.primaryIpv4Config()
                                                          : m_state.primaryIpv6Config();
        if (generation != m_generation || refreshGeneration != m_refreshGeneration
            || primaryEpoch != m_primaryEpoch || requestId != currentRequestId
            || activeConnectionPath != m_activeConnectionPath
            || configPath != currentConfig.objectPath || !m_running) {
            finished->deleteLater();
            return;
        }
        const QDBusMessage reply = finished->reply();
        QVariantMap properties;
        if (reply.type() != QDBusMessage::ErrorMessage && !reply.arguments().isEmpty())
            properties = propertyMapFromDbus(reply.arguments().constFirst());
        if (m_state.applyPrimaryIpConfigReply(activeConnectionPath, primaryEpoch,
                                              configPath, ipv4, std::move(properties))) {
            m_snapshot = m_state.snapshot();
            m_snapshot.wifiScanning = m_scanState.active();
            publishReconciledSnapshot();
        }
        finished->deleteLater();
    });
}

void NetworkManagerBackend::refreshAccessPoints()
{
    if (!m_running || m_wifiDevicePath.isEmpty())
        return;
    const quint64 generation = m_generation;
    const quint64 refreshGeneration = m_refreshGeneration;
    const quint64 accessPointRefreshGeneration = ++m_accessPointRefreshGeneration;
    const QString wifiDevicePath = m_wifiDevicePath;
    m_state.clearAccessPoints();
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), wifiDevicePath,
        QString::fromLatin1(wirelessInterface), QStringLiteral("GetAllAccessPoints"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation, refreshGeneration, accessPointRefreshGeneration,
             wifiDevicePath](QDBusPendingCallWatcher *finished) {
        if (generation != m_generation || refreshGeneration != m_refreshGeneration
            || accessPointRefreshGeneration != m_accessPointRefreshGeneration
            || wifiDevicePath != m_wifiDevicePath || !m_running) {
            finished->deleteLater();
            return;
        }
        QVector<QString> paths;
        const QDBusMessage reply = finished->reply();
        if (reply.type() != QDBusMessage::ErrorMessage && !reply.arguments().isEmpty()) {
            QDBusArgument argument = reply.arguments().constFirst().value<QDBusArgument>();
            argument.beginArray();
            while (!argument.atEnd()) {
                QDBusObjectPath path;
                argument >> path;
                paths.append(path.path());
            }
            argument.endArray();
        }
        std::sort(paths.begin(), paths.end());
        m_pendingAccessPointProperties = paths.size();
        if (paths.isEmpty()) {
            m_snapshot = m_state.snapshot();
            m_snapshot.wifiScanning = m_scanState.active();
            publishReconciledSnapshot();
            finished->deleteLater();
            return;
        }
        for (const QString &path : paths) {
            QDBusMessage propertiesCall = QDBusMessage::createMethodCall(
                QString::fromLatin1(serviceName), path,
                QString::fromLatin1(propertiesInterface), QStringLiteral("GetAll"));
            propertiesCall << QString::fromLatin1(accessPointInterface);
            auto *propertiesWatcher = new QDBusPendingCallWatcher(
                QDBusConnection::systemBus().asyncCall(propertiesCall), this);
            connect(propertiesWatcher, &QDBusPendingCallWatcher::finished, this,
                    [this, generation, refreshGeneration, accessPointRefreshGeneration,
                     path](QDBusPendingCallWatcher *apWatcher) {
                if (generation == m_generation && refreshGeneration == m_refreshGeneration
                    && accessPointRefreshGeneration == m_accessPointRefreshGeneration
                    && m_running) {
                    const QDBusMessage apReply = apWatcher->reply();
                    if (apReply.type() != QDBusMessage::ErrorMessage
                        && !apReply.arguments().isEmpty()) {
                        const QVariantMap properties = propertyMapFromDbus(
                            apReply.arguments().constFirst());
                        m_state.upsertAccessPoint(path, properties);
                    }
                    if (--m_pendingAccessPointProperties == 0) {
                        m_snapshot = m_state.snapshot();
                        m_snapshot.wifiScanning = m_scanState.active();
                        publishReconciledSnapshot();
                        rebuildPropertyWatchers();
                    }
                }
                apWatcher->deleteLater();
            });
        }
        finished->deleteLater();
    });
}

void NetworkManagerBackend::refreshAccessPoint(const QString &objectPath)
{
    if (!m_running || objectPath.isEmpty() || m_wifiDevicePath.isEmpty())
        return;
    const quint64 generation = m_generation;
    const quint64 refreshGeneration = m_refreshGeneration;
    const quint64 accessPointRefreshGeneration = m_accessPointRefreshGeneration;
    const QString wifiDevicePath = m_wifiDevicePath;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), objectPath,
        QString::fromLatin1(propertiesInterface), QStringLiteral("GetAll"));
    call << QString::fromLatin1(accessPointInterface);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation, refreshGeneration, accessPointRefreshGeneration, wifiDevicePath,
             objectPath](QDBusPendingCallWatcher *finished) {
        if (generation == m_generation && refreshGeneration == m_refreshGeneration
            && accessPointRefreshGeneration == m_accessPointRefreshGeneration
            && wifiDevicePath == m_wifiDevicePath && m_running) {
            const QDBusMessage reply = finished->reply();
            if (reply.type() != QDBusMessage::ErrorMessage && !reply.arguments().isEmpty()) {
                m_state.upsertAccessPoint(
                    objectPath, propertyMapFromDbus(reply.arguments().constFirst()));
                m_snapshot = m_state.snapshot();
                m_snapshot.wifiScanning = m_scanState.active();
                publishReconciledSnapshot();
                rebuildPropertyWatchers();
            }
        }
        finished->deleteLater();
    });
}

void NetworkManagerBackend::managerPropertiesChanged(const QString &interfaceName,
                                                     const QVariantMap &changed,
                                                     const QStringList &invalidated)
{
    if (!m_running || interfaceName != QString::fromLatin1(managerInterface))
        return;
    Q_UNUSED(invalidated)
    for (auto it = changed.constBegin(); it != changed.constEnd(); ++it)
        m_managerProperties.insert(it.key(), it.value());
    refreshDevices();
}

void NetworkManagerBackend::interfacesAdded(const QDBusObjectPath &, const QVariantMap &)
{
    probe();
}

void NetworkManagerBackend::interfacesRemoved(const QDBusObjectPath &, const QStringList &)
{
    probe();
}

void NetworkManagerBackend::deviceAdded(const QDBusObjectPath &)
{
    probe();
}

void NetworkManagerBackend::deviceRemoved(const QDBusObjectPath &path)
{
    m_state.removeDevice(path.path());
    if (path.path() == m_wifiDevicePath) {
        ++m_accessPointRefreshGeneration;
        m_scanState.invalidate();
        if (m_scanCooldownTimer)
            m_scanCooldownTimer->stop();
        m_state.clearAccessPoints();
        m_wifiDevicePath.clear();
        m_activeAccessPointPath.clear();
        m_snapshot = m_state.snapshot();
        m_snapshot.wifiScanning = false;
        publishReconciledSnapshot();
    }
    probe();
}

void NetworkManagerBackend::rebuildPropertyWatchers()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    for (NetworkPropertyWatcher *watcher : std::as_const(m_propertyWatchers)) {
        bus.disconnect(QString::fromLatin1(serviceName), watcher->path,
                       QString::fromLatin1(propertiesInterface), QStringLiteral("PropertiesChanged"),
                       watcher, nullptr);
        bus.disconnect(QString::fromLatin1(serviceName), watcher->path,
                       QString::fromLatin1(activeConnectionInterface), QStringLiteral("StateChanged"),
                       watcher, nullptr);
        bus.disconnect(QString::fromLatin1(serviceName), watcher->path,
                       QString::fromLatin1(wirelessInterface), QStringLiteral("AccessPointAdded"),
                       watcher, nullptr);
        bus.disconnect(QString::fromLatin1(serviceName), watcher->path,
                       QString::fromLatin1(wirelessInterface), QStringLiteral("AccessPointRemoved"),
                       watcher, nullptr);
        delete watcher;
    }
    m_propertyWatchers.clear();
    if (!m_running)
        return;
    QStringList paths = m_deviceProperties.keys();
    paths.append(m_activeConnectionPath);
    paths.append(m_state.primaryIpv4Config().objectPath);
    paths.append(m_state.primaryIpv6Config().objectPath);
    for (auto it = m_state.accessPoints().cbegin(); it != m_state.accessPoints().cend(); ++it)
        paths.append(it.key());
    paths.removeDuplicates();
    for (const QString &path : paths) {
        if (path.isEmpty())
            continue;
        auto *watcher = new NetworkPropertyWatcher(path, this);
        connect(watcher, &NetworkPropertyWatcher::propertiesUpdated, this,
                [this](const QString &path, const QString &interfaceName,
                       const QVariantMap &changed, const QStringList &invalidated) {
            if (!m_running)
                return;
            if (interfaceName == QString::fromLatin1(wiredInterface)) {
                if (path != m_state.primaryDevicePath())
                    return;
                ++m_wiredPropertiesRequestId;
                m_state.updateDeviceProperties(path, interfaceName, changed, invalidated);
                m_snapshot = m_state.snapshot();
                m_snapshot.wifiScanning = m_scanState.active();
                publishReconciledSnapshot();
                if (!invalidated.isEmpty())
                    refreshWiredProperties(path);
                return;
            }
            const bool isIpv4Config = interfaceName == QString::fromLatin1(ip4ConfigInterface);
            const bool isIpv6Config = interfaceName == QString::fromLatin1(ip6ConfigInterface);
            if (isIpv4Config || isIpv6Config) {
                const bool relevantChanged = changed.contains(QStringLiteral("AddressData"))
                    || changed.contains(QStringLiteral("Gateway"));
                const bool relevantInvalidated = invalidated.contains(
                    QStringLiteral("AddressData"))
                    || invalidated.contains(QStringLiteral("Gateway"));
                if (!relevantChanged && !relevantInvalidated)
                    return;
                if (!m_state.updatePrimaryIpConfigProperties(path, interfaceName,
                                                              changed, invalidated))
                    return;
                if (isIpv4Config)
                    ++m_ipv4ConfigRequestId;
                else
                    ++m_ipv6ConfigRequestId;
                m_snapshot = m_state.snapshot();
                m_snapshot.wifiScanning = m_scanState.active();
                publishReconciledSnapshot();
                refreshPrimaryIpConfig(isIpv4Config);
                return;
            }
            if (interfaceName == QString::fromLatin1(deviceInterface)
                || interfaceName == QString::fromLatin1(wirelessInterface)) {
                m_state.updateDeviceProperties(path, interfaceName, changed, invalidated);
                if (interfaceName == QString::fromLatin1(wirelessInterface)
                    && changed.contains(QStringLiteral("LastScan"))) {
                    if (m_scanState.lastScanAdvanced(
                            m_generation, path,
                            changed.value(QStringLiteral("LastScan")).toLongLong(),
                            m_scanClock.elapsed())) {
                        if (m_scanCooldownTimer) {
                            m_scanCooldownTimer->stop();
                            m_scanCooldownTimer->start(3000);
                        }
                    }
                }
                if (interfaceName == QString::fromLatin1(wirelessInterface)
                    && changed.contains(QStringLiteral("ActiveAccessPoint"))) {
                    const QString activePath = objectPathString(
                        changed.value(QStringLiteral("ActiveAccessPoint")));
                    if (!activePath.isEmpty() && activePath != QLatin1String("/"))
                        refreshAccessPoint(activePath);
                }
                m_snapshot = m_state.snapshot();
                m_snapshot.wifiScanning = m_scanState.active();
                publishReconciledSnapshot();
                if (!invalidated.isEmpty())
                    refreshDevices();
                return;
            }
            if (interfaceName == QString::fromLatin1(activeConnectionInterface)
                && path == m_activeConnectionPath) {
                const QString oldDevicePath = m_state.primaryDevicePath();
                const QString oldIpv4Path = m_state.primaryIpv4Config().objectPath;
                const QString oldIpv6Path = m_state.primaryIpv6Config().objectPath;
                m_state.updatePrimaryProperties(changed, invalidated);
                if (!invalidated.isEmpty() || changed.contains(QStringLiteral("State"))) {
                    refreshActiveConnection(path);
                    return;
                }
                const bool deviceChanged = oldDevicePath != m_state.primaryDevicePath();
                const bool ipv4Changed = oldIpv4Path != m_state.primaryIpv4Config().objectPath;
                const bool ipv6Changed = oldIpv6Path != m_state.primaryIpv6Config().objectPath;
                reconcileSelectedWifiDevice();
                m_snapshot = m_state.snapshot();
                m_snapshot.wifiScanning = m_scanState.active();
                if (ipv4Changed || ipv6Changed)
                    rebuildPropertyWatchers();
                publishReconciledSnapshot();
                if (deviceChanged || ipv4Changed || ipv6Changed)
                    refreshPrimaryRuntimeDetails(deviceChanged, ipv4Changed, ipv6Changed);
                return;
            }
            if (interfaceName == QString::fromLatin1(accessPointInterface)) {
                m_state.updateAccessPoint(path, changed, invalidated);
                m_snapshot = m_state.snapshot();
                m_snapshot.wifiScanning = m_scanState.active();
                publishReconciledSnapshot();
                if (!invalidated.isEmpty())
                    refreshAccessPoints();
            }
        });
        connect(watcher, &NetworkPropertyWatcher::accessPointAddedSignal, this,
                [this](const QString &point) { refreshAccessPoint(point); });
        connect(watcher, &NetworkPropertyWatcher::accessPointRemovedSignal, this,
                [this](const QString &point) {
            ++m_accessPointRefreshGeneration;
            m_state.removeAccessPoint(point);
            m_snapshot = m_state.snapshot();
            m_snapshot.wifiScanning = m_scanState.active();
            publishReconciledSnapshot();
            rebuildPropertyWatchers();
        });
        bus.connect(QString::fromLatin1(serviceName), watcher->path,
                    QString::fromLatin1(propertiesInterface), QStringLiteral("PropertiesChanged"),
                    watcher, SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
        if (watcher->path == m_activeConnectionPath)
            bus.connect(QString::fromLatin1(serviceName), watcher->path,
                        QString::fromLatin1(activeConnectionInterface), QStringLiteral("StateChanged"),
                        watcher, SLOT(activeStateChanged(uint,uint)));
        if (watcher->path == m_wifiDevicePath) {
            bus.connect(QString::fromLatin1(serviceName), watcher->path,
                        QString::fromLatin1(wirelessInterface), QStringLiteral("AccessPointAdded"),
                        watcher, SLOT(accessPointAdded(QDBusObjectPath)));
            bus.connect(QString::fromLatin1(serviceName), watcher->path,
                        QString::fromLatin1(wirelessInterface), QStringLiteral("AccessPointRemoved"),
                        watcher, SLOT(accessPointRemoved(QDBusObjectPath)));
        }
        m_propertyWatchers.append(watcher);
    }
}

void NetworkManagerBackend::publishUnavailable()
{
    ++m_generation;
    ++m_accessPointRefreshGeneration;
    m_managerProperties.clear();
    m_deviceProperties.clear();
    m_wifiDevicePath.clear();
    m_activeAccessPointPath.clear();
    m_activeConnectionPath.clear();
    m_state.reset();
    m_scanState.invalidate();
    m_haveTrafficSample = false;
    m_trafficInterface.clear();
    rebuildPropertyWatchers();
    m_snapshot = {};
    if (m_callbacks.snapshotChanged)
        m_callbacks.snapshotChanged(m_snapshot);
}

void NetworkManagerBackend::publishFromProperties(const QVariantMap &properties)
{
    if (!m_running)
        return;
    m_managerProperties = properties;
    refreshDevices();
}

void NetworkManagerBackend::publishReconciledSnapshot()
{
    if (!m_running || !m_callbacks.snapshotChanged)
        return;
    m_snapshot.daemonAvailable = true;
    m_callbacks.snapshotChanged(m_snapshot);
}

void NetworkManagerBackend::sampleTraffic()
{
    if (!m_running || m_snapshot.interfaceName.isEmpty() || !m_callbacks.snapshotChanged)
        return;
    const QString interfaceName = m_snapshot.interfaceName;
    if (m_trafficInterface != interfaceName) {
        m_trafficInterface = interfaceName;
        m_haveTrafficSample = false;
        m_previousRx = 0;
        m_previousTx = 0;
        m_trafficClock.restart();
    }
    QFile rxFile(QStringLiteral("/sys/class/net/%1/statistics/rx_bytes").arg(interfaceName));
    QFile txFile(QStringLiteral("/sys/class/net/%1/statistics/tx_bytes").arg(interfaceName));
    if (!rxFile.open(QIODevice::ReadOnly) || !txFile.open(QIODevice::ReadOnly))
        return;
    bool rxOk = false;
    bool txOk = false;
    const quint64 rx = rxFile.readAll().trimmed().toULongLong(&rxOk);
    const quint64 tx = txFile.readAll().trimmed().toULongLong(&txOk);
    const qint64 elapsed = m_trafficClock.restart();
    if (!rxOk || !txOk || elapsed <= 0 || !m_haveTrafficSample
        || rx < m_previousRx || tx < m_previousTx) {
        m_previousRx = rx;
        m_previousTx = tx;
        m_haveTrafficSample = rxOk && txOk;
        m_snapshot.downloadBytesPerSecond = 0;
        m_snapshot.uploadBytesPerSecond = 0;
        m_callbacks.snapshotChanged(m_snapshot);
        return;
    }
    m_snapshot.downloadBytesPerSecond = (rx - m_previousRx) * 1000 / elapsed;
    m_snapshot.uploadBytesPerSecond = (tx - m_previousTx) * 1000 / elapsed;
    m_previousRx = rx;
    m_previousTx = tx;
    m_callbacks.snapshotChanged(m_snapshot);
}

} // namespace Astrea::System

#include "NetworkManagerBackend.moc"
