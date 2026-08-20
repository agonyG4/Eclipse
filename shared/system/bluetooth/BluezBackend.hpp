#pragma once

#include "system/bluetooth/BluetoothBackend.hpp"

#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QMap>
#include <QObject>
#include <QVariant>
#include <QVector>

#include <memory>

class QDBusServiceWatcher;

namespace Astrea::System {

class BluezPropertyWatcher;
class BluezObjectStore;

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
    bool startDiscovery(quint64 requestId) override;
    bool stopDiscovery(quint64 requestId) override;
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
    void refreshInvalidatedProperties(const QString &objectPath, const QString &interfaceName,
                                      quint64 generation, quint64 interfaceRevision);
    void rebuildPropertyWatchers();
    void publishSnapshot();
    void callDeviceMethod(const QString &objectPath, const QString &method);
    bool callAdapterMethod(const QString &method, quint64 requestId);
    void finishOperation(BluetoothOperationKind kind, quint64 requestId,
                         bool success, const QString &error = {});
    void clearGeneration();

    Callbacks m_callbacks;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    QVector<BluezPropertyWatcher *> m_propertyWatchers;
    BluezManagedObjects m_objects;
    std::unique_ptr<BluezObjectStore> m_objectStore;
    QString m_adapterPath;
    bool m_running = false;
    quint64 m_generation = 0;
};

} // namespace Astrea::System

Q_DECLARE_METATYPE(Astrea::System::BluezDBusInterfaces)
Q_DECLARE_METATYPE(Astrea::System::BluezManagedObjects)
