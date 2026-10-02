#include "system/network/NetworkDns.hpp"

#include <QDBusArgument>
#include <QDBusVariant>
#include <QHostAddress>
#include <QSet>

namespace Astrea::System {
namespace {

QVariant unwrap(QVariant value)
{
    while (value.metaType() == QMetaType::fromType<QDBusVariant>())
        value = value.value<QDBusVariant>().variant();
    return value;
}

bool isDnsCapableMethod(const QString &method)
{
    return method == QLatin1String("auto") || method == QLatin1String("manual")
        || method == QLatin1String("dhcp");
}

bool isKnownNonDnsMethod(const QString &method)
{
    return method == QLatin1String("disabled") || method == QLatin1String("ignore")
        || method == QLatin1String("link-local") || method == QLatin1String("shared");
}

struct FamilyDns {
    bool capable = false;
    bool knownMethod = false;
    bool hasDnsData = false;
    bool validDnsData = true;
    bool ignoreAutomatic = false;
    bool validIgnoreAutomatic = true;
    QStringList servers;
    bool hasLegacyDns = false;
};

bool decodeDnsData(const QVariant &value, QStringList *servers)
{
    const QVariant unwrapped = unwrap(value);
    if (unwrapped.metaType() == QMetaType::fromType<QStringList>()) {
        *servers = unwrapped.toStringList();
        return true;
    }
    if (unwrapped.metaType() == QMetaType::fromType<QVariantList>()) {
        for (const QVariant &item : unwrapped.toList()) {
            const QVariant server = unwrap(item);
            if (server.metaType() != QMetaType::fromType<QString>())
                return false;
            servers->append(server.toString());
        }
        return true;
    }
    if (unwrapped.metaType() == QMetaType::fromType<QDBusArgument>()) {
        QDBusArgument argument = unwrapped.value<QDBusArgument>();
        if (argument.currentType() != QDBusArgument::ArrayType)
            return false;
        argument.beginArray();
        while (!argument.atEnd()) {
            QString server;
            argument >> server;
            servers->append(server);
        }
        argument.endArray();
        return true;
    }
    return false;
}

FamilyDns inspectFamily(const QVariantMap &family, QAbstractSocket::NetworkLayerProtocol protocol)
{
    FamilyDns result;
    const QString method = unwrap(family.value(QStringLiteral("method"))).toString();
    result.capable = isDnsCapableMethod(method);
    result.knownMethod = result.capable || isKnownNonDnsMethod(method);

    const QString dnsDataKey = QStringLiteral("dns-data");
    result.hasDnsData = family.contains(dnsDataKey);
    if (result.hasDnsData) {
        QStringList decoded;
        result.validDnsData = decodeDnsData(family.value(dnsDataKey), &decoded);
        if (result.validDnsData) {
            const NetworkDnsValidation canonical = canonicalizeDnsServers(decoded);
            if (!canonical.valid || canonical.servers.size() != decoded.size()) {
                result.validDnsData = false;
            } else {
                for (const QString &server : canonical.servers) {
                    QHostAddress address;
                    if (!address.setAddress(server) || address.protocol() != protocol) {
                        result.validDnsData = false;
                        break;
                    }
                    result.servers.append(address.toString());
                }
            }
        }
    }

    const QString ignoreKey = QStringLiteral("ignore-auto-dns");
    if (family.contains(ignoreKey)) {
        const QVariant value = unwrap(family.value(ignoreKey));
        if (value.metaType() == QMetaType::fromType<bool>())
            result.ignoreAutomatic = value.toBool();
        else
            result.validIgnoreAutomatic = false;
    }
    result.hasLegacyDns = family.contains(QStringLiteral("dns"));
    return result;
}

QStringList settingServers(const FamilyDns &family)
{
    return family.validDnsData ? family.servers : QStringList{};
}

} // namespace

NetworkDnsProjection projectDns(const NetworkConnectionSettings &settings)
{
    NetworkDnsProjection result;
    const QVariantMap ipv4 = settings.sections.value(QStringLiteral("ipv4"));
    const QVariantMap ipv6 = settings.sections.value(QStringLiteral("ipv6"));
    const FamilyDns v4 = inspectFamily(ipv4, QAbstractSocket::IPv4Protocol);
    const FamilyDns v6 = inspectFamily(ipv6, QAbstractSocket::IPv6Protocol);
    result.configuredIpv4DnsServers = settingServers(v4);
    result.configuredIpv6DnsServers = settingServers(v6);
    result.dnsEditable = v4.knownMethod && v6.knownMethod && v4.validDnsData
        && v6.validDnsData && v4.validIgnoreAutomatic && v6.validIgnoreAutomatic;

    if (!result.dnsEditable || (!v4.capable && !v6.capable))
        return result;

    const bool unsupportedDnsState = (!v4.capable && (v4.hasDnsData || v4.hasLegacyDns
                                        || v4.ignoreAutomatic))
        || (!v6.capable && (v6.hasDnsData || v6.hasLegacyDns || v6.ignoreAutomatic));
    if (unsupportedDnsState || (!v4.hasDnsData && v4.hasLegacyDns)
        || (!v6.hasDnsData && v6.hasLegacyDns))
        return result;

    const bool anyManual = !v4.servers.isEmpty() || !v6.servers.isEmpty();
    const bool allAutomatic = (!v4.capable || !v4.ignoreAutomatic)
        && (!v6.capable || !v6.ignoreAutomatic);
    const bool allCustom = (!v4.capable || v4.ignoreAutomatic)
        && (!v6.capable || v6.ignoreAutomatic);
    if (!anyManual && allAutomatic) {
        result.mode = NetworkDnsMode::Automatic;
    } else if (anyManual && allCustom) {
        result.mode = NetworkDnsMode::Custom;
    }
    return result;
}

NetworkDnsValidation canonicalizeDnsServers(const QStringList &servers)
{
    NetworkDnsValidation result;
    QSet<QString> seen;
    for (const QString &raw : servers) {
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty())
            continue;
        QHostAddress address;
        if (!address.setAddress(trimmed)) {
            result.error = QStringLiteral("Enter literal IPv4 or IPv6 addresses only");
            return result;
        }
        const QString canonical = address.toString();
        if (seen.contains(canonical))
            continue;
        seen.insert(canonical);
        result.servers.append(canonical);
        if (address.protocol() == QAbstractSocket::IPv4Protocol)
            result.ipv4Servers.append(canonical);
        else if (address.protocol() == QAbstractSocket::IPv6Protocol)
            result.ipv6Servers.append(canonical);
        else {
            result.error = QStringLiteral("Enter literal IPv4 or IPv6 addresses only");
            result.servers.clear();
            result.ipv4Servers.clear();
            result.ipv6Servers.clear();
            return result;
        }
    }
    if (result.servers.isEmpty()) {
        result.error = QStringLiteral("Enter at least one DNS server");
        return result;
    }
    result.valid = true;
    return result;
}

