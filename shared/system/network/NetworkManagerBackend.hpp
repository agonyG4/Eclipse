#pragma once

#include "system/network/NetworkBackend.hpp"
#include "system/network/NetworkManagerState.hpp"

#include <QDBusObjectPath>
#include <QMap>
#include <QObject>
#include <QElapsedTimer>
#include <functional>
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
    bool setWifiEnabled(bool enabled, quint64 requestId) override;
    bool requestWifiScan() override;
    bool applyDns(NetworkDnsMode mode, const QStringList &servers,
                  quint64 requestId, QString *errorOut) override;

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
    void refreshPrimaryRuntimeDetails(bool refreshWired = true, bool refreshIpv4 = true,
                                     bool refreshIpv6 = true);
    void refreshWiredProperties(const QString &devicePath);
    void refreshPrimaryIpConfig(bool ipv4);
    void refreshPrimaryConnectionProfile();
    void refreshProfileVersion(const QString &profilePath,
                              std::function<void(bool, quint64)> completion);
    void readCoherentDnsProfile(quint64 daemonGeneration, quint64 refreshGeneration,
                                quint64 primaryEpoch, quint64 profileRequestId,
                                const QString &activeConnectionPath,
                                const QString &profilePath, int attempt,
                                std::function<void(bool, NetworkConnectionSettings, quint64)> completion,
                                bool requirePrimaryIdentity = true);
    void beginDnsOperation(NetworkDnsMode mode, const QStringList &servers,
                           quint64 requestId);
    void continueDnsOperation(quint64 daemonGeneration, quint64 refreshGeneration,
                              quint64 primaryEpoch, quint64 requestId,
                              const QString &activeConnectionPath, const QString &profilePath,
                              const QString &devicePath, NetworkDnsMode mode,
                              const QStringList &servers, int attempt);
    void persistDnsOperation(quint64 daemonGeneration, quint64 refreshGeneration,
                             quint64 primaryEpoch, quint64 requestId,
                             const QString &activeConnectionPath, const QString &profilePath,
                             const QString &devicePath, NetworkConnectionSettings settings,
                             NetworkDnsIntent intent);
    void verifyDnsPersistence(quint64 daemonGeneration, quint64 refreshGeneration,
                              quint64 primaryEpoch, quint64 requestId,
                              const QString &activeConnectionPath, const QString &profilePath,
                              const QString &devicePath, NetworkDnsIntent intent);
    void reapplyDnsOperation(quint64 daemonGeneration, quint64 refreshGeneration,
                             quint64 primaryEpoch, quint64 requestId,
                             const QString &activeConnectionPath, const QString &profilePath,
                             const QString &devicePath, NetworkDnsIntent intent);
    bool dnsRequestCurrent(quint64 daemonGeneration, quint64 requestId) const;
    bool dnsIdentityCurrent(quint64 daemonGeneration, quint64 refreshGeneration,
                            quint64 primaryEpoch, quint64 requestId,
                            const QString &activeConnectionPath, const QString &profilePath,
                            const QString &devicePath) const;
    void finishDnsOperation(quint64 daemonGeneration, quint64 requestId,
                            NetworkDnsOutcome outcome, const QString &error = {});
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
    quint64 m_scanOperationId = 0;
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
    quint64 m_wiredPropertiesRequestId = 0;
    quint64 m_ipv4ConfigRequestId = 0;
    quint64 m_ipv6ConfigRequestId = 0;
    quint64 m_profileRequestId = 0;
    quint64 m_dnsRequestId = 0;
    int m_pendingAccessPointProperties = 0;
    quint64 m_accessPointRefreshGeneration = 0;
    NetworkWirelessRefreshState m_wirelessRefresh;
};

} // namespace Astrea::System
