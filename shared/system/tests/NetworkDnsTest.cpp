#include "system/network/NetworkDns.hpp"

#include <QDBusMessage>
#include <QDBusMetaType>
#include <QtTest>

using namespace Astrea::System;

namespace {

QVariantMap family(const QString &method, bool ignoreAutomatic,
                   QStringList servers = {})
{
    QVariantMap result{{QStringLiteral("method"), method},
                       {QStringLiteral("ignore-auto-dns"), ignoreAutomatic}};
    if (!servers.isEmpty())
        result.insert(QStringLiteral("dns-data"), servers);
    return result;
}

NetworkConnectionSettings profile(const QVariantMap &ipv4, const QVariantMap &ipv6)
{
    return {{{QStringLiteral("connection"),
              {{QStringLiteral("id"), QStringLiteral("Wired" )},
               {QStringLiteral("uuid"), QStringLiteral("profile-uuid")},
               {QStringLiteral("autoconnect"), true}}},
             {QStringLiteral("802-3-ethernet"),
              {{QStringLiteral("mtu"), 1500u},
               {QStringLiteral("mac-address"), QStringLiteral("AA:BB:CC:DD:EE:FF")}}},
             {QStringLiteral("ipv4"), ipv4},
             {QStringLiteral("ipv6"), ipv6},
             {QStringLiteral("proxy"),
              {{QStringLiteral("method"), QStringLiteral("none")}}}}};
}

} // namespace

class NetworkDnsTest final : public QObject {
    Q_OBJECT

private slots:
    void noManualServersWithAutomaticDnsClassifiesAutomatic();
    void exclusiveLiteralServersClassifyCustom();
    void automaticDnsWithManualServersClassifiesAdvanced();
    void familyPolicyMismatchClassifiesAdvanced();
    void malformedDnsDataIsAdvancedAndNotEditable();
    void canonicalizesAndDeduplicatesLiteralAddresses();
    void rejectsNonLiteralAndEmptyCustomInput();
    void customPatchSplitsFamiliesAndPreservesUnrelatedSettings();
    void automaticPatchRemovesOnlyDnsProperties();
    void coherentProfileReadUsesBoundedVersionChecks();
    void update2ErrorsHaveConservativeDispositions();
    void ambiguousPersistenceIsVerifiedAgainstVersionAndDns();
    void appliedConnectionPatchPreservesItsRuntimeBaseline();
    void reapplyCallArgumentsUseDbusWireWidths();
    void authorityLossUsesTransactionPhaseRules();
    void dnsOperationIdentityIncludesEveryAuthority();
};

void NetworkDnsTest::noManualServersWithAutomaticDnsClassifiesAutomatic()
{
    const NetworkDnsProjection state = projectDns(profile(
        family(QStringLiteral("auto"), false), family(QStringLiteral("auto"), false)));

    QCOMPARE(state.mode, NetworkDnsMode::Automatic);
    QVERIFY(state.dnsEditable);
    QVERIFY(state.configuredIpv4DnsServers.isEmpty());
    QVERIFY(state.configuredIpv6DnsServers.isEmpty());
}

void NetworkDnsTest::exclusiveLiteralServersClassifyCustom()
{
    const NetworkDnsProjection state = projectDns(profile(
        family(QStringLiteral("auto"), true, {QStringLiteral("1.1.1.1")}),
        family(QStringLiteral("auto"), true, {QStringLiteral("2606:4700:4700::1111")})));

    QCOMPARE(state.mode, NetworkDnsMode::Custom);
    QVERIFY(state.dnsEditable);
    QCOMPARE(state.configuredIpv4DnsServers, QStringList{QStringLiteral("1.1.1.1")});
    QCOMPARE(state.configuredIpv6DnsServers,
             QStringList{QStringLiteral("2606:4700:4700::1111")});
}

void NetworkDnsTest::automaticDnsWithManualServersClassifiesAdvanced()
{
    const NetworkDnsProjection state = projectDns(profile(
        family(QStringLiteral("auto"), false, {QStringLiteral("1.1.1.1")}),
        family(QStringLiteral("auto"), false)));

    QCOMPARE(state.mode, NetworkDnsMode::Advanced);
}