NetworkDnsPatchResult patchDnsSettings(const NetworkConnectionSettings &settings,
                                       NetworkDnsMode mode,
                                       const QStringList &servers)
{
    NetworkDnsPatchResult result;
    result.settings = settings;
    if (mode == NetworkDnsMode::Advanced) {
        result.error = QStringLiteral("Choose Automatic or Custom DNS to apply settings");
        return result;
    }

    NetworkDnsValidation canonical;
    if (mode == NetworkDnsMode::Custom) {
        canonical = canonicalizeDnsServers(servers);
        if (!canonical.valid) {
            result.error = canonical.error;
            return result;
        }
    }

    QVariantMap ipv4 = settings.sections.value(QStringLiteral("ipv4"));
    QVariantMap ipv6 = settings.sections.value(QStringLiteral("ipv6"));
    FamilyDns v4 = inspectFamily(ipv4, QAbstractSocket::IPv4Protocol);
    FamilyDns v6 = inspectFamily(ipv6, QAbstractSocket::IPv6Protocol);
    if (!v4.knownMethod || !v6.knownMethod) {
        result.error = QStringLiteral("The connection uses an unsupported IP method");
        return result;
    }
    if (mode == NetworkDnsMode::Custom
        && ((!canonical.ipv4Servers.isEmpty() && !v4.capable)
            || (!canonical.ipv6Servers.isEmpty() && !v6.capable))) {
        result.error = QStringLiteral("A DNS server family is incompatible with this connection's IP method");
        return result;
    }
    if (!v4.capable && !v6.capable) {
        result.error = QStringLiteral("This connection has no IP family that supports DNS servers");
        return result;
    }

    const auto patchFamily = [mode](QVariantMap *family, const FamilyDns &dns,
                                    const QStringList &familyServers) {
        if (!dns.capable)
            return;
        family->insert(QStringLiteral("ignore-auto-dns"), mode == NetworkDnsMode::Custom);
        family->remove(QStringLiteral("dns"));
        if (mode == NetworkDnsMode::Automatic) {
            family->remove(QStringLiteral("dns-data"));
        } else {
            family->insert(QStringLiteral("dns-data"), familyServers);
        }
    };
    patchFamily(&ipv4, v4, canonical.ipv4Servers);
    patchFamily(&ipv6, v6, canonical.ipv6Servers);
    result.settings.sections.insert(QStringLiteral("ipv4"), std::move(ipv4));
    result.settings.sections.insert(QStringLiteral("ipv6"), std::move(ipv6));
    result.valid = true;
    return result;
}

