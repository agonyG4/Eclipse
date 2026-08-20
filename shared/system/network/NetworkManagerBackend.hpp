#pragma once

#include "system/network/NetworkBackend.hpp"
#include "system/network/NetworkManagerState.hpp"

#include <QDBusObjectPath>
#include <QMap>
#include <QObject>
#include <QElapsedTimer>
#include <QSet>
#include <QVector>

class QDBusServiceWatcher;
class QTimer;

namespace Astrea::System {

class NetworkPropertyWatcher;

class NetworkManagerBackend final : public QObject, public NetworkBackend {
    Q_OBJECT

public:
    explicit NetworkManagerBackend(QObject *parent = nullptr);
    ~NetworkManagerBackend() override;

    bool start(const Callbacks &callbacks, QString *errorOut) override;
    void stop() override;
    bool setWifiEnabled(bool enabled) override;
    bool requestWifiScan() override;

private slots:
    void managerPropertiesChanged(const QString &interfaceName,
                                  const QVariantMap &changed,
                                  const QStringList &invalidated);
    void interfacesAdded(const QDBusObjectPath &path, const QVariantMap &interfaces);
    void interfacesRemoved(const QDBusObjectPath &path, const QStringList &interfaces);
    void deviceAdded(const QDBusObjectPath &path);
    void deviceRemoved(const QDBusObjectPath &path);

private:
    void probe();
    void refreshDevices();
    void refreshActiveConnection(const QString &objectPath);
    void refreshAccessPoints();
    void refreshAccessPoint(const QString &objectPath);
    void publishUnavailable();
    void publishFromProperties(const QVariantMap &properties);
    void rebuildPropertyWatchers();
    void reconcileDevices(quint64 generation);
    void finishDeviceReconciliation(quint64 generation);
    bool reconcileSelectedWifiDevice();
    void publishReconciledSnapshot();
    void sampleTraffic();

    Callbacks m_callbacks;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    QVector<NetworkPropertyWatcher *> m_propertyWatchers;
    QVariantMap m_managerProperties;
    QMap<QString, QVariantMap> m_deviceProperties;
    NetworkManagerState m_state;
    NetworkScanState m_scanState;
    quint64 m_generation = 0;
    quint64 m_refreshGeneration = 0;
    int m_pendingDeviceProperties = 0;
    QString m_wifiDevicePath;
    QString m_activeAccessPointPath;
    bool m_running = false;
    QTimer *m_scanCooldownTimer = nullptr;
    QElapsedTimer m_scanClock;
    QTimer *m_trafficTimer = nullptr;
    NetworkSnapshot m_snapshot;
    QElapsedTimer m_trafficClock;
    quint64 m_previousRx = 0;
    quint64 m_previousTx = 0;
    bool m_haveTrafficSample = false;
    QString m_trafficInterface;
    QString m_activeConnectionPath;
    quint64 m_primaryEpoch = 0;
    int m_pendingAccessPointProperties = 0;
    quint64 m_accessPointRefreshGeneration = 0;
    NetworkWirelessRefreshState m_wirelessRefresh;
};

} // namespace Astrea::System
