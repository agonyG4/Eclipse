#pragma once

#include "system/SystemServiceState.hpp"

#include <QMap>
#include <QDBusArgument>
#include <QStringList>
#include <QVariantMap>

namespace Astrea::System {

// NetworkManager's org.freedesktop.NetworkManager.Settings.Connection settings
// argument has signature a{sa{sv}}. Each setting remains a typed QVariant map.
struct NetworkConnectionSettings {
    QMap<QString, QVariantMap> sections;
};

struct NetworkDnsProjection {
    NetworkDnsMode mode = NetworkDnsMode::Advanced;
    QStringList configuredIpv4DnsServers;
    QStringList configuredIpv6DnsServers;
    bool dnsEditable = false;
};

struct NetworkDnsValidation {
    bool valid = false;
    QStringList servers;
    QStringList ipv4Servers;
    QStringList ipv6Servers;
    QString error;
};

struct NetworkDnsPatchResult {
    bool valid = false;
    NetworkConnectionSettings settings;
    QString error;
};

struct NetworkDnsOperationIdentity {
    bool running = false;
    quint64 daemonGeneration = 0;
    quint64 primaryEpoch = 0;
    quint64 requestId = 0;
    QString activeConnectionPath;
    QString profilePath;
    QString devicePath;
};

enum class NetworkDnsProfileReadDecision {
    Proceed,
    Retry,
    Conflict,
    Failed,
};

NetworkDnsProjection projectDns(const NetworkConnectionSettings &settings);
NetworkDnsValidation canonicalizeDnsServers(const QStringList &servers);
NetworkDnsPatchResult patchDnsSettings(const NetworkConnectionSettings &settings,
                                       NetworkDnsMode mode,
                                       const QStringList &servers);
NetworkDnsProfileReadDecision dnsProfileReadDecision(bool firstVersionAvailable,
                                                     bool settingsAvailable,
                                                     bool secondVersionAvailable,
                                                     quint64 firstVersion,
                                                     quint64 secondVersion,
                                                     int attempt,
                                                     int maximumAttempts = 3);
NetworkDnsOutcome dnsUpdateErrorOutcome(const QString &errorName);
bool sameNetworkDnsOperationIdentity(const NetworkDnsOperationIdentity &captured,
                                     const NetworkDnsOperationIdentity &current);

} // namespace Astrea::System

Q_DECLARE_METATYPE(Astrea::System::NetworkConnectionSettings)

namespace Astrea::System {
QDBusArgument &operator<<(QDBusArgument &argument,
                          const NetworkConnectionSettings &settings);
const QDBusArgument &operator>>(const QDBusArgument &argument,
                                NetworkConnectionSettings &settings);
}