NetworkDnsReapplyPayload prepareDnsReapply(const NetworkConnectionSettings &appliedSettings,
                                           quint64 appliedVersionId,
                                           const NetworkDnsIntent &intent)
{
    NetworkDnsReapplyPayload result;
    if (appliedVersionId == 0) {
        result.error = QStringLiteral("The active connection version is unavailable");
        return result;
    }
    const NetworkDnsPatchResult patch = patchDnsSettings(
        appliedSettings, intent.mode, intent.ipv4Servers + intent.ipv6Servers);
    if (!patch.valid) {
        result.error = patch.error;
        return result;
    }
    result.valid = true;
    result.settings = patch.settings;
    result.appliedVersionId = appliedVersionId;
    return result;
}

QList<QVariant> dnsReapplyArguments(const NetworkDnsReapplyPayload &payload)
{
    return {QVariant::fromValue(payload.settings),
            QVariant::fromValue<qulonglong>(
                static_cast<qulonglong>(payload.appliedVersionId)),
            QVariant::fromValue<quint32>(0)};
}

NetworkDnsAuthorityLossResult dnsAuthorityLossResult(NetworkDnsTransactionPhase phase)
{
    NetworkDnsAuthorityLossResult result;
    switch (phase) {
    case NetworkDnsTransactionPhase::Idle:
        return result;
    case NetworkDnsTransactionPhase::Preparing:
        result.shouldComplete = true;
        result.outcome = NetworkDnsOutcome::Failed;
        result.error = QStringLiteral(
            "NetworkManager became unavailable before DNS settings could be saved.");
        return result;
    case NetworkDnsTransactionPhase::Persisting:
    case NetworkDnsTransactionPhase::VerifyingPersistence:
        result.shouldComplete = true;
        result.outcome = NetworkDnsOutcome::Indeterminate;
        result.error = QStringLiteral(
            "Astrea could not confirm whether the DNS settings were saved. Reload the current connection settings before trying again.");
        return result;
    case NetworkDnsTransactionPhase::ApplyingRuntime:
        result.shouldComplete = true;
        result.outcome = NetworkDnsOutcome::SavedNeedsReconnect;
        return result;
    }
    return result;
}

NetworkDnsTransactionPhase dnsTransactionPhaseAfterTerminalResult()
{
    return NetworkDnsTransactionPhase::Idle;
}

