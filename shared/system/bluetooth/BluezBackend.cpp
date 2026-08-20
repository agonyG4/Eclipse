#include "system/bluetooth/BluezBackend.hpp"

#include "system/bluetooth/BluezObjectStore.hpp"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QMetaType>
#include <QSet>

#include <algorithm>

namespace Astrea::System {

class BluezPropertyWatcher final : public QObject {
    Q_OBJECT

public:
    BluezPropertyWatcher(QString objectPath, QObject *parent)
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

constexpr auto serviceName = "org.bluez";
constexpr auto rootPath = "/";
constexpr auto objectManagerInterface = "org.freedesktop.DBus.ObjectManager";
constexpr auto adapterInterface = "org.bluez.Adapter1";
constexpr auto deviceInterface = "org.bluez.Device1";
constexpr auto batteryInterface = "org.bluez.Battery1";
constexpr auto propertiesInterface = "org.freedesktop.DBus.Properties";

QString propertyString(const BluezDBusProperties &properties, const QString &name,
                       const QString &fallback = {})
{
    const QString value = properties.value(name).toString();
    return value.isEmpty() ? fallback : value;
}

bool propertyBool(const BluezDBusProperties &properties, const QString &name)
{
    return properties.value(name).toBool();
}

} // namespace

BluezBackend::BluezBackend(QObject *parent)
    : QObject(parent)
    , m_objectStore(std::make_unique<BluezObjectStore>())
{
    qRegisterMetaType<BluezDBusInterfaces>("Astrea::System::BluezDBusInterfaces");
    qRegisterMetaType<BluezManagedObjects>("Astrea::System::BluezManagedObjects");
    qDBusRegisterMetaType<BluezDBusProperties>();
    qDBusRegisterMetaType<BluezDBusInterfaces>();
    qDBusRegisterMetaType<BluezManagedObjects>();
}

BluezBackend::~BluezBackend()
{
    stop();
}

bool BluezBackend::start(const Callbacks &callbacks, QString *errorOut)
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
    m_objects.clear();
    m_objectStore->clear();
    m_adapterPath.clear();
    m_serviceWatcher = new QDBusServiceWatcher(QString::fromLatin1(serviceName), bus,
                                               QDBusServiceWatcher::WatchForRegistration
                                                   | QDBusServiceWatcher::WatchForUnregistration,
                                               this);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceRegistered,
            this, [this](const QString &) {
                if (!m_running)
                    return;
                ++m_generation;
                m_objects.clear();
                m_adapterPath.clear();
                rebuildPropertyWatchers();
                probe();
            });
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, [this](const QString &) {
                if (!m_running)
                    return;
                clearGeneration();
                publishUnavailable();
            });

    bus.connect(QString::fromLatin1(serviceName), QString::fromLatin1(rootPath),
                QString::fromLatin1(objectManagerInterface), QStringLiteral("InterfacesAdded"),
                this, SLOT(interfacesAdded(QDBusObjectPath,BluezDBusInterfaces)));
    bus.connect(QString::fromLatin1(serviceName), QString::fromLatin1(rootPath),
                QString::fromLatin1(objectManagerInterface), QStringLiteral("InterfacesRemoved"),
                this, SLOT(interfacesRemoved(QDBusObjectPath,QStringList)));
    probe();
    return true;
}

void BluezBackend::stop()
{
    if (!m_running && !m_serviceWatcher)
        return;

    m_running = false;
    ++m_generation;
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.disconnect(QString::fromLatin1(serviceName), QString::fromLatin1(rootPath),
                   QString::fromLatin1(objectManagerInterface), QStringLiteral("InterfacesAdded"),
                   this, nullptr);
    bus.disconnect(QString::fromLatin1(serviceName), QString::fromLatin1(rootPath),
                   QString::fromLatin1(objectManagerInterface), QStringLiteral("InterfacesRemoved"),
                   this, nullptr);
    rebuildPropertyWatchers();
    m_objects.clear();
    m_objectStore->clear();
    m_adapterPath.clear();
    delete m_serviceWatcher;
    m_serviceWatcher = nullptr;
    m_callbacks = {};
}

