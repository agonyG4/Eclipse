#include "core/SettingsController.hpp"
#include "core/navigation/SettingsNavigationModel.hpp"
#include "system/network/NetworkBackend.hpp"
#include "system/network/NetworkService.hpp"
#include "system/network/WifiNetworkModel.hpp"
#include "theme/ThemeController.hpp"

#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

namespace {

class TestI18n final : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE QString tr(const QString &, const QString &fallback) const { return fallback; }
};

class FakeNetworkBackend final : public Astrea::System::NetworkBackend {
public:
    bool start(const Callbacks &callbacks, QString *error) override
    {
        ++starts;
        callbacks_ = callbacks;
        if (!startResult && error)
            *error = QStringLiteral("NetworkManager is not available");
        return startResult;
    }
    void stop() override { ++stops; callbacks_ = {}; }
    bool setWifiEnabled(bool enabled, quint64 id) override
    {
        powerRequests.append({enabled, id});
        return actionResult;
    }
    bool requestWifiScan() override { ++scans; return actionResult; }
    bool applyDns(Astrea::System::NetworkDnsMode mode, const QStringList &servers,
                  quint64 id, QString *error) override
    {
        Q_UNUSED(error)
        dnsRequests.append({mode, servers, id});
        return actionResult;
    }
    void publish(const Astrea::System::NetworkSnapshot &snapshot)
    {
        if (callbacks_.snapshotChanged)
            callbacks_.snapshotChanged(snapshot);
    }
    void fail(const QString &message)
    {
        if (callbacks_.errorChanged)
            callbacks_.errorChanged(message);
    }
    void completeLastPowerRequest()
    {
        if (!callbacks_.operationFinished || powerRequests.isEmpty())
            return;
        callbacks_.operationFinished({Astrea::System::NetworkOperationKind::WifiEnabled,
                                      powerRequests.constLast().second, true, {}});
    }
    void completeDns(quint64 id, Astrea::System::NetworkDnsOutcome outcome,
                     const QString &error = {}, bool dnsAuthorityCurrent = true)
    {
        if (!callbacks_.operationFinished)
            return;
        Astrea::System::NetworkOperationResult result;
        result.kind = Astrea::System::NetworkOperationKind::DnsConfiguration;
        result.requestId = id;
        result.dnsOutcome = outcome;
        result.success = outcome == Astrea::System::NetworkDnsOutcome::Applied;
        result.error = error;
        result.dnsAuthorityCurrent = dnsAuthorityCurrent;
        callbacks_.operationFinished(result);
    }

    bool startResult = true;
    bool actionResult = true;
    int starts = 0;
    int stops = 0;
    int scans = 0;
    QList<QPair<bool, quint64>> powerRequests;
    struct DnsRequest {
        Astrea::System::NetworkDnsMode mode;
        QStringList servers;
        quint64 requestId;
    };
    QList<DnsRequest> dnsRequests;

private:
    Callbacks callbacks_;
};

class Page final {
public:
    explicit Page(bool backendStarts = true)
    {
        auto backend = std::make_unique<FakeNetworkBackend>();
        fake = backend.get();
        fake->startResult = backendStarts;
        auto network = std::make_unique<Astrea::System::NetworkService>(std::move(backend));
        controller = std::make_unique<SettingsController>(
            std::make_unique<SettingsNavigationModel>(), SettingsUserProfile{},
            SettingsIconResolver{},
            std::unique_ptr<Astrea::System::BluetoothService>{},
            std::unique_ptr<Astrea::System::AudioService>{}, std::move(network));
        controller->navigateTo(QStringLiteral("internet"));
        engine.addImportPath(QStringLiteral("qrc:/"));
        engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"),
                                                 controller.get());
        engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &i18n);
        engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &theme);
        component = std::make_unique<QQmlComponent>(&engine, controller->selectedPageSource());
        window.resize(1000, 760);
    }
    ~Page() { destroy(); }

    QQuickItem *create()
    {
        QObject *object = component->create();
        if (!object)
            return nullptr;
        root = qobject_cast<QQuickItem *>(object);
        if (!root) {
            delete object;
            return nullptr;
        }
        root->setParentItem(window.contentItem());
        root->setSize(window.size());
        window.show();
        events();
        return root;
    }
    void publish(const Astrea::System::NetworkSnapshot &snapshot)
    {
        fake->publish(snapshot);
        events();
    }
    void events() const
    {
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
    }
    QObject *find(const QString &name) const { return findIn(root, name); }
    QList<QQuickItem *> named(const QString &name) const
    {
        QList<QQuickItem *> out;
        collect(root, name, out);
        return out;
    }
    QList<QQuickItem *> all() const
    {
        QList<QQuickItem *> out;
        collectAll(root, out);
        return out;
    }
    void destroy()
    {
        if (!root)
            return;
        root->setParentItem(nullptr);
        delete root;
        root = nullptr;
    }

    std::unique_ptr<SettingsController> controller;
    FakeNetworkBackend *fake = nullptr;
    QQuickItem *root = nullptr;
    QString errors() const { return component->errorString(); }

private:
    static QObject *findIn(QQuickItem *item, const QString &name)
    {
        if (!item)
            return nullptr;
        if (item->objectName() == name)
            return item;
        for (QQuickItem *child : item->childItems())
            if (QObject *found = findIn(child, name))
                return found;
        return nullptr;
    }
    static void collect(QQuickItem *item, const QString &name, QList<QQuickItem *> &out)
    {
        if (!item)
            return;
        if (item->objectName() == name)
            out.append(item);
        for (QQuickItem *child : item->childItems())
            collect(child, name, out);
    }
    static void collectAll(QQuickItem *item, QList<QQuickItem *> &out)
    {
        if (!item)
            return;
        out.append(item);
        for (QQuickItem *child : item->childItems())
            collectAll(child, out);
    }

    TestI18n i18n;
    ThemeController theme;
    QQmlApplicationEngine engine;
    std::unique_ptr<QQmlComponent> component;
    QQuickWindow window;
};

Astrea::System::NetworkSnapshot ready()
{
    Astrea::System::NetworkSnapshot result;
    result.daemonAvailable = true;
    result.wifiAvailable = true;
    result.wifiHardwareEnabled = true;
    result.wifiEnabled = true;
    return result;
}