void NetworkDnsTest::familyPolicyMismatchClassifiesAdvanced()
{
    const NetworkDnsProjection state = projectDns(profile(
        family(QStringLiteral("auto"), true, {QStringLiteral("1.1.1.1")}),
        family(QStringLiteral("auto"), false)));

    QCOMPARE(state.mode, NetworkDnsMode::Advanced);
}

void NetworkDnsTest::malformedDnsDataIsAdvancedAndNotEditable()
{
    QVariantMap malformed = family(QStringLiteral("auto"), true);
    malformed.insert(QStringLiteral("dns-data"),
                     QVariantList{QVariantMap{{QStringLiteral("address"),
                                               QStringLiteral("1.1.1.1")}}});
    const NetworkDnsProjection state = projectDns(profile(
        malformed, family(QStringLiteral("auto"), true)));

    QCOMPARE(state.mode, NetworkDnsMode::Advanced);
    QVERIFY(!state.dnsEditable);
}

void NetworkDnsTest::canonicalizesAndDeduplicatesLiteralAddresses()
{
    const NetworkDnsValidation result = canonicalizeDnsServers({
        QStringLiteral("1.1.1.1"), QStringLiteral("2606:4700:4700:0:0:0:0:1111"),
        QStringLiteral("1.1.1.1"), QStringLiteral("2606:4700:4700::1111")});

    QVERIFY(result.valid);
    QCOMPARE(result.servers, (QStringList{QStringLiteral("1.1.1.1"),
                                         QStringLiteral("2606:4700:4700::1111")}));
    QCOMPARE(result.ipv4Servers, QStringList{QStringLiteral("1.1.1.1")});
    QCOMPARE(result.ipv6Servers, QStringList{QStringLiteral("2606:4700:4700::1111")});
}

void NetworkDnsTest::rejectsNonLiteralAndEmptyCustomInput()
{
    for (const QString &invalid : {QString(), QStringLiteral("resolver.example"),
                                   QStringLiteral("https://1.1.1.1/dns-query"),
                                   QStringLiteral("1.1.1.1:53"),
                                   QStringLiteral("999.1.1.1"), QStringLiteral("2001:::1")}) {
        const NetworkDnsValidation result = canonicalizeDnsServers(
            invalid.isEmpty() ? QStringList{} : QStringList{invalid});
        QVERIFY2(!result.valid, qPrintable(invalid));
    }
}

void NetworkDnsTest::customPatchSplitsFamiliesAndPreservesUnrelatedSettings()
{
    const NetworkConnectionSettings original = profile(
        {{QStringLiteral("method"), QStringLiteral("manual")},
         {QStringLiteral("routes"), QStringList{QStringLiteral("route-v4")}},
         {QStringLiteral("address-data"), QStringList{QStringLiteral("address-v4")}},
         {QStringLiteral("dns"), QStringList{QStringLiteral("deprecated-v4")}},
         {QStringLiteral("dns-search"), QStringList{QStringLiteral("corp.example")}},
         {QStringLiteral("dns-options"), QStringList{QStringLiteral("rotate")}},
         {QStringLiteral("dns-priority"), -42}},
        {{QStringLiteral("method"), QStringLiteral("auto")},
         {QStringLiteral("routes"), QStringList{QStringLiteral("route-v6")}},
         {QStringLiteral("dns-priority"), 7}});
    const NetworkDnsPatchResult result = patchDnsSettings(original, NetworkDnsMode::Custom,
        {QStringLiteral("1.1.1.1"), QStringLiteral("2606:4700:4700::1111")});

    QVERIFY(result.valid);
    const QVariantMap ipv4 = result.settings.sections.value(QStringLiteral("ipv4"));
    const QVariantMap ipv6 = result.settings.sections.value(QStringLiteral("ipv6"));
    QCOMPARE(ipv4.value(QStringLiteral("method")), QStringLiteral("manual"));
    QCOMPARE(ipv4.value(QStringLiteral("routes")), QStringList{QStringLiteral("route-v4")});
    QCOMPARE(ipv4.value(QStringLiteral("address-data")),
             QStringList{QStringLiteral("address-v4")});
    QCOMPARE(ipv4.value(QStringLiteral("dns-data")), QStringList{QStringLiteral("1.1.1.1")});
    QVERIFY(!ipv4.contains(QStringLiteral("dns")));
    QCOMPARE(ipv4.value(QStringLiteral("ignore-auto-dns")).toBool(), true);
    QCOMPARE(ipv4.value(QStringLiteral("dns-search")), QStringList{QStringLiteral("corp.example")});
    QCOMPARE(ipv4.value(QStringLiteral("dns-options")), QStringList{QStringLiteral("rotate")});
    QCOMPARE(ipv4.value(QStringLiteral("dns-priority")).toInt(), -42);
    QCOMPARE(ipv6.value(QStringLiteral("dns-data")),
             QStringList{QStringLiteral("2606:4700:4700::1111")});
    QVERIFY(!ipv6.contains(QStringLiteral("dns")));
    QCOMPARE(ipv6.value(QStringLiteral("dns-priority")).toInt(), 7);
    QCOMPARE(result.settings.sections.value(QStringLiteral("connection")),
             original.sections.value(QStringLiteral("connection")));
    QCOMPARE(result.settings.sections.value(QStringLiteral("802-3-ethernet")),
             original.sections.value(QStringLiteral("802-3-ethernet")));
    QCOMPARE(result.settings.sections.value(QStringLiteral("proxy")),
             original.sections.value(QStringLiteral("proxy")));
}