bool BluezBackend::setPowered(bool powered)
{
    if (!m_running || m_adapterPath.isEmpty())
        return false;

    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), m_adapterPath,
        QString::fromLatin1(propertiesInterface), QStringLiteral("Set"));
    message << QString::fromLatin1(adapterInterface) << QStringLiteral("Powered")
            << QVariant::fromValue(QDBusVariant(QVariant(powered)));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message),
                                                 this);
    const quint64 generation = m_generation;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher *finished) {
        if (generation == m_generation && m_running) {
            const QDBusMessage reply = finished->reply();
            if (reply.type() == QDBusMessage::ErrorMessage)
                finishOperation(BluetoothOperationKind::Power, 0, false, reply.errorMessage());
            else
                finishOperation(BluetoothOperationKind::Power, 0, true);
        }
        finished->deleteLater();
    });
    return true;
}

bool BluezBackend::startDiscovery(quint64 requestId)
{
    return callAdapterMethod(QStringLiteral("StartDiscovery"), requestId);
}

bool BluezBackend::stopDiscovery(quint64 requestId)
{
    return callAdapterMethod(QStringLiteral("StopDiscovery"), requestId);
}

bool BluezBackend::connectDevice(const QString &objectPath)
{
    if (!m_running || objectPath.isEmpty())
        return false;
    callDeviceMethod(objectPath, QStringLiteral("Connect"));
    return true;
}

bool BluezBackend::disconnectDevice(const QString &objectPath)
{
    if (!m_running || objectPath.isEmpty())
        return false;
    callDeviceMethod(objectPath, QStringLiteral("Disconnect"));
    return true;
}

void BluezBackend::interfacesAdded(const QDBusObjectPath &path,
                                   const BluezDBusInterfaces &interfaces)
{
    if (!m_running)
        return;
    m_objectStore->interfacesAdded(path, interfaces);
    m_objects = m_objectStore->objects();
    rebuildPropertyWatchers();
    publishSnapshot();
}

void BluezBackend::interfacesRemoved(const QDBusObjectPath &path,
                                     const QStringList &interfaces)
{
    if (!m_running)
        return;
    m_objectStore->interfacesRemoved(path, interfaces);
    m_objects = m_objectStore->objects();
    rebuildPropertyWatchers();
    publishSnapshot();
}

void BluezBackend::probe()
{
    if (!m_running)
        return;

    const quint64 generation = m_generation;
    const QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), QString::fromLatin1(rootPath),
        QString::fromLatin1(objectManagerInterface), QStringLiteral("GetManagedObjects"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message),
                                                 this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation](QDBusPendingCallWatcher *finished) {
        const QDBusMessage reply = finished->reply();
        if (generation != m_generation || !m_running) {
            finished->deleteLater();
            return;
        }
        if (reply.type() == QDBusMessage::ErrorMessage) {
            if (m_callbacks.errorChanged)
                m_callbacks.errorChanged(reply.errorMessage());
            publishUnavailable();
        } else if (!reply.arguments().isEmpty()) {
            const QDBusArgument argument = reply.arguments().constFirst().value<QDBusArgument>();
            publishManagedObjects(argument);
        }
        finished->deleteLater();
    });
}

void BluezBackend::publishManagedObjects(const QDBusArgument &objects)
{
    BluezManagedObjects parsed;
    objects >> parsed;
    m_objectStore->replace(std::move(parsed));
    m_objects = m_objectStore->objects();
    rebuildPropertyWatchers();
    publishSnapshot();
}

void BluezBackend::handlePropertiesChanged(const QString &objectPath,
                                           const QString &interfaceName,
                                           const QVariantMap &changed,
                                           const QStringList &invalidated)
{
    if (!m_running)
        return;
    const QDBusObjectPath path(objectPath);
    const bool changedApplied = m_objectStore->propertiesChanged(path, interfaceName, changed);
    if (!changedApplied)
        return;
    const quint64 interfaceRevision = m_objectStore->interfaceRevision(path, interfaceName);
    m_objects = m_objectStore->objects();
    publishSnapshot();
    if (!invalidated.isEmpty())
        refreshInvalidatedProperties(objectPath, interfaceName, m_generation, interfaceRevision);
}