QQuickItem *rowFor(Page &page, const QString &ssid)
{
    for (QQuickItem *label : page.named(QStringLiteral("availableNetworkSsid"))) {
        if (label->property("text").toString() != ssid)
            continue;
        for (QQuickItem *parent = label; parent; parent = parent->parentItem())
            if (parent->objectName() == QStringLiteral("availableNetworkRow"))
                return parent;
    }
    return nullptr;
}

QStringList rowText(Page &page, QQuickItem *row)
{
    QStringList result;
    for (QQuickItem *item : page.all()) {
        if (item->metaObject()->indexOfProperty("text") < 0)
            continue;
        for (QQuickItem *parent = item; parent; parent = parent->parentItem()) {
            if (parent == row) {
                result.append(item->property("text").toString());
                break;
            }
        }
    }
    return result;
}

Astrea::System::NetworkSnapshot connectedNetwork()
{
    auto snapshot = ready();
    snapshot.connected = true;
    snapshot.connectionType = Astrea::System::NetworkConnectionType::Wired;
    snapshot.connectionName = QStringLiteral("Wired <b>connection</b>");
    snapshot.activeConnectionPath = QStringLiteral("/active/1");
    snapshot.profilePath = QStringLiteral("/settings/1");
    snapshot.devicePath = QStringLiteral("/device/ethernet0");
    snapshot.dnsEditable = true;
    return snapshot;
}

} // namespace

class SettingsInternetQmlTest final : public QObject {
    Q_OBJECT
private slots:
    void serviceHealthAndConnectionProjection();
    void wiredConnectionDetailsProjectAndClear();
    void wifiPowerAndScanAreAuthoritative();
    void availableNetworksRemainInformationalPlainTextRows();
    void dnsSectionProjectsModeProviderAndConnectionServers();
    void dnsPresetDraftRequiresExplicitApplyAndShowsPartialSuccess();
    void dnsInvalidDraftAndExternalChangeRemainExplicit();
    void dnsOwnRefreshChurnDoesNotMarkDraftExternal();
    void dnsFingerprintUsesOpaqueRevisionAndDnsState();
    void dnsAuthoritySwitchLoadsNewDraftAfterPartialSuccess();
    void dnsConflictPreservesDraftForExplicitResolution();
    void dnsIndeterminateRequiresAuthoritativeReload();
    void dnsFailedOutcomeKeepsDraftRetryable();
    void dnsSlowStateRemainsPendingInPresentation();
};

