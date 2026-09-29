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

    bool startResult = true;
    bool actionResult = true;
    int starts = 0;
    int stops = 0;
    int scans = 0;
    QList<QPair<bool, quint64>> powerRequests;

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

} // namespace

class SettingsInternetQmlTest final : public QObject {
    Q_OBJECT
private slots:
    void serviceHealthAndConnectionProjection();
    void wifiPowerAndScanAreAuthoritative();
    void availableNetworksRemainInformationalPlainTextRows();
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

QTEST_MAIN(SettingsInternetQmlTest)
#include "SettingsInternetQmlTest.moc"
