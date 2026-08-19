#pragma once

#include "system/bluetooth/BluetoothBackend.hpp"

#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QMap>
#include <QObject>
#include <QVariant>
#include <QVector>

class QDBusServiceWatcher;

namespace Astrea::System {

class BluezPropertyWatcher;

using BluezDBusProperties = QMap<QString, QVariant>;
using BluezDBusInterfaces = QMap<QString, BluezDBusProperties>;
using BluezManagedObjects = QMap<QDBusObjectPath, BluezDBusInterfaces>;

class BluezBackend final : public QObject, public BluetoothBackend {
    Q_OBJECT

public:
    explicit BluezBackend(QObject *parent = nullptr);
    ~BluezBackend() override;

    bool start(const Callbacks &callbacks, QString *errorOut) override;
    void stop() override;
    bool setPowered(bool powered) override;
    bool startDiscovery() override;
    bool stopDiscovery() override;
    bool connectDevice(const QString &objectPath) override;
    bool disconnectDevice(const QString &objectPath) override;

private slots:
    void interfacesAdded(const QDBusObjectPath &path, const BluezDBusInterfaces &interfaces);
    void interfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces);

private:
    void probe();
    void publishUnavailable();
    void publishManagedObjects(const QDBusArgument &objects);
    void handlePropertiesChanged(const QString &objectPath, const QString &interfaceName,
                                 const QVariantMap &changed, const QStringList &invalidated);
    void rebuildPropertyWatchers();
    void publishSnapshot();
    void callDeviceMethod(const QString &objectPath, const QString &method);
    bool callAdapterMethod(const QString &method);
    void finishOperation(const QString &operation, bool success, const QString &error = {});
    void clearGeneration();

    Callbacks m_callbacks;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    QVector<BluezPropertyWatcher *> m_propertyWatchers;
    BluezManagedObjects m_objects;
    QString m_adapterPath;
    bool m_running = false;
    quint64 m_generation = 0;
};

} // namespace Astrea::System

Q_DECLARE_METATYPE(Astrea::System::BluezDBusInterfaces)
Q_DECLARE_METATYPE(Astrea::System::BluezManagedObjects)
