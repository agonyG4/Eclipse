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

public slots:
    void propertiesChanged(const QString &interfaceName, const QVariantMap &changed,
                            const QStringList &invalidated)
    {
        emit propertiesUpdated(path, interfaceName, changed, invalidated);
    }
};

namespace {

constexpr auto serviceName = "org.freedesktop.NetworkManager";
constexpr auto managerPath = "/org/freedesktop/NetworkManager";
constexpr auto managerInterface = "org.freedesktop.NetworkManager";
constexpr auto propertiesInterface = "org.freedesktop.DBus.Properties";
constexpr auto deviceInterface = "org.freedesktop.NetworkManager.Device";
constexpr auto activeConnectionInterface = "org.freedesktop.NetworkManager.Connection.Active";
constexpr auto wirelessInterface = "org.freedesktop.NetworkManager.Device.Wireless";
constexpr auto accessPointInterface = "org.freedesktop.NetworkManager.AccessPoint";

QString objectPathString(const QVariant &value)
{
    return value.value<QDBusObjectPath>().path();
}

QVector<QString> objectPathArray(const QVariant &value)
{
    QVector<QString> paths;
    if (value.canConvert<QDBusArgument>()) {
        QDBusArgument argument = value.value<QDBusArgument>();
        argument.beginArray();
        while (!argument.atEnd()) {
            QDBusObjectPath path;
            argument >> path;
            paths.append(path.path());
        }
        argument.endArray();
    } else {
        for (const QVariant &item : value.toList())
            paths.append(objectPathString(item));
    }
    return paths;
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
    m_scanClock.start();
    m_lastScanMs = -1;
    m_trafficTimer = new QTimer(this);
    m_trafficTimer->setInterval(1000);
    connect(m_trafficTimer, &QTimer::timeout, this, &NetworkManagerBackend::sampleTraffic);
    m_trafficClock.start();
    m_trafficTimer->start();
    m_scanCooldownTimer = new QTimer(this);
    m_scanCooldownTimer->setSingleShot(true);
    connect(m_scanCooldownTimer, &QTimer::timeout, this, [this] {
        if (m_scanQueued) {
            m_scanQueued = false;
            requestWifiScan();
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
    m_scanQueued = false;
    m_scanInFlight = false;
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
    m_pendingAccessPoints.clear();
    m_pendingAccessPointProperties = 0;
    m_snapshot = {};
    m_callbacks = {};
}

bool NetworkManagerBackend::setWifiEnabled(bool enabled)
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
            [this, generation](QDBusPendingCallWatcher *finished) {
        if (generation == m_generation && m_running) {
            reportAsyncError(finished, m_callbacks);
            const QDBusMessage reply = finished->reply();
            if (m_callbacks.operationFinished)
                m_callbacks.operationFinished(QStringLiteral("wifi-enabled"),
                                               reply.type() != QDBusMessage::ErrorMessage,
                                               reply.type() == QDBusMessage::ErrorMessage
                                                   ? reply.errorMessage() : QString());
        }
        finished->deleteLater();
    });
    return true;
}

bool NetworkManagerBackend::requestWifiScan()
{
    if (!m_running || m_wifiDevicePath.isEmpty() || !m_snapshot.wifiAvailable)
        return false;
    if (m_scanInFlight) {
        m_scanQueued = true;
        return true;
    }
    const qint64 now = m_scanClock.elapsed();
    if (m_lastScanMs >= 0 && now - m_lastScanMs < 3000) {
        m_scanQueued = true;
        if (m_scanCooldownTimer)
            m_scanCooldownTimer->start(static_cast<int>(3000 - (now - m_lastScanMs)));
        return true;
    }
    m_lastScanMs = now;
    m_scanInFlight = true;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), m_wifiDevicePath,
        QString::fromLatin1(wirelessInterface), QStringLiteral("RequestScan"));
    call << QVariantMap{};
    const quint64 generation = m_generation;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call),
                                                 this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher *finished) {
        if (generation != m_generation || !m_running) {
            finished->deleteLater();
            return;
        }
        reportAsyncError(finished, m_callbacks);
        m_scanInFlight = false;
        finished->deleteLater();
        if (m_scanQueued) {
            if (m_scanCooldownTimer)
                m_scanCooldownTimer->start(3000);
        } else {
            refreshAccessPoints();
        }
    });
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
            publishFromProperties(reply.arguments().constFirst().toMap());
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
    m_pendingDeviceProperties = 0;
    m_deviceProperties.clear();
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
                        m_deviceProperties.insert(path,
                                                  deviceReply.arguments().constFirst().toMap());
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
    QStringList wifiPaths;
    for (auto it = m_deviceProperties.cbegin(); it != m_deviceProperties.cend(); ++it) {
        if (it.value().value(QStringLiteral("DeviceType")).toUInt() == 2)
            wifiPaths.append(it.key());
    }
    std::sort(wifiPaths.begin(), wifiPaths.end());
    m_wifiDevicePath = wifiPaths.isEmpty() ? QString() : wifiPaths.constFirst();
    m_activeAccessPointPath.clear();
    m_snapshot = {};
    m_snapshot.daemonAvailable = true;
    m_snapshot.wifiAvailable = !wifiPaths.isEmpty();
    m_snapshot.wifiEnabled = m_managerProperties.value(QStringLiteral("WirelessEnabled"))
                                 .toBool();
    if (!m_wifiDevicePath.isEmpty()) {
        const QVariantMap wifi = m_deviceProperties.value(m_wifiDevicePath);
        m_snapshot.wifiScanning = wifi.value(QStringLiteral("Scanning")).toBool();
        m_activeAccessPointPath = objectPathString(wifi.value(QStringLiteral("ActiveAccessPoint")));
    }
    const QString primaryPath = objectPathString(
        m_managerProperties.value(QStringLiteral("PrimaryConnection")));
    m_snapshot.connectionType = NetworkConnectionType::None;
    m_snapshot.connected = false;
    if (!primaryPath.isEmpty() && primaryPath != QLatin1String("/")) {
        refreshActiveConnection(primaryPath);
        return;
    }
    publishReconciledSnapshot();
    if (m_snapshot.wifiAvailable)
        refreshAccessPoints();
}