void BluezBackend::refreshInvalidatedProperties(const QString &objectPath,
                                                const QString &interfaceName,
                                                quint64 generation,
                                                quint64 interfaceRevision)
{
    if (!m_running || generation != m_generation)
        return;
    QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), objectPath,
        QString::fromLatin1(propertiesInterface), QStringLiteral("GetAll"));
    message << interfaceName;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message),
                                                 this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation, objectPath, interfaceName,
             interfaceRevision](QDBusPendingCallWatcher *finished) {
        if (generation == m_generation && m_running) {
            const QDBusMessage reply = finished->reply();
            if (reply.type() != QDBusMessage::ErrorMessage && !reply.arguments().isEmpty()) {
                if (m_objectStore->replaceInterfaceIfRevision(
                        QDBusObjectPath(objectPath), interfaceName, interfaceRevision,
                        reply.arguments().constFirst().toMap())) {
                    m_objects = m_objectStore->objects();
                    publishSnapshot();
                }
            } else if (reply.type() == QDBusMessage::ErrorMessage && m_callbacks.errorChanged) {
                m_callbacks.errorChanged(reply.errorMessage());
            }
        }
        finished->deleteLater();
    });
}

void BluezBackend::rebuildPropertyWatchers()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    for (BluezPropertyWatcher *watcher : std::as_const(m_propertyWatchers)) {
        bus.disconnect(QString::fromLatin1(serviceName), watcher->path,
                       QString::fromLatin1(propertiesInterface), QStringLiteral("PropertiesChanged"),
                       watcher, nullptr);
        delete watcher;
    }
    m_propertyWatchers.clear();
    if (!m_running)
        return;

    for (auto object = m_objects.constBegin(); object != m_objects.constEnd(); ++object) {
        if (object->isEmpty())
            continue;
        auto *watcher = new BluezPropertyWatcher(object.key().path(), this);
        connect(watcher, &BluezPropertyWatcher::propertiesUpdated, this,
                [this](const QString &path, const QString &interfaceName,
                       const QVariantMap &changed, const QStringList &invalidated) {
            handlePropertiesChanged(path, interfaceName, changed, invalidated);
        });
        bus.connect(QString::fromLatin1(serviceName), watcher->path,
                    QString::fromLatin1(propertiesInterface), QStringLiteral("PropertiesChanged"),
                    watcher, SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
        m_propertyWatchers.append(watcher);
    }
}

void BluezBackend::publishSnapshot()
{
    if (!m_running || !m_callbacks.snapshotChanged)
        return;

    struct Adapter {
        QString path;
        QString name;
        bool powered = false;
        bool discovering = false;
    };
    QList<Adapter> adapters;
    for (auto it = m_objects.constBegin(); it != m_objects.constEnd(); ++it) {
        const BluezDBusProperties properties =
            it.value().value(QString::fromLatin1(adapterInterface));
        if (properties.isEmpty())
            continue;
        adapters.append({it.key().path(),
                         propertyString(properties, QStringLiteral("Alias"),
                                        propertyString(properties, QStringLiteral("Name"))),
                         propertyBool(properties, QStringLiteral("Powered")),
                         propertyBool(properties, QStringLiteral("Discovering"))});
    }
    std::sort(adapters.begin(), adapters.end(), [](const Adapter &left, const Adapter &right) {
        return left.path < right.path;
    });
    auto selected = std::find_if(adapters.cbegin(), adapters.cend(), [this](const Adapter &adapter) {
        return adapter.path == m_adapterPath;
    });
    if (selected == adapters.cend())
        selected = std::find_if(adapters.cbegin(), adapters.cend(),
                                [](const Adapter &adapter) { return adapter.powered; });
    if (selected == adapters.cend() && !adapters.isEmpty())
        selected = adapters.cbegin();
    m_adapterPath = selected == adapters.cend() ? QString() : selected->path;

    QVector<BluetoothDevice> devices;
    for (auto it = m_objects.constBegin(); it != m_objects.constEnd(); ++it) {
        if (m_adapterPath.isEmpty() || !it.key().path().startsWith(m_adapterPath + QLatin1Char('/')))
            continue;
        const BluezDBusProperties properties = it.value().value(QString::fromLatin1(deviceInterface));
        if (properties.isEmpty())
            continue;
        BluetoothDevice device;
        device.id = it.key().path();
        device.objectPath = it.key().path();
        device.address = propertyString(properties, QStringLiteral("Address"));
        device.name = propertyString(properties, QStringLiteral("Alias"),
                                     propertyString(properties, QStringLiteral("Name")));
        device.paired = propertyBool(properties, QStringLiteral("Paired"));
        device.trusted = propertyBool(properties, QStringLiteral("Trusted"));
        device.connected = propertyBool(properties, QStringLiteral("Connected"));
        device.discovered = true;
        device.icon = propertyString(properties, QStringLiteral("Icon"));
        device.rssi = properties.contains(QStringLiteral("RSSI"))
            ? properties.value(QStringLiteral("RSSI")).toInt() : -1;
        const BluezDBusProperties battery =
            it.value().value(QString::fromLatin1(batteryInterface));
        device.batteryPercent = battery.value(QStringLiteral("Percentage"), -1).toInt();
        devices.append(std::move(device));
    }

    BluetoothSnapshot snapshot;
    snapshot.daemonAvailable = true;
    snapshot.adapterAvailable = selected != adapters.cend();
    if (snapshot.adapterAvailable) {
        snapshot.adapterPath = selected->path;
        snapshot.adapterName = selected->name;
        snapshot.powered = selected->powered;
        snapshot.scanning = selected->discovering;
    }
    snapshot.devices = std::move(devices);
    m_callbacks.snapshotChanged(std::move(snapshot));
}