void NetworkDnsTest::automaticPatchRemovesOnlyDnsProperties()
{
    const NetworkConnectionSettings original = profile(
        {{QStringLiteral("method"), QStringLiteral("auto")},
         {QStringLiteral("dns-data"), QStringList{QStringLiteral("1.1.1.1")}},
         {QStringLiteral("dns"), QStringList{QStringLiteral("legacy")}},
         {QStringLiteral("dns-priority"), -7},
         {QStringLiteral("dns-search"), QStringList{QStringLiteral("corp.example")}}},
        {{QStringLiteral("method"), QStringLiteral("auto")},
         {QStringLiteral("dns-priority"), 13}});
    const NetworkDnsPatchResult result = patchDnsSettings(original, NetworkDnsMode::Automatic, {});

    QVERIFY(result.valid);
    for (const QString familyName : {QStringLiteral("ipv4"), QStringLiteral("ipv6")}) {
        const QVariantMap familySettings = result.settings.sections.value(familyName);
        QVERIFY(!familySettings.value(QStringLiteral("ignore-auto-dns")).toBool());
        QVERIFY(!familySettings.contains(QStringLiteral("dns-data")));
        QVERIFY(!familySettings.contains(QStringLiteral("dns")));
    }
    QCOMPARE(result.settings.sections.value(QStringLiteral("ipv4")).value(
                 QStringLiteral("dns-priority")).toInt(), -7);
    QCOMPARE(result.settings.sections.value(QStringLiteral("ipv4")).value(
                 QStringLiteral("dns-search")), QStringList{QStringLiteral("corp.example")});
}

void NetworkDnsTest::coherentProfileReadUsesBoundedVersionChecks()
{
    QCOMPARE(dnsProfileReadDecision(true, true, true, 41, 41, 0),
             NetworkDnsProfileReadDecision::Proceed);
    QCOMPARE(dnsProfileReadDecision(true, true, true, 41, 42, 0),
             NetworkDnsProfileReadDecision::Retry);
    QCOMPARE(dnsProfileReadDecision(true, true, true, 41, 42, 2),
             NetworkDnsProfileReadDecision::Conflict);
    QCOMPARE(dnsProfileReadDecision(true, false, true, 41, 41, 0),
             NetworkDnsProfileReadDecision::Failed);
    QCOMPARE(dnsProfileReadDecision(true, true, false, 41, 0, 0),
             NetworkDnsProfileReadDecision::Failed);
}

void NetworkDnsTest::update2ErrorsHaveConservativeDispositions()
{
    QCOMPARE(dnsUpdateErrorDisposition(QStringLiteral(
                 "org.freedesktop.NetworkManager.Settings.Connection.Error.VersionIdMismatch")),
             NetworkDnsUpdateDisposition::Conflict);
    QCOMPARE(dnsUpdateErrorDisposition(QStringLiteral(
                 "org.freedesktop.NetworkManager.Settings.Connection.Error.PermissionDenied")),
             NetworkDnsUpdateDisposition::Rejected);
    for (const QString &errorName : {
             QStringLiteral("org.freedesktop.DBus.Error.NoReply"),
             QStringLiteral("org.freedesktop.DBus.Error.Timeout"),
             QStringLiteral("org.freedesktop.DBus.Error.Disconnected")}) {
        QCOMPARE(dnsUpdateErrorDisposition(errorName), NetworkDnsUpdateDisposition::Verify);
    }
}