void NetworkManagerBackend::refreshActiveConnection(const QString &objectPath)
{
    if (!m_running || objectPath.isEmpty())
        return;
    const quint64 generation = m_generation;
    const quint64 refreshGeneration = m_refreshGeneration;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), objectPath,
        QString::fromLatin1(propertiesInterface), QStringLiteral("GetAll"));
    call << QString::fromLatin1(activeConnectionInterface);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation, refreshGeneration](QDBusPendingCallWatcher *finished) {
        if (generation != m_generation || refreshGeneration != m_refreshGeneration
            || !m_running) {
            finished->deleteLater();
            return;
        }
        const QDBusMessage reply = finished->reply();
        if (reply.type() == QDBusMessage::ErrorMessage) {
            if (m_callbacks.errorChanged)
                m_callbacks.errorChanged(reply.errorMessage());
        } else if (!reply.arguments().isEmpty()) {
            const QVariantMap properties = reply.arguments().constFirst().toMap();
            const QString type = properties.value(QStringLiteral("Type")).toString();
            const QVector<QString> activeDevices = objectPathArray(
                properties.value(QStringLiteral("Devices")));
            QStringList physical;
            for (const QString &path : activeDevices) {
                const uint deviceType = m_deviceProperties.value(path)
                                            .value(QStringLiteral("DeviceType"))
                                            .toUInt();
                if (deviceType == 1 || deviceType == 2)
                    physical.append(path);
            }
            std::sort(physical.begin(), physical.end());
            const QString physicalPath = physical.isEmpty() ? QString() : physical.constFirst();
            const QVariantMap physicalProperties = m_deviceProperties.value(physicalPath);
            const uint deviceType = physicalProperties.value(QStringLiteral("DeviceType")).toUInt();
            m_snapshot.connectionName = properties.value(QStringLiteral("Id")).toString();
            m_snapshot.connected = properties.value(QStringLiteral("State")).toUInt() == 2
                && !physicalPath.isEmpty();
            if (deviceType == 2)
                m_snapshot.connectionType = NetworkConnectionType::Wifi;
            else if (deviceType == 1)
                m_snapshot.connectionType = NetworkConnectionType::Wired;
            else if (type == QLatin1String("802-11-wireless"))
                m_snapshot.connectionType = NetworkConnectionType::Wifi;
            else if (type == QLatin1String("802-3-ethernet"))
                m_snapshot.connectionType = NetworkConnectionType::Wired;
            else
                m_snapshot.connectionType = NetworkConnectionType::None;
            if (m_snapshot.connected)
                m_snapshot.interfaceName = physicalProperties.value(QStringLiteral("Interface"))
                                               .toString();
        }
        publishReconciledSnapshot();
        if (m_snapshot.wifiAvailable)
            refreshAccessPoints();
        finished->deleteLater();
    });
}