void BluezBackend::publishUnavailable()
{
    m_objects.clear();
    m_objectStore->clear();
    m_adapterPath.clear();
    rebuildPropertyWatchers();
    if (!m_callbacks.snapshotChanged)
        return;
    BluetoothSnapshot snapshot;
    snapshot.daemonAvailable = false;
    m_callbacks.snapshotChanged(std::move(snapshot));
}

void BluezBackend::callDeviceMethod(const QString &objectPath, const QString &method)
{
    const QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), objectPath,
        QString::fromLatin1(deviceInterface), method);
    const quint64 generation = m_generation;
    const BluetoothOperationKind kind = method.compare(QStringLiteral("Connect"),
                                                       Qt::CaseInsensitive) == 0
        ? BluetoothOperationKind::Connect : BluetoothOperationKind::Disconnect;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message),
                                                 this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation, kind](QDBusPendingCallWatcher *finished) {
        if (generation == m_generation && m_running) {
            const QDBusMessage reply = finished->reply();
            if (reply.type() == QDBusMessage::ErrorMessage) {
                if (m_callbacks.errorChanged)
                    m_callbacks.errorChanged(reply.errorMessage());
                finishOperation(kind, 0, false, reply.errorMessage());
            } else {
                finishOperation(kind, 0, true);
            }
        }
        finished->deleteLater();
    });
}

bool BluezBackend::callAdapterMethod(const QString &method, quint64 requestId)
{
    if (!m_running || m_adapterPath.isEmpty())
        return false;
    const QDBusMessage message = QDBusMessage::createMethodCall(
        QString::fromLatin1(serviceName), m_adapterPath,
        QString::fromLatin1(adapterInterface), method);
    const quint64 generation = m_generation;
    const BluetoothOperationKind kind = method.compare(QStringLiteral("StartDiscovery"),
                                                       Qt::CaseInsensitive) == 0
        ? BluetoothOperationKind::StartDiscovery : BluetoothOperationKind::StopDiscovery;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message),
                                                 this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, generation, kind, requestId](QDBusPendingCallWatcher *finished) {
        if (generation == m_generation && m_running) {
            const QDBusMessage reply = finished->reply();
            if (reply.type() == QDBusMessage::ErrorMessage) {
                if (m_callbacks.errorChanged)
                    m_callbacks.errorChanged(reply.errorMessage());
                finishOperation(kind, requestId, false, reply.errorMessage());
            } else {
                finishOperation(kind, requestId, true);
            }
        }
        finished->deleteLater();
    });
    return true;
}

void BluezBackend::finishOperation(BluetoothOperationKind kind, quint64 requestId,
                                   bool success, const QString &error)
{
    if (error.isEmpty() == false && m_callbacks.errorChanged)
        m_callbacks.errorChanged(error);
    if (m_callbacks.operationFinished)
        m_callbacks.operationFinished({kind, requestId, success, error});
}

void BluezBackend::clearGeneration()
{
    ++m_generation;
    m_objects.clear();
    m_objectStore->clear();
    m_adapterPath.clear();
    rebuildPropertyWatchers();
}

} // namespace Astrea::System

#include "BluezBackend.moc"