NetworkDnsProfileReadDecision dnsProfileReadDecision(bool firstVersionAvailable,
                                                     bool settingsAvailable,
                                                     bool secondVersionAvailable,
                                                     quint64 firstVersion,
                                                     quint64 secondVersion,
                                                     int attempt,
                                                     int maximumAttempts)
{
    if (!firstVersionAvailable || !settingsAvailable || !secondVersionAvailable
        || firstVersion == 0 || secondVersion == 0 || maximumAttempts <= 0 || attempt < 0)
        return NetworkDnsProfileReadDecision::Failed;
    if (firstVersion == secondVersion)
        return NetworkDnsProfileReadDecision::Proceed;
    return attempt + 1 < maximumAttempts ? NetworkDnsProfileReadDecision::Retry
                                         : NetworkDnsProfileReadDecision::Conflict;
}

NetworkDnsUpdateDisposition dnsUpdateErrorDisposition(const QString &errorName)
{
    if (errorName == QLatin1String(
            "org.freedesktop.NetworkManager.Settings.Connection.Error.VersionIdMismatch"))
        return NetworkDnsUpdateDisposition::Conflict;
    if (errorName.isEmpty()
        || errorName == QLatin1String("org.freedesktop.DBus.Error.NoReply")
        || errorName == QLatin1String("org.freedesktop.DBus.Error.Timeout")
        || errorName == QLatin1String("org.freedesktop.DBus.Error.Disconnected")
        || errorName == QLatin1String("org.freedesktop.DBus.Error.ServiceUnknown")
        || errorName == QLatin1String("org.freedesktop.DBus.Error.NameHasNoOwner")
        || errorName == QLatin1String("org.freedesktop.DBus.Error.NoServer"))
        return NetworkDnsUpdateDisposition::Verify;
    return NetworkDnsUpdateDisposition::Rejected;
}

NetworkDnsPersistenceDecision dnsPersistenceDecision(
    bool profileAvailable, const NetworkDnsProjection &projection,
    quint64 observedVersionId, const NetworkDnsIntent &intent)
{
    if (!profileAvailable || observedVersionId == 0)
        return NetworkDnsPersistenceDecision::Indeterminate;
    if (projection.mode == intent.mode
        && projection.configuredIpv4DnsServers == intent.ipv4Servers
        && projection.configuredIpv6DnsServers == intent.ipv6Servers)
        return NetworkDnsPersistenceDecision::Confirmed;
    if (intent.preCommitProfileVersionId != 0
        && observedVersionId == intent.preCommitProfileVersionId)
        return NetworkDnsPersistenceDecision::Failed;
    return NetworkDnsPersistenceDecision::Conflict;
}

bool sameNetworkDnsOperationIdentity(const NetworkDnsOperationIdentity &captured,
                                     const NetworkDnsOperationIdentity &current)
{
    return captured.running && current.running
        && captured.daemonGeneration == current.daemonGeneration
        && captured.refreshGeneration == current.refreshGeneration
        && captured.primaryEpoch == current.primaryEpoch
        && captured.requestId != 0 && captured.requestId == current.requestId
        && !captured.activeConnectionPath.isEmpty()
        && captured.activeConnectionPath == current.activeConnectionPath
        && !captured.profilePath.isEmpty() && captured.profilePath == current.profilePath
        && !captured.devicePath.isEmpty() && captured.devicePath == current.devicePath;
}

QDBusArgument &operator<<(QDBusArgument &argument,
                          const NetworkConnectionSettings &settings)
{
    argument.beginMap(QMetaType::fromType<QString>(), QMetaType::fromType<QVariantMap>());
    for (auto it = settings.sections.cbegin(); it != settings.sections.cend(); ++it) {
        argument.beginMapEntry();
        argument << it.key() << it.value();
        argument.endMapEntry();
    }
    argument.endMap();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                NetworkConnectionSettings &settings)
{
    settings.sections.clear();
    argument.beginMap();
    while (!argument.atEnd()) {
        QString name;
        QVariantMap values;
        argument.beginMapEntry();
        argument >> name >> values;
        argument.endMapEntry();
        settings.sections.insert(std::move(name), std::move(values));
    }
    argument.endMap();
    return argument;
}

} // namespace Astrea::System