void NetworkDnsTest::ambiguousPersistenceIsVerifiedAgainstVersionAndDns()
{
    const NetworkDnsIntent intent{NetworkDnsMode::Custom,
        {QStringLiteral("1.1.1.1")}, {QStringLiteral("2606:4700:4700::1111")}, 41};
    const NetworkDnsProjection requested{NetworkDnsMode::Custom,
        {QStringLiteral("1.1.1.1")}, {QStringLiteral("2606:4700:4700::1111")}, true};
    const NetworkDnsProjection unchanged{NetworkDnsMode::Automatic, {}, {}, true};
    const NetworkDnsProjection externalEdit{NetworkDnsMode::Custom,
        {QStringLiteral("9.9.9.9")}, {QStringLiteral("2620:fe::fe")}, true};

    QCOMPARE(dnsPersistenceDecision(true, requested, 42, intent),
             NetworkDnsPersistenceDecision::Confirmed);
    QCOMPARE(dnsPersistenceDecision(true, unchanged, 41, intent),
             NetworkDnsPersistenceDecision::Failed);
    QCOMPARE(dnsPersistenceDecision(true, externalEdit, 42, intent),
             NetworkDnsPersistenceDecision::Conflict);
    QCOMPARE(dnsPersistenceDecision(false, {}, 0, intent),
             NetworkDnsPersistenceDecision::Indeterminate);
}

void NetworkDnsTest::appliedConnectionPatchPreservesItsRuntimeBaseline()
{
    NetworkConnectionSettings applied = profile(
        family(QStringLiteral("auto"), false, {QStringLiteral("4.4.4.4")}),
        family(QStringLiteral("auto"), false, {QStringLiteral("2001:4860:4860::8844")}));
    applied.sections[QStringLiteral("connection")].insert(
        QStringLiteral("id"), QStringLiteral("Live connection name"));
    applied.sections[QStringLiteral("802-3-ethernet")].insert(
        QStringLiteral("mtu"), 9000u);
    const NetworkDnsIntent intent{NetworkDnsMode::Custom,
        {QStringLiteral("1.1.1.1")}, {QStringLiteral("2606:4700:4700::1111")}, 99};

    constexpr quint64 appliedVersion = 0x1'0000'0001ULL;
    const NetworkDnsReapplyPayload payload = prepareDnsReapply(applied, appliedVersion, intent);

    QVERIFY(payload.valid);
    QCOMPARE(payload.appliedVersionId, appliedVersion);
    QCOMPARE(payload.settings.sections.value(QStringLiteral("connection"))
                 .value(QStringLiteral("id")).toString(),
             QStringLiteral("Live connection name"));
    QCOMPARE(payload.settings.sections.value(QStringLiteral("802-3-ethernet"))
                 .value(QStringLiteral("mtu")).toUInt(), 9000u);
    const NetworkDnsProjection dns = projectDns(payload.settings);
    QCOMPARE(dns.mode, NetworkDnsMode::Custom);
    QCOMPARE(dns.configuredIpv4DnsServers, intent.ipv4Servers);
    QCOMPARE(dns.configuredIpv6DnsServers, intent.ipv6Servers);
}