void NetworkManagerBackend::refreshAccessPoints()
{
    if (!m_running || m_wifiDevicePath.isEmpty())
        return;
    const quint64 generation = m_generation;
    const quint64 refreshGeneration = m_refreshGeneration;
    const QString wifiDevicePath = m_wifiDevicePath;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), wifiDevicePath,
        QString::fromLatin1(wirelessInterface), QStringLiteral("GetAllAccessPoints"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation, refreshGeneration, wifiDevicePath](QDBusPendingCallWatcher *finished) {
        if (generation != m_generation || refreshGeneration != m_refreshGeneration
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
        m_pendingAccessPoints.clear();
        m_pendingAccessPointProperties = paths.size();
        if (paths.isEmpty()) {
            m_snapshot.wifiNetworks.clear();
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
                    [this, generation, refreshGeneration, path](QDBusPendingCallWatcher *apWatcher) {
                if (generation == m_generation && refreshGeneration == m_refreshGeneration
                    && m_running) {
                    const QDBusMessage apReply = apWatcher->reply();
                    if (apReply.type() != QDBusMessage::ErrorMessage
                        && !apReply.arguments().isEmpty()) {
                        const QVariantMap properties = apReply.arguments().constFirst().toMap();
                        WifiNetwork network;
                        network.ssid = QString::fromUtf8(
                            properties.value(QStringLiteral("Ssid")).toByteArray());
                        network.strength = properties.value(QStringLiteral("Strength")).toInt();
                        network.frequencyMHz = properties.value(QStringLiteral("Frequency")).toInt();
                        network.bssid = properties.value(QStringLiteral("HwAddress")).toString();
                        network.secured = properties.value(QStringLiteral("Flags")).toUInt() != 0
                            || properties.value(QStringLiteral("WpaFlags")).toUInt() != 0;
                        network.active = path == m_activeAccessPointPath;
                        m_pendingAccessPoints.append(std::move(network));
                    }
                    if (--m_pendingAccessPointProperties == 0) {
                        std::sort(m_pendingAccessPoints.begin(), m_pendingAccessPoints.end(),
                                  [](const WifiNetwork &left, const WifiNetwork &right) {
                            return std::tie(left.ssid, left.bssid) < std::tie(right.ssid, right.bssid);
                        });
                        m_snapshot.wifiNetworks = m_pendingAccessPoints;
                        publishReconciledSnapshot();
                    }
                }
                apWatcher->deleteLater();
            });
        }
        finished->deleteLater();
    });
}

void NetworkManagerBackend::publishAccessPoint(const QVariantMap &properties)
{
    WifiNetwork network;
    network.ssid = QString::fromUtf8(properties.value(QStringLiteral("Ssid")).toByteArray());
    network.strength = properties.value(QStringLiteral("Strength")).toInt();
    network.frequencyMHz = properties.value(QStringLiteral("Frequency")).toInt();
    network.bssid = properties.value(QStringLiteral("HwAddress")).toString();
    network.secured = properties.value(QStringLiteral("Flags")).toUInt() != 0
        || properties.value(QStringLiteral("WpaFlags")).toUInt() != 0;
    network.active = properties.value(QStringLiteral("_objectPath")).toString()
        == m_activeAccessPointPath;
    m_snapshot.wifiNetworks.append(std::move(network));
    publishReconciledSnapshot();
}

void NetworkManagerBackend::managerPropertiesChanged(const QString &interfaceName,
                                                     const QVariantMap &changed,
                                                     const QStringList &invalidated)
{
    if (!m_running || interfaceName != QString::fromLatin1(managerInterface))
        return;
    for (auto it = changed.constBegin(); it != changed.constEnd(); ++it)
        m_managerProperties.insert(it.key(), it.value());
    for (const QString &name : invalidated)
        m_managerProperties.remove(name);
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

void NetworkManagerBackend::deviceRemoved(const QDBusObjectPath &)
{
    probe();
}

void NetworkManagerBackend::rebuildPropertyWatchers()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    for (NetworkPropertyWatcher *watcher : std::as_const(m_propertyWatchers)) {
        bus.disconnect(QString::fromLatin1(serviceName), watcher->path,
                       QString::fromLatin1(propertiesInterface), QStringLiteral("PropertiesChanged"),
                       watcher, nullptr);
        delete watcher;
    }
    m_propertyWatchers.clear();
    if (!m_running)
        return;
    for (auto it = m_deviceProperties.cbegin(); it != m_deviceProperties.cend(); ++it) {
        auto *watcher = new NetworkPropertyWatcher(it.key(), this);
        connect(watcher, &NetworkPropertyWatcher::propertiesUpdated, this,
                [this](const QString &path, const QString &interfaceName,
                       const QVariantMap &changed, const QStringList &invalidated) {
            if (!m_running || interfaceName != QString::fromLatin1(deviceInterface))
                return;
            QVariantMap &properties = m_deviceProperties[path];
            for (auto it = changed.constBegin(); it != changed.constEnd(); ++it)
                properties.insert(it.key(), it.value());
            for (const QString &name : invalidated)
                properties.remove(name);
            refreshDevices();
        });
        bus.connect(QString::fromLatin1(serviceName), watcher->path,
                    QString::fromLatin1(propertiesInterface), QStringLiteral("PropertiesChanged"),
                    watcher, SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
        m_propertyWatchers.append(watcher);
    }
}

void NetworkManagerBackend::publishUnavailable()
{
    ++m_generation;
    m_managerProperties.clear();
    m_deviceProperties.clear();
    m_wifiDevicePath.clear();
    m_activeAccessPointPath.clear();
    m_scanQueued = false;
    m_scanInFlight = false;
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
