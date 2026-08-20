#pragma once

#include "system/network/NetworkBackend.hpp"

#include <QMap>
#include <QSet>
#include <QVariantMap>

namespace Astrea::System {

struct NetworkDeviceState {
    QString objectPath;
    QVariantMap deviceProperties;
    QVariantMap wirelessProperties;
    bool wirelessInterfacePresent = false;
};

struct NetworkAccessPointState {
    QString objectPath;
    QVariantMap properties;
};

class NetworkManagerState final {
public:
    void reset();
    void setManagerProperties(QVariantMap properties);
    void updateManagerProperties(const QVariantMap &changed, const QStringList &invalidated);
    void upsertDevice(const QString &path, QVariantMap properties);
    void setWirelessProperties(const QString &path, QVariantMap properties);
    void updateDeviceProperties(const QString &path, const QString &interfaceName,
                                const QVariantMap &changed, const QStringList &invalidated);
    void removeDevice(const QString &path);
    void upsertAccessPoint(const QString &path, QVariantMap properties);
    void clearAccessPoints();
    void updateAccessPoint(const QString &path, const QVariantMap &changed,
                           const QStringList &invalidated);
    void removeAccessPoint(const QString &path);
    void setPrimaryProperties(const QString &path, QVariantMap properties);
    quint64 beginPrimaryRequest(const QString &path);
    bool applyPrimaryReply(const QString &path, quint64 epoch, QVariantMap properties);
    void updatePrimaryProperties(const QVariantMap &changed, const QStringList &invalidated);
    void clearPrimary();

    const QMap<QString, NetworkDeviceState> &devices() const { return m_devices; }
    const QMap<QString, NetworkAccessPointState> &accessPoints() const { return m_accessPoints; }
    const QString &primaryPath() const { return m_primaryPath; }
    quint64 primaryEpoch() const { return m_primaryEpoch; }
    QString selectedWifiDevicePath() const;
    QString activeAccessPointPath() const;
    NetworkSnapshot snapshot() const;

private:
    static void applyProperties(QVariantMap &target, const QVariantMap &changed,
                                const QStringList &invalidated);
    QString physicalDeviceForPrimary() const;
    static QString objectPath(const QVariant &value);
    static QVector<QString> objectPaths(const QVariant &value);

    QVariantMap m_managerProperties;
    QMap<QString, NetworkDeviceState> m_devices;
    QMap<QString, NetworkAccessPointState> m_accessPoints;
    QString m_primaryPath;
    QVariantMap m_primaryProperties;
    quint64 m_primaryEpoch = 0;
};

class NetworkWirelessRefreshState final {
public:
    void begin(quint64 daemonGeneration, quint64 refreshGeneration,
               const QStringList &devicePaths);
    bool acceptReply(quint64 daemonGeneration, quint64 refreshGeneration,
                     const QString &devicePath);
    bool complete() const { return m_pendingDevicePaths.isEmpty(); }
    int remaining() const { return m_pendingDevicePaths.size(); }

private:
    quint64 m_daemonGeneration = 0;
    quint64 m_refreshGeneration = 0;
    QSet<QString> m_pendingDevicePaths;
};

enum class NetworkScanPhase {
    Idle,
    RequestPending,
    WaitingForLastScan,
    Cooldown,
};

class NetworkScanState final {
public:
    bool request(quint64 generation, const QString &devicePath, qint64 lastScan,
                 qint64 nowMs);
    bool requestFinished(quint64 generation, const QString &devicePath,
                         bool success, qint64 nowMs);
    bool lastScanAdvanced(quint64 generation, const QString &devicePath, qint64 lastScan,
                          qint64 nowMs);
    bool cooldownExpired(qint64 nowMs);
    void timeout(qint64 nowMs);
    void invalidate();

    NetworkScanPhase phase() const { return m_phase; }
    bool active() const { return m_phase == NetworkScanPhase::RequestPending
        || m_phase == NetworkScanPhase::WaitingForLastScan; }
    bool queuedDemand() const { return m_queuedDemand; }
    quint64 generation() const { return m_generation; }
    const QString &devicePath() const { return m_devicePath; }
    qint64 baselineLastScan() const { return m_baselineLastScan; }
    qint64 cooldownUntilMs() const { return m_cooldownUntilMs; }

private:
    NetworkScanPhase m_phase = NetworkScanPhase::Idle;
    bool m_queuedDemand = false;
    quint64 m_generation = 0;
    QString m_devicePath;
    qint64 m_baselineLastScan = -1;
    qint64 m_cooldownUntilMs = -1;
};

} // namespace Astrea::System