void NetworkDnsTest::reapplyCallArgumentsUseDbusWireWidths()
{
    const NetworkConnectionSettings applied = profile(
        family(QStringLiteral("auto"), false), family(QStringLiteral("auto"), false));
    const NetworkDnsIntent intent{NetworkDnsMode::Custom,
        {QStringLiteral("1.1.1.1")}, {}, 99};
    constexpr quint64 appliedVersion = 0x1'0000'0001ULL;
    const NetworkDnsReapplyPayload payload = prepareDnsReapply(applied, appliedVersion, intent);
    QVERIFY(payload.valid);

    qDBusRegisterMetaType<NetworkConnectionSettings>();
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.NetworkManager"), QStringLiteral("/device/1"),
        QStringLiteral("org.freedesktop.NetworkManager.Device"), QStringLiteral("Reapply"));
    message.setArguments(dnsReapplyArguments(payload));

    const QList<QVariant> arguments = message.arguments();
    QCOMPARE(arguments.size(), 3);
    const auto signatureFor = [](const QVariant &argument) {
        return QString::fromLatin1(QDBusMetaType::typeToSignature(argument.metaType()));
    };
    QCOMPARE(signatureFor(arguments.at(0)), QStringLiteral("a{sa{sv}}"));
    QCOMPARE(arguments.at(1).metaType(), QMetaType::fromType<qulonglong>());
    QCOMPARE(arguments.at(1).toULongLong(), appliedVersion);
    QCOMPARE(signatureFor(arguments.at(1)), QStringLiteral("t"));
    QCOMPARE(arguments.at(2).metaType(), QMetaType::fromType<quint32>());
    QCOMPARE(arguments.at(2).toUInt(), 0u);
    QCOMPARE(signatureFor(arguments.at(2)), QStringLiteral("u"));
    QCOMPARE(signatureFor(arguments.at(0)) + signatureFor(arguments.at(1))
                 + signatureFor(arguments.at(2)),
             QStringLiteral("a{sa{sv}}tu"));
}

void NetworkDnsTest::authorityLossUsesTransactionPhaseRules()
{
    const NetworkDnsAuthorityLossResult idle =
        dnsAuthorityLossResult(NetworkDnsTransactionPhase::Idle);
    QVERIFY(!idle.shouldComplete);

    const NetworkDnsAuthorityLossResult preparing =
        dnsAuthorityLossResult(NetworkDnsTransactionPhase::Preparing);
    QVERIFY(preparing.shouldComplete);
    QCOMPARE(preparing.outcome, NetworkDnsOutcome::Failed);
    QCOMPARE(preparing.error,
             QStringLiteral("NetworkManager became unavailable before DNS settings could be saved."));
    QVERIFY(!preparing.dnsAuthorityCurrent);

    for (const NetworkDnsTransactionPhase phase : {
             NetworkDnsTransactionPhase::Persisting,
             NetworkDnsTransactionPhase::VerifyingPersistence}) {
        const NetworkDnsAuthorityLossResult result = dnsAuthorityLossResult(phase);
        QVERIFY(result.shouldComplete);
        QCOMPARE(result.outcome, NetworkDnsOutcome::Indeterminate);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(!result.dnsAuthorityCurrent);
    }

    const NetworkDnsAuthorityLossResult applying =
        dnsAuthorityLossResult(NetworkDnsTransactionPhase::ApplyingRuntime);
    QVERIFY(applying.shouldComplete);
    QCOMPARE(applying.outcome, NetworkDnsOutcome::SavedNeedsReconnect);
    QVERIFY(!applying.dnsAuthorityCurrent);
    QCOMPARE(dnsTransactionPhaseAfterTerminalResult(), NetworkDnsTransactionPhase::Idle);
}

void NetworkDnsTest::dnsOperationIdentityIncludesEveryAuthority()
{
    const NetworkDnsOperationIdentity captured{true, 9, 12, 4, 17,
        QStringLiteral("/active/1"), QStringLiteral("/settings/1"), QStringLiteral("/device/1")};
    QVERIFY(sameNetworkDnsOperationIdentity(captured, captured));

    const auto changed = [&captured](auto mutate) {
        NetworkDnsOperationIdentity current = captured;
        mutate(current);
        return sameNetworkDnsOperationIdentity(captured, current);
    };
    QVERIFY(!changed([](auto &id) { id.running = false; }));
    QVERIFY(!changed([](auto &id) { ++id.daemonGeneration; }));
    QVERIFY(!changed([](auto &id) { ++id.refreshGeneration; }));
    QVERIFY(!changed([](auto &id) { ++id.primaryEpoch; }));
    QVERIFY(!changed([](auto &id) { ++id.requestId; }));
    QVERIFY(!changed([](auto &id) { id.activeConnectionPath = QStringLiteral("/active/2"); }));
    QVERIFY(!changed([](auto &id) { id.profilePath = QStringLiteral("/settings/2"); }));
    QVERIFY(!changed([](auto &id) { id.devicePath = QStringLiteral("/device/2"); }));
}

QTEST_GUILESS_MAIN(NetworkDnsTest)
#include "NetworkDnsTest.moc"