void SettingsInternetQmlTest::serviceHealthAndConnectionProjection()
{
    Page page;
    QCOMPARE(page.controller->selectedPageSource(),
             QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Internet.qml")));
    QVERIFY2(page.create(), qPrintable(page.errors()));
    QCOMPARE(page.find(QStringLiteral("networkServiceStatus"))->property("text").toString(),
             QStringLiteral("Network service is inactive"));
    QVERIFY(page.controller->network()->start());
    QCOMPARE(page.fake->starts, 1);
    QCOMPARE(page.find(QStringLiteral("networkServiceStatus"))->property("text").toString(),
             QStringLiteral("Network service is ready"));
    page.publish(ready());
    QCOMPARE(page.find(QStringLiteral("networkConnectionState"))->property("text").toString(),
             QStringLiteral("Disconnected"));
    QVERIFY(!page.find(QStringLiteral("connectionDetails"))->property("visible").toBool());

    auto snapshot = ready();
    snapshot.connected = true;
    snapshot.connectionType = Astrea::System::NetworkConnectionType::Wifi;
    snapshot.connectionName = QStringLiteral("<i>Lab & guest</i>");
    snapshot.interfaceName = QStringLiteral("wlp-test");
    snapshot.downloadBytesPerSecond = 1000;
    snapshot.uploadBytesPerSecond = 2'500'000;
    page.publish(snapshot);
    QCOMPARE(page.find(QStringLiteral("connectionName"))->property("text").toString(),
             QStringLiteral("<i>Lab & guest</i>"));
    QCOMPARE(page.find(QStringLiteral("connectionName"))->property("textFormat").toInt(), 0);
    QCOMPARE(page.find(QStringLiteral("connectionTypeLabel"))->property("text").toString(),
             QStringLiteral("Wi-Fi"));
    QCOMPARE(page.find(QStringLiteral("connectionInterface"))->property("text").toString(),
             QStringLiteral("Interface: wlp-test"));
    QCOMPARE(page.find(QStringLiteral("downloadRate"))->property("text").toString(),
             QStringLiteral("Download: 1.0 KB/s"));
    QCOMPARE(page.find(QStringLiteral("uploadRate"))->property("text").toString(),
             QStringLiteral("Upload: 2.5 MB/s"));

    snapshot.connectionType = Astrea::System::NetworkConnectionType::Wired;
    page.publish(snapshot);
    QCOMPARE(page.find(QStringLiteral("connectionTypeLabel"))->property("text").toString(),
             QStringLiteral("Ethernet"));
    snapshot.connectionType = Astrea::System::NetworkConnectionType::Other;
    page.publish(snapshot);
    QCOMPARE(page.find(QStringLiteral("connectionTypeLabel"))->property("text").toString(),
             QStringLiteral("Network"));
    snapshot.connected = false;
    snapshot.connectionName = QStringLiteral("stale name");
    page.publish(snapshot);
    QVERIFY(!page.find(QStringLiteral("connectionDetails"))->property("visible").toBool());
    QCOMPARE(page.find(QStringLiteral("connectionName"))->property("text").toString(), QString());

    page.fake->fail(QStringLiteral("NetworkManager connection lost"));
    page.events();
    QCOMPARE(page.controller->network()->state(), Astrea::System::SystemServiceState::Degraded);
    QCOMPARE(page.find(QStringLiteral("networkServiceError"))->property("text").toString(),
             QStringLiteral("NetworkManager connection lost"));

    Page unavailable(false);
    QVERIFY(unavailable.create());
    QVERIFY(unavailable.controller->network()->start());
    QCOMPARE(unavailable.controller->network()->state(),
             Astrea::System::SystemServiceState::Unavailable);
    QCOMPARE(unavailable.find(QStringLiteral("networkServiceError"))->property("text").toString(),
             QStringLiteral("NetworkManager is not available"));
}

void SettingsInternetQmlTest::wiredConnectionDetailsProjectAndClear()
{
    Page page;
    QVERIFY2(page.create(), qPrintable(page.errors()));
    QVERIFY(page.controller->network()->start());

    auto snapshot = ready();
    snapshot.connected = true;
    snapshot.connectionType = Astrea::System::NetworkConnectionType::Wired;
    snapshot.connectionName = QStringLiteral("Wired connection 1");
    snapshot.interfaceName = QStringLiteral("enp4s0");
    snapshot.hardwareAddress = QStringLiteral("AA:BB:CC:DD:EE:FF");
    snapshot.mtu = 1500;
    snapshot.linkCarrier = true;
    snapshot.linkSpeedMbps = 1000;
    snapshot.ipv4Addresses = {QStringLiteral("192.168.1.50/24"),
                              QStringLiteral("192.168.1.51/24")};
    snapshot.ipv4Gateway = QStringLiteral("192.168.1.1");
    snapshot.ipv6Addresses = {QStringLiteral("2804:abcd::1234/64")};
    snapshot.ipv6Gateway = QStringLiteral("fe80::1");
    page.publish(snapshot);

    QCOMPARE(page.find(QStringLiteral("connectionDetailsSection"))->property("visible").toBool(),
             true);
    QCOMPARE(page.find(QStringLiteral("connectionIpv4Addresses"))->property("visible").toBool(),
             true);
    QCOMPARE(page.find(QStringLiteral("connectionIpv4Addresses"))->property("sublabel").toString(),
             QStringLiteral("192.168.1.50/24\n192.168.1.51/24"));
    QCOMPARE(page.find(QStringLiteral("connectionIpv4Gateway"))->property("sublabel").toString(),
             QStringLiteral("192.168.1.1"));
    QCOMPARE(page.find(QStringLiteral("connectionIpv6Addresses"))->property("sublabel").toString(),
             QStringLiteral("2804:abcd::1234/64"));
    QCOMPARE(page.find(QStringLiteral("connectionIpv6Gateway"))->property("sublabel").toString(),
             QStringLiteral("fe80::1"));
    QCOMPARE(page.find(QStringLiteral("connectionHardwareAddress"))->property("sublabel").toString(),
             QStringLiteral("AA:BB:CC:DD:EE:FF"));
    QCOMPARE(page.find(QStringLiteral("connectionMtu"))->property("sublabel").toString(),
             QStringLiteral("1500"));
    QCOMPARE(page.find(QStringLiteral("connectionLink"))->property("sublabel").toString(),
             QStringLiteral("Up"));
    QCOMPARE(page.find(QStringLiteral("connectionLinkSpeed"))->property("sublabel").toString(),
             QStringLiteral("1 Gbps"));
    for (const QString &name : {QStringLiteral("connectionIpv4Addresses"),
                                QStringLiteral("connectionIpv4Gateway"),
                                QStringLiteral("connectionIpv6Addresses"),
                                QStringLiteral("connectionIpv6Gateway"),
                                QStringLiteral("connectionHardwareAddress"),
                                QStringLiteral("connectionMtu"),
                                QStringLiteral("connectionLink"),
                                QStringLiteral("connectionLinkSpeed")}) {
        QCOMPARE(page.find(name)->property("sublabelTextFormat").toInt(), 0);
    }

    snapshot.linkSpeedMbps = 2500;
    page.publish(snapshot);
    QCOMPARE(page.find(QStringLiteral("connectionLinkSpeed"))->property("sublabel").toString(),
             QStringLiteral("2.5 Gbps"));

    snapshot.connectionType = Astrea::System::NetworkConnectionType::Wifi;
    snapshot.linkSpeedMbps = 0;
    page.publish(snapshot);
    QVERIFY(!page.find(QStringLiteral("connectionLink"))->property("visible").toBool());
    QVERIFY(!page.find(QStringLiteral("connectionLinkSpeed"))->property("visible").toBool());

    snapshot.connectionType = Astrea::System::NetworkConnectionType::Other;
    snapshot.ipv4Addresses = {QStringLiteral("10.0.0.5/24")};
    page.publish(snapshot);
    QVERIFY(page.find(QStringLiteral("connectionIpv4Addresses"))->property("visible").toBool());
    QVERIFY(!page.find(QStringLiteral("connectionLink"))->property("visible").toBool());
    QVERIFY(!page.find(QStringLiteral("connectionLinkSpeed"))->property("visible").toBool());

    snapshot.connectionType = Astrea::System::NetworkConnectionType::Wired;
    snapshot.hardwareAddress.clear();
    snapshot.mtu = 0;
    snapshot.linkCarrier = false;
    snapshot.ipv4Addresses.clear();
    snapshot.ipv4Gateway.clear();
    snapshot.ipv6Addresses.clear();
    snapshot.ipv6Gateway.clear();
    page.publish(snapshot);
    QVERIFY(!page.find(QStringLiteral("connectionIpv4Addresses"))->property("visible").toBool());
    QVERIFY(!page.find(QStringLiteral("connectionIpv4Gateway"))->property("visible").toBool());
    QVERIFY(!page.find(QStringLiteral("connectionIpv6Addresses"))->property("visible").toBool());
    QVERIFY(!page.find(QStringLiteral("connectionIpv6Gateway"))->property("visible").toBool());
    QVERIFY(!page.find(QStringLiteral("connectionHardwareAddress"))->property("visible").toBool());
    QVERIFY(!page.find(QStringLiteral("connectionMtu"))->property("visible").toBool());
    QCOMPARE(page.find(QStringLiteral("connectionLink"))->property("sublabel").toString(),
             QStringLiteral("Down"));
    QVERIFY(!page.find(QStringLiteral("connectionDetailsEmpty"))->property("visible").toBool());

    snapshot.connected = false;
    snapshot.connectionType = Astrea::System::NetworkConnectionType::None;
    snapshot.connectionName = QStringLiteral("stale ethernet name");
    snapshot.hardwareAddress = QStringLiteral("stale mac");
    snapshot.mtu = 1500;
    snapshot.linkCarrier = true;
    snapshot.linkSpeedMbps = 1000;
    snapshot.ipv4Addresses = {QStringLiteral("stale address")};
    snapshot.ipv4Gateway = QStringLiteral("stale gateway");
    snapshot.ipv6Addresses = {QStringLiteral("stale ipv6")};
    snapshot.ipv6Gateway = QStringLiteral("stale ipv6 gateway");
    page.publish(snapshot);
    QVERIFY(!page.find(QStringLiteral("connectionDetailsSection"))->property("visible").toBool());
    for (const QString &name : {QStringLiteral("connectionIpv4Addresses"),
                                QStringLiteral("connectionIpv4Gateway"),
                                QStringLiteral("connectionIpv6Addresses"),
                                QStringLiteral("connectionIpv6Gateway"),
                                QStringLiteral("connectionHardwareAddress"),
                                QStringLiteral("connectionMtu"),
                                QStringLiteral("connectionLink"),
                                QStringLiteral("connectionLinkSpeed")}) {
        QVERIFY(!page.find(name)->property("visible").toBool());
        QVERIFY(page.find(name)->property("sublabel").toString().isEmpty());
    }
}

void SettingsInternetQmlTest::wifiPowerAndScanAreAuthoritative()
{
    Page page;
    QVERIFY(page.create());
    QVERIFY(page.controller->network()->start());
    auto snapshot = ready();
    page.publish(snapshot);

    QObject *toggle = page.find(QStringLiteral("wifiPowerToggle"));
    QObject *scan = page.find(QStringLiteral("wifiScanButton"));
    QVERIFY(toggle && scan);
    QVERIFY(toggle->property("enabled").toBool());
    QVERIFY(scan->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(scan, "clicked", Qt::DirectConnection));
    QCOMPARE(page.fake->scans, 1);
    QVERIFY(!page.controller->network()->wifiScanning());
    snapshot.wifiScanning = true;
    page.publish(snapshot);
    QVERIFY(page.controller->network()->wifiScanning());
    QVERIFY(!scan->property("enabled").toBool());

    snapshot.wifiScanning = false;
    page.publish(snapshot);
    QVERIFY(QMetaObject::invokeMethod(toggle, "toggled", Qt::DirectConnection,
                                      Q_ARG(bool, false)));
    QCOMPARE(page.fake->powerRequests.size(), 1);
    QVERIFY(!page.fake->powerRequests.constFirst().first);
    QVERIFY(toggle->property("checked").toBool());
    QVERIFY(!toggle->property("enabled").toBool());
    QVERIFY(!scan->property("enabled").toBool());
    QVERIFY(page.controller->network()->wifiPending());
    page.publish(snapshot);
    QVERIFY(toggle->property("checked").toBool());
    page.fake->completeLastPowerRequest();
    page.events();
    QVERIFY(page.controller->network()->wifiPending());
    QVERIFY(toggle->property("checked").toBool());

    snapshot.wifiEnabled = false;
    page.publish(snapshot);
    QVERIFY(!page.controller->network()->wifiPending());
    QVERIFY(!toggle->property("checked").toBool());
    QVERIFY(!scan->property("enabled").toBool());
    QVERIFY(page.find(QStringLiteral("wifiSoftwareDisabledMessage"))
                ->property("visible").toBool());
    snapshot.wifiHardwareEnabled = false;
    page.publish(snapshot);
    QVERIFY(page.find(QStringLiteral("wifiHardwareUnavailableMessage"))
                ->property("visible").toBool());
    snapshot.wifiAvailable = false;
    page.publish(snapshot);
    QVERIFY(page.find(QStringLiteral("wifiUnavailableMessage"))->property("visible").toBool());
    QVERIFY(!toggle->property("enabled").toBool());
}

void SettingsInternetQmlTest::availableNetworksRemainInformationalPlainTextRows()
{
    Page page;
    QVERIFY(page.create());
    QVERIFY(page.controller->network()->start());
    auto snapshot = ready();
    snapshot.wifiNetworks = {
        {QStringLiteral("Cafe <b>& guest"), 71, true, true, 2412,
         QStringLiteral("BSSID_SECRET"), QStringLiteral("/ap/secret")},
        {QStringLiteral("Open yard"), 66, false, false, 5180,
         QStringLiteral("open-bssid"), QStringLiteral("/ap/open")},
        {QStringLiteral("Secure 6 GHz"), 92, false, true, 6000,
         QStringLiteral("secure-bssid"), QStringLiteral("/ap/secure")},
        {QString(), 100, false, true, 2412,
         QStringLiteral("hidden-bssid"), QStringLiteral("/ap/unnamed")},
    };
    page.publish(snapshot);
    QCOMPARE(page.controller->network()->wifiModel()->rowCount(), 4);

    QTRY_VERIFY_WITH_TIMEOUT(
        rowFor(page, QStringLiteral("Cafe <b>& guest"))
            && rowFor(page, QStringLiteral("Open yard"))
            && rowFor(page, QStringLiteral("Secure 6 GHz")),
        3000);

    QQuickItem *active = rowFor(page, QStringLiteral("Cafe <b>& guest"));
    QQuickItem *open = rowFor(page, QStringLiteral("Open yard"));
    QQuickItem *secure = rowFor(page, QStringLiteral("Secure 6 GHz"));
    QVERIFY(active && open && secure);
    QVERIFY(active->y() < open->y());
    QVERIFY(rowText(page, active).contains(QStringLiteral("Connected")));
    QVERIFY(rowText(page, active).contains(QStringLiteral("Secured")));
    QVERIFY(rowText(page, active).contains(QStringLiteral("2.4 GHz")));
    QVERIFY(rowText(page, active).contains(QStringLiteral("Signal 71%")));
    QVERIFY(rowText(page, open).contains(QStringLiteral("Open")));
    QVERIFY(rowText(page, open).contains(QStringLiteral("5 GHz")));
    QVERIFY(rowText(page, secure).contains(QStringLiteral("6 GHz")));

    QQuickItem *ssid = nullptr;
    for (QQuickItem *item : page.named(QStringLiteral("availableNetworkSsid"))) {
        if (item->property("text").toString() == QStringLiteral("Cafe <b>& guest"))
            ssid = item;
    }
    QVERIFY(ssid);
    QCOMPARE(ssid->property("textFormat").toInt(), 0);

    QQuickItem *unnamed = rowFor(page, QString());
    QVERIFY(unnamed);
    QCOMPARE(unnamed->height(), 0.0);
    for (QQuickItem *item : page.all()) {
        if (item->metaObject()->indexOfProperty("text") < 0)
            continue;
        const QString text = item->property("text").toString();
        QVERIFY(!text.contains(QStringLiteral("BSSID_SECRET")));
        QVERIFY(!text.contains(QStringLiteral("/ap/secret")));
    }
}

void SettingsInternetQmlTest::dnsSectionProjectsModeProviderAndConnectionServers()
{
    Page page;
    QVERIFY2(page.create(), qPrintable(page.errors()));
    QVERIFY(page.controller->network()->start());
    auto snapshot = connectedNetwork();
    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Automatic;
    snapshot.activeIpv4DnsServers = {QStringLiteral("1.1.1.1"), QStringLiteral("1.0.0.1")};
    snapshot.activeIpv6DnsServers = {QStringLiteral("2606:4700:4700::1111")};
    page.publish(snapshot);

    QTRY_VERIFY_WITH_TIMEOUT(page.find(QStringLiteral("dnsSection")), 1000);
    QObject *mode = page.find(QStringLiteral("dnsModeSelector"));
    QObject *provider = page.find(QStringLiteral("dnsProviderValue"));
    QObject *active = page.find(QStringLiteral("dnsActiveServers"));
    QVERIFY(mode && provider && active);
    QCOMPARE(mode->property("label").toString(), QStringLiteral("Automatic"));
    QCOMPARE(provider->property("sublabel").toString(), QStringLiteral("Automatic"));
    QCOMPARE(active->property("sublabel").toString(),
             QStringLiteral("1.1.1.1\n1.0.0.1\n2606:4700:4700::1111"));
    QCOMPARE(active->property("sublabelTextFormat").toInt(), 0);

    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Custom;
    snapshot.configuredIpv4DnsServers = {QStringLiteral("1.1.1.1"), QStringLiteral("1.0.0.1")};
    snapshot.configuredIpv6DnsServers = {QStringLiteral("2606:4700:4700::1111"),
                                         QStringLiteral("2606:4700:4700::1001")};
    page.publish(snapshot);
    QCOMPARE(mode->property("label").toString(), QStringLiteral("Custom"));
    QCOMPARE(provider->property("sublabel").toString(), QStringLiteral("Cloudflare"));

    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Advanced;
    page.publish(snapshot);
    QCOMPARE(mode->property("label").toString(), QStringLiteral("Advanced"));
    QVERIFY(!page.find(QStringLiteral("dnsApplyButton"))->property("enabled").toBool());
}

void SettingsInternetQmlTest::dnsPresetDraftRequiresExplicitApplyAndShowsPartialSuccess()
{
    Page page;
    QVERIFY2(page.create(), qPrintable(page.errors()));
    QVERIFY(page.controller->network()->start());
    auto snapshot = connectedNetwork();
    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Automatic;
    page.publish(snapshot);

    QObject *mode = page.find(QStringLiteral("dnsModeSelector"));
    QObject *preset = page.find(QStringLiteral("dnsPresetSelector"));
    QObject *apply = page.find(QStringLiteral("dnsApplyButton"));
    QObject *input = page.find(QStringLiteral("dnsCustomInput"));
    QVERIFY(mode && preset && apply && input);
    QVERIFY(QMetaObject::invokeMethod(mode, "selected", Qt::DirectConnection, Q_ARG(int, 1)));
    page.events();
    QVERIFY(preset->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(preset, "selected", Qt::DirectConnection,
                                     Q_ARG(int, 0)));
    page.events();
    QCOMPARE(input->property("text").toString(),
             QStringLiteral("1.1.1.1\n1.0.0.1\n2606:4700:4700::1111\n2606:4700:4700::1001"));
    QVERIFY(page.fake->dnsRequests.isEmpty());
    QVERIFY(apply->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(apply, "clicked", Qt::DirectConnection));
    QCOMPARE(page.fake->dnsRequests.size(), 1);
    QCOMPARE(page.fake->dnsRequests.constFirst().mode, Astrea::System::NetworkDnsMode::Custom);
    QCOMPARE(page.fake->dnsRequests.constFirst().servers,
             (QStringList{QStringLiteral("1.1.1.1"), QStringLiteral("1.0.0.1"),
                          QStringLiteral("2606:4700:4700::1111"),
                          QStringLiteral("2606:4700:4700::1001")}));
    QVERIFY(page.controller->network()->dnsPending());
    const quint64 requestId = page.fake->dnsRequests.constFirst().requestId;
    page.fake->completeDns(requestId, Astrea::System::NetworkDnsOutcome::SavedNeedsReconnect);
    QTRY_VERIFY_WITH_TIMEOUT(!page.controller->network()->dnsPending(), 1000);
    QVERIFY(page.controller->network()->dnsNeedsReconnect());
    QCOMPARE(page.find(QStringLiteral("dnsOperationStatus"))->property("text").toString(),
             QStringLiteral("DNS settings were saved. Reconnect this network to apply them."));
}

void SettingsInternetQmlTest::dnsInvalidDraftAndExternalChangeRemainExplicit()
{
    Page page;
    QVERIFY2(page.create(), qPrintable(page.errors()));
    QVERIFY(page.controller->network()->start());
    auto snapshot = connectedNetwork();
    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Automatic;
    page.publish(snapshot);

    QObject *mode = page.find(QStringLiteral("dnsModeSelector"));
    QObject *preset = page.find(QStringLiteral("dnsPresetSelector"));
    QObject *input = page.find(QStringLiteral("dnsCustomInput"));
    QObject *apply = page.find(QStringLiteral("dnsApplyButton"));
    QVERIFY(mode && preset && input && apply);
    QVERIFY(QMetaObject::invokeMethod(mode, "selected", Qt::DirectConnection, Q_ARG(int, 1)));
    page.events();
    QVERIFY(QMetaObject::invokeMethod(preset, "selected", Qt::DirectConnection,
                                     Q_ARG(int, 3)));
    page.events();
    input->setProperty("text", QStringLiteral("resolver.example"));
    page.events();
    QVERIFY(page.find(QStringLiteral("dnsValidationError"))->property("visible").toBool());
    QVERIFY(!apply->property("enabled").toBool());
    QVERIFY(page.fake->dnsRequests.isEmpty());

    input->setProperty("text", QStringLiteral("1.1.1.1, 2606:4700:4700::1111"));
    page.events();
    QVERIFY(apply->property("enabled").toBool());
    const QString draftBeforeExternalUpdate = input->property("text").toString();
    snapshot.activeConnectionPath = QStringLiteral("/active/2");
    snapshot.profilePath = QStringLiteral("/settings/2");
    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Custom;
    snapshot.configuredIpv4DnsServers = {QStringLiteral("9.9.9.9")};
    snapshot.configuredIpv6DnsServers = {QStringLiteral("2620:fe::fe")};
    page.publish(snapshot);
    QCOMPARE(input->property("text").toString(), draftBeforeExternalUpdate);
    QVERIFY(page.find(QStringLiteral("dnsExternalChangeWarning"))->property("visible").toBool());
    QVERIFY(!apply->property("enabled").toBool());
    QVERIFY(page.fake->dnsRequests.isEmpty());

    QObject *reload = page.find(QStringLiteral("dnsReloadButton"));
    QVERIFY(reload && reload->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(reload, "clicked", Qt::DirectConnection));
    page.events();
    QVERIFY(!page.find(QStringLiteral("dnsExternalChangeWarning"))->property("visible").toBool());
    QCOMPARE(input->property("text").toString(),
             QStringLiteral("9.9.9.9\n2620:fe::fe"));
}

void SettingsInternetQmlTest::dnsOwnRefreshChurnDoesNotMarkDraftExternal()
{
    for (const auto outcome : {Astrea::System::NetworkDnsOutcome::Applied,
                               Astrea::System::NetworkDnsOutcome::SavedNeedsReconnect}) {
        Page page;
        QVERIFY2(page.create(), qPrintable(page.errors()));
        QVERIFY(page.controller->network()->start());
        auto snapshot = connectedNetwork();
        snapshot.dnsMode = Astrea::System::NetworkDnsMode::Automatic;
        page.publish(snapshot);

        QObject *mode = page.find(QStringLiteral("dnsModeSelector"));
        QObject *preset = page.find(QStringLiteral("dnsPresetSelector"));
        QObject *input = page.find(QStringLiteral("dnsCustomInput"));
        QObject *apply = page.find(QStringLiteral("dnsApplyButton"));
        QVERIFY(mode && preset && input && apply);
        QVERIFY(QMetaObject::invokeMethod(mode, "selected", Qt::DirectConnection,
                                         Q_ARG(int, 1)));
        QVERIFY(QMetaObject::invokeMethod(preset, "selected", Qt::DirectConnection,
                                         Q_ARG(int, 0)));
        page.events();
        const QString expectedDraft = input->property("text").toString();
        QVERIFY(apply->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(apply, "clicked", Qt::DirectConnection));
        QCOMPARE(page.fake->dnsRequests.size(), 1);
        const quint64 requestId = page.fake->dnsRequests.constFirst().requestId;
        QVERIFY(page.controller->network()->dnsPending());

        snapshot.dnsMode = Astrea::System::NetworkDnsMode::Advanced;
        snapshot.configuredIpv4DnsServers.clear();
        snapshot.configuredIpv6DnsServers.clear();
        snapshot.dnsEditable = false;
        page.publish(snapshot);
        QVERIFY(page.controller->network()->dnsPending());
        QCOMPARE(input->property("text").toString(), expectedDraft);
        QVERIFY(!page.find(QStringLiteral("dnsExternalChangeWarning"))
                     ->property("visible").toBool());

        snapshot.dnsMode = Astrea::System::NetworkDnsMode::Custom;
        snapshot.configuredIpv4DnsServers = {QStringLiteral("1.1.1.1"),
                                             QStringLiteral("1.0.0.1")};
        snapshot.configuredIpv6DnsServers = {QStringLiteral("2606:4700:4700::1111"),
                                             QStringLiteral("2606:4700:4700::1001")};
        snapshot.dnsEditable = true;
        page.publish(snapshot);
        QVERIFY(page.controller->network()->dnsPending());
        QCOMPARE(input->property("text").toString(), expectedDraft);
        QVERIFY(!page.find(QStringLiteral("dnsExternalChangeWarning"))
                     ->property("visible").toBool());

        page.fake->completeDns(requestId, outcome);
        QTRY_VERIFY_WITH_TIMEOUT(!page.controller->network()->dnsPending(), 1000);
        QCOMPARE(input->property("text").toString(), expectedDraft);
        QVERIFY(!page.root->property("dnsDraftDirty").toBool());
        QVERIFY(!page.root->property("dnsAwaitingAuthoritative").toBool());
        QVERIFY(!page.root->property("dnsExternalChanged").toBool());
        if (outcome == Astrea::System::NetworkDnsOutcome::Applied) {
            QCOMPARE(page.find(QStringLiteral("dnsOperationStatus"))->property("text").toString(),
                     QStringLiteral("DNS settings updated for this connection."));
            QVERIFY(!page.controller->network()->dnsNeedsReconnect());
        } else {
            QCOMPARE(page.find(QStringLiteral("dnsOperationStatus"))->property("text").toString(),
                     QStringLiteral("DNS settings were saved. Reconnect this network to apply them."));
            QVERIFY(page.controller->network()->dnsNeedsReconnect());
        }
    }
}

void SettingsInternetQmlTest::dnsFingerprintUsesOpaqueRevisionAndDnsState()
{
    Page page;
    QVERIFY2(page.create(), qPrintable(page.errors()));
    QVERIFY(page.controller->network()->start());
    auto snapshot = connectedNetwork();
    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Automatic;
    page.publish(snapshot);

    const auto fingerprint = [&page] {
        QVariant result;
        const bool invoked = QMetaObject::invokeMethod(
            page.root, "dnsAuthorityFingerprint", Qt::DirectConnection,
            Q_RETURN_ARG(QVariant, result));
        return invoked ? result.toString() : QString();
    };
    const quint64 revision = page.controller->network()
        ->property("dnsAuthorityRevision").toULongLong();
    const QString automatic = fingerprint();
    QVERIFY(!automatic.isEmpty());
    QVERIFY(automatic.startsWith(QString::number(revision) + QLatin1Char('|')));
    QVERIFY(!automatic.contains(QStringLiteral("/active/1")));
    QVERIFY(!automatic.contains(QStringLiteral("/settings/1")));

    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Custom;
    snapshot.configuredIpv4DnsServers = {QStringLiteral("9.9.9.9")};
    snapshot.dnsEditable = true;
    page.publish(snapshot);
    const QString semanticEdit = fingerprint();
    QCOMPARE(page.controller->network()->property("dnsAuthorityRevision").toULongLong(),
             revision);
    QVERIFY(semanticEdit != automatic);

    snapshot.activeConnectionPath = QStringLiteral("/active/2");
    page.publish(snapshot);
    const QString newAuthority = fingerprint();
    QVERIFY(newAuthority != semanticEdit);
    QVERIFY(newAuthority.startsWith(QString::number(revision + 1) + QLatin1Char('|')));
    QVERIFY(!newAuthority.contains(QStringLiteral("/active/2")));
}

void SettingsInternetQmlTest::dnsAuthoritySwitchLoadsNewDraftAfterPartialSuccess()
{
    Page page;
    QVERIFY2(page.create(), qPrintable(page.errors()));
    QVERIFY(page.controller->network()->start());
    auto snapshot = connectedNetwork();
    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Automatic;
    page.publish(snapshot);
    QObject *mode = page.find(QStringLiteral("dnsModeSelector"));
    QObject *preset = page.find(QStringLiteral("dnsPresetSelector"));
    QObject *input = page.find(QStringLiteral("dnsCustomInput"));
    QObject *apply = page.find(QStringLiteral("dnsApplyButton"));
    QVERIFY(mode && preset && input && apply);
    QVERIFY(QMetaObject::invokeMethod(mode, "selected", Qt::DirectConnection, Q_ARG(int, 1)));
    QVERIFY(QMetaObject::invokeMethod(preset, "selected", Qt::DirectConnection, Q_ARG(int, 0)));
    page.events();
    const QString oldDraft = input->property("text").toString();
    QVERIFY(QMetaObject::invokeMethod(apply, "clicked", Qt::DirectConnection));
    const quint64 requestId = page.fake->dnsRequests.constFirst().requestId;

    snapshot.activeConnectionPath = QStringLiteral("/active/2");
    snapshot.profilePath = QStringLiteral("/settings/2");
    snapshot.devicePath = QStringLiteral("/device/ethernet1");
    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Custom;
    snapshot.configuredIpv4DnsServers = {QStringLiteral("9.9.9.9")};
    snapshot.configuredIpv6DnsServers = {QStringLiteral("2620:fe::fe")};
    page.publish(snapshot);
    QVERIFY(page.controller->network()->dnsPending());
    QCOMPARE(input->property("text").toString(), oldDraft);

    page.fake->completeDns(requestId, Astrea::System::NetworkDnsOutcome::SavedNeedsReconnect,
                           {}, false);
    QTRY_VERIFY_WITH_TIMEOUT(!page.controller->network()->dnsPending(), 1000);
    QCOMPARE(input->property("text").toString(),
             QStringLiteral("9.9.9.9\n2620:fe::fe"));
    QVERIFY(!page.root->property("dnsDraftDirty").toBool());
    QVERIFY(!page.root->property("dnsExternalChanged").toBool());
    QVERIFY(!page.root->property("dnsAwaitingAuthoritative").toBool());
    QCOMPARE(page.find(QStringLiteral("dnsOperationStatus"))->property("text").toString(),
             QStringLiteral("DNS settings were saved for the previous connection and will apply the next time it connects."));
    QVERIFY(!page.controller->network()->dnsNeedsReconnect());
    QVERIFY(!page.root->property("dnsDraftDirty").toBool());
    QCOMPARE(page.fake->dnsRequests.size(), 1);
    input->setProperty("text", QStringLiteral("8.8.8.8"));
    page.events();
    QVERIFY(page.root->property("dnsDraftDirty").toBool());
    QVERIFY(apply->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(apply, "clicked", Qt::DirectConnection));
    QCOMPARE(page.fake->dnsRequests.size(), 2);
}

void SettingsInternetQmlTest::dnsConflictPreservesDraftForExplicitResolution()
{
    Page page;
    QVERIFY2(page.create(), qPrintable(page.errors()));
    QVERIFY(page.controller->network()->start());
    auto snapshot = connectedNetwork();
    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Automatic;
    page.publish(snapshot);
    QObject *mode = page.find(QStringLiteral("dnsModeSelector"));
    QObject *preset = page.find(QStringLiteral("dnsPresetSelector"));
    QObject *input = page.find(QStringLiteral("dnsCustomInput"));
    QObject *apply = page.find(QStringLiteral("dnsApplyButton"));
    QVERIFY(mode && preset && input && apply);
    QVERIFY(QMetaObject::invokeMethod(mode, "selected", Qt::DirectConnection, Q_ARG(int, 1)));
    QVERIFY(QMetaObject::invokeMethod(preset, "selected", Qt::DirectConnection, Q_ARG(int, 0)));
    page.events();
    const QString draft = input->property("text").toString();
    QVERIFY(QMetaObject::invokeMethod(apply, "clicked", Qt::DirectConnection));
    const quint64 requestId = page.fake->dnsRequests.constFirst().requestId;
    page.fake->completeDns(requestId, Astrea::System::NetworkDnsOutcome::Conflict);
    QTRY_VERIFY_WITH_TIMEOUT(!page.controller->network()->dnsPending(), 1000);

    QCOMPARE(input->property("text").toString(), draft);
    QVERIFY(page.root->property("dnsDraftDirty").toBool());
    QVERIFY(page.root->property("dnsExternalChanged").toBool());
    QVERIFY(page.find(QStringLiteral("dnsExternalChangeWarning"))->property("visible").toBool());
    QVERIFY(!apply->property("enabled").toBool());
    QVERIFY(page.fake->dnsRequests.size() == 1);
}

void SettingsInternetQmlTest::dnsIndeterminateRequiresAuthoritativeReload()
{
    Page page;
    QVERIFY2(page.create(), qPrintable(page.errors()));
    QVERIFY(page.controller->network()->start());
    auto snapshot = connectedNetwork();
    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Automatic;
    page.publish(snapshot);
    QObject *mode = page.find(QStringLiteral("dnsModeSelector"));
    QObject *preset = page.find(QStringLiteral("dnsPresetSelector"));
    QObject *input = page.find(QStringLiteral("dnsCustomInput"));
    QObject *apply = page.find(QStringLiteral("dnsApplyButton"));
    QVERIFY(mode && preset && input && apply);
    QVERIFY(QMetaObject::invokeMethod(mode, "selected", Qt::DirectConnection, Q_ARG(int, 1)));
    QVERIFY(QMetaObject::invokeMethod(preset, "selected", Qt::DirectConnection, Q_ARG(int, 0)));
    page.events();
    const QString draft = input->property("text").toString();
    QVERIFY(QMetaObject::invokeMethod(apply, "clicked", Qt::DirectConnection));
    const quint64 requestId = page.fake->dnsRequests.constFirst().requestId;
    page.fake->completeDns(requestId, Astrea::System::NetworkDnsOutcome::Indeterminate);
    QTRY_VERIFY_WITH_TIMEOUT(!page.controller->network()->dnsPending(), 1000);

    QCOMPARE(input->property("text").toString(), draft);
    QVERIFY(page.root->property("dnsDraftDirty").toBool());
    QVERIFY(!page.controller->network()->dnsNeedsReconnect());
    QVERIFY(!apply->property("enabled").toBool());
    QCOMPARE(page.find(QStringLiteral("dnsOperationStatus"))->property("text").toString(),
             QStringLiteral("Astrea could not confirm whether the DNS settings were saved. Reload the current connection settings before trying again."));
    QCOMPARE(page.fake->dnsRequests.size(), 1);
    QObject *reload = page.find(QStringLiteral("dnsReloadButton"));
    QVERIFY(reload && reload->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(reload, "clicked", Qt::DirectConnection));
    page.events();
    QVERIFY(!page.root->property("dnsReloadRequired").toBool());
    QVERIFY(!page.root->property("dnsDraftDirty").toBool());
    QCOMPARE(page.fake->dnsRequests.size(), 1);
}

void SettingsInternetQmlTest::dnsFailedOutcomeKeepsDraftRetryable()
{
    Page page;
    QVERIFY2(page.create(), qPrintable(page.errors()));
    QVERIFY(page.controller->network()->start());
    auto snapshot = connectedNetwork();
    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Automatic;
    page.publish(snapshot);
    QObject *mode = page.find(QStringLiteral("dnsModeSelector"));
    QObject *preset = page.find(QStringLiteral("dnsPresetSelector"));
    QObject *input = page.find(QStringLiteral("dnsCustomInput"));
    QObject *apply = page.find(QStringLiteral("dnsApplyButton"));
    QVERIFY(mode && preset && input && apply);
    QVERIFY(QMetaObject::invokeMethod(mode, "selected", Qt::DirectConnection, Q_ARG(int, 1)));
    QVERIFY(QMetaObject::invokeMethod(preset, "selected", Qt::DirectConnection, Q_ARG(int, 0)));
    page.events();
    const QString draft = input->property("text").toString();
    QVERIFY(QMetaObject::invokeMethod(apply, "clicked", Qt::DirectConnection));
    const quint64 requestId = page.fake->dnsRequests.constFirst().requestId;
    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Advanced;
    snapshot.configuredIpv4DnsServers.clear();
    snapshot.configuredIpv6DnsServers.clear();
    snapshot.dnsEditable = false;
    page.publish(snapshot);
    QVERIFY(page.controller->network()->dnsPending());
    page.fake->completeDns(requestId, Astrea::System::NetworkDnsOutcome::Failed,
                           QStringLiteral("Could not save DNS settings"));
    QTRY_VERIFY_WITH_TIMEOUT(!page.controller->network()->dnsPending(), 1000);

    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Automatic;
    snapshot.dnsEditable = true;
    page.publish(snapshot);

    QCOMPARE(input->property("text").toString(), draft);
    QVERIFY(page.root->property("dnsDraftDirty").toBool());
    QVERIFY(!page.root->property("dnsExternalChanged").toBool());
    QVERIFY(!page.controller->network()->dnsNeedsReconnect());
    QCOMPARE(page.controller->network()->state(), Astrea::System::SystemServiceState::Ready);
    QVERIFY(apply->property("enabled").toBool());
    QCOMPARE(page.find(QStringLiteral("dnsOperationStatus"))->property("text").toString(),
             QStringLiteral("Could not save DNS settings"));
}

void SettingsInternetQmlTest::dnsSlowStateRemainsPendingInPresentation()
{
    Page page;
    QVERIFY2(page.create(), qPrintable(page.errors()));
    QVERIFY(page.controller->network()->start());
    auto snapshot = connectedNetwork();
    snapshot.dnsMode = Astrea::System::NetworkDnsMode::Automatic;
    page.publish(snapshot);
    QObject *mode = page.find(QStringLiteral("dnsModeSelector"));
    QObject *preset = page.find(QStringLiteral("dnsPresetSelector"));
    QObject *apply = page.find(QStringLiteral("dnsApplyButton"));
    QVERIFY(mode && preset && apply);
    QVERIFY(QMetaObject::invokeMethod(mode, "selected", Qt::DirectConnection, Q_ARG(int, 1)));
    QVERIFY(QMetaObject::invokeMethod(preset, "selected", Qt::DirectConnection, Q_ARG(int, 0)));
    page.events();
    QVERIFY(QMetaObject::invokeMethod(apply, "clicked", Qt::DirectConnection));
    const quint64 requestId = page.fake->dnsRequests.constFirst().requestId;
    QTRY_VERIFY_WITH_TIMEOUT(page.controller->network()
                                 ->property("dnsOperationSlow").toBool(), 7000);
    QVERIFY(page.controller->network()->dnsPending());
    QVERIFY(!apply->property("enabled").toBool());
    QCOMPARE(page.find(QStringLiteral("dnsOperationStatus"))->property("text").toString(),
             QStringLiteral("NetworkManager is taking longer than expected. The update is still in progress…"));
    page.fake->completeDns(requestId, Astrea::System::NetworkDnsOutcome::Applied);
    QTRY_VERIFY_WITH_TIMEOUT(!page.controller->network()->dnsPending(), 1000);
    QVERIFY(!page.controller->network()->property("dnsOperationSlow").toBool());
}

QTEST_MAIN(SettingsInternetQmlTest)
#include "SettingsInternetQmlTest.moc"
