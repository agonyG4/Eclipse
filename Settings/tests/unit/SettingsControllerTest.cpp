#include "core/SettingsController.hpp"
#include "core/navigation/SettingsNavigationModel.hpp"
#include "system/bluetooth/BluetoothBackend.hpp"
#include "system/bluetooth/BluetoothService.hpp"

#include <QSignalSpy>
#include <QtTest>

namespace {

class ControllerBluetoothBackend final : public Astrea::System::BluetoothBackend {
public:
    bool start(const Callbacks &, QString *) override
    {
        ++startCount;
        return true;
    }
    void stop() override {}
    bool setPowered(bool) override { return true; }
    bool requestScan(const QString &) override { return true; }
    void releaseScan(const QString &) override {}
    bool connectDevice(const QString &) override { return true; }
    bool disconnectDevice(const QString &) override { return true; }
    bool pairDevice(const QString &) override { return true; }
    bool cancelPairing() override { return true; }
    bool setDeviceTrusted(const QString &, bool) override { return true; }
    bool forgetDevice(const QString &) override { return true; }
    bool submitAgentText(quint64, const QString &) override { return true; }
    bool confirmAgentRequest(quint64, bool) override { return true; }
    bool rejectAgentRequest(quint64) override { return true; }

    int startCount = 0;
};

} // namespace

class SettingsControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithPreferredCompositorDestination();
    void navigatesToBluetoothPage();
    void navigatesToCustomizationHub();
    void nestedDestinationsKeepSidebarAncestorSelected();
    void directNestedNavigationDerivesSidebarAncestor();
    void backAndForwardTraverseSessionHistory();
    void divergentNavigationClearsForwardHistory();
    void currentDestinationDoesNotDuplicateHistory();
    void invalidNavigationPreservesRouteAndHistory();
    void historyRemainsBounded();
    void usesInjectedProfileForIsSudo();
    void ownsBluetoothServiceWithoutStartingIt();
    void preservesInjectedBluetoothServiceWithoutStartingIt();
    void exposesRustBackedVisualEffectsController();
};

void SettingsControllerTest::startsWithPreferredCompositorDestination()
{
    SettingsController controller;

    QCOMPARE(controller.currentDestinationId(), QStringLiteral("compositor"));
    QCOMPARE(controller.selectedSidebarId(), QStringLiteral("compositor"));
    QCOMPARE(controller.selectedPageSource(),
             QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Compositor.qml")));
    QVERIFY(!controller.canGoBack());
    QVERIFY(!controller.canGoForward());
}

void SettingsControllerTest::navigatesToBluetoothPage()
{
    SettingsController controller;

    QVERIFY(controller.navigateTo(QStringLiteral("bluetooth")));
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("bluetooth"));
    QCOMPARE(controller.selectedSidebarId(), QStringLiteral("bluetooth"));
    QCOMPARE(controller.selectedPageSource(),
             QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Bluetooth.qml")));
}

void SettingsControllerTest::navigatesToCustomizationHub()
{
    SettingsController controller;
    QSignalSpy navigationSpy(&controller, &SettingsController::navigationChanged);

    QVERIFY(controller.navigateTo(QStringLiteral("customization")));
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("customization"));
    QCOMPARE(controller.selectedSidebarId(), QStringLiteral("customization"));
    QCOMPARE(controller.selectedPageSource(),
             QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/navigation/Hub.qml")));
    QCOMPARE(controller.currentDestination().value(QStringLiteral("kind")).toString(), QStringLiteral("hub"));
    QCOMPARE(controller.currentDestinationChildren().size(), 6);
    QVERIFY(controller.navigateTo(QStringLiteral("animations")));
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("animations"));
    QCOMPARE(controller.selectedSidebarId(), QStringLiteral("customization"));
    QCOMPARE(navigationSpy.count(), 2);
    QVERIFY(controller.canGoBack());
    QVERIFY(!controller.canGoForward());
}

void SettingsControllerTest::nestedDestinationsKeepSidebarAncestorSelected()
{
    SettingsController controller;

    QVERIFY(controller.navigateTo(QStringLiteral("customization")));
    QVERIFY(controller.navigateTo(QStringLiteral("dock")));
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("dock"));
    QCOMPARE(controller.selectedSidebarId(), QStringLiteral("customization"));
    QCOMPARE(controller.selectedPageSource(),
             QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/Dock.qml")));

    QVERIFY(controller.navigateTo(QStringLiteral("wallpaper")));
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("wallpaper"));
    QCOMPARE(controller.selectedSidebarId(), QStringLiteral("customization"));

    QVERIFY(controller.navigateTo(QStringLiteral("visual-effects")));
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("visual-effects"));
    QCOMPARE(controller.selectedSidebarId(), QStringLiteral("customization"));
    QVERIFY(controller.navigateTo(QStringLiteral("icons")));
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("icons"));
    QCOMPARE(controller.selectedSidebarId(), QStringLiteral("customization"));
    QVERIFY(!controller.navigateTo(QStringLiteral("themes")));
    controller.goBack();
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("visual-effects"));
    controller.goForward();
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("icons"));
}

void SettingsControllerTest::directNestedNavigationDerivesSidebarAncestor()
{
    SettingsController controller;

    QVERIFY(controller.navigateTo(QStringLiteral("dock")));
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("dock"));
    QCOMPARE(controller.selectedSidebarId(), QStringLiteral("customization"));
}

void SettingsControllerTest::backAndForwardTraverseSessionHistory()
{
    SettingsController controller;
    QSignalSpy navigationSpy(&controller, &SettingsController::navigationChanged);

    QVERIFY(controller.navigateTo(QStringLiteral("customization")));
    QVERIFY(controller.navigateTo(QStringLiteral("dock")));
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("dock"));

    controller.goBack();
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("customization"));
    QCOMPARE(controller.selectedSidebarId(), QStringLiteral("customization"));
    QVERIFY(controller.canGoForward());

    controller.goForward();
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("dock"));
    QCOMPARE(controller.selectedSidebarId(), QStringLiteral("customization"));
    QVERIFY(!controller.canGoForward());
    QCOMPARE(navigationSpy.count(), 4);
}

void SettingsControllerTest::divergentNavigationClearsForwardHistory()
{
    SettingsController controller;

    QVERIFY(controller.navigateTo(QStringLiteral("customization")));
    QVERIFY(controller.navigateTo(QStringLiteral("dock")));
    controller.goBack();
    QVERIFY(controller.canGoForward());

    QVERIFY(controller.navigateTo(QStringLiteral("wallpaper")));
    QVERIFY(!controller.canGoForward());
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("wallpaper"));
    controller.goForward();
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("wallpaper"));
}

void SettingsControllerTest::currentDestinationDoesNotDuplicateHistory()
{
    SettingsController controller;
    QSignalSpy navigationSpy(&controller, &SettingsController::navigationChanged);
    QSignalSpy historySpy(&controller, &SettingsController::historyChanged);

    QVERIFY(controller.navigateTo(QStringLiteral("customization")));
    QCOMPARE(navigationSpy.count(), 1);
    QCOMPARE(historySpy.count(), 1);

    QVERIFY(controller.navigateTo(QStringLiteral("customization")));
    QCOMPARE(navigationSpy.count(), 1);
    QCOMPARE(historySpy.count(), 1);

    controller.goBack();
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("compositor"));
    QVERIFY(!controller.canGoBack());
    QVERIFY(controller.canGoForward());
}

void SettingsControllerTest::invalidNavigationPreservesRouteAndHistory()
{
    SettingsController controller;
    QSignalSpy navigationSpy(&controller, &SettingsController::navigationChanged);

    QVERIFY(controller.navigateTo(QStringLiteral("customization")));
    QVERIFY(controller.navigateTo(QStringLiteral("dock")));
    const qsizetype signalCount = navigationSpy.count();

    QVERIFY(!controller.navigateTo(QStringLiteral("missing")));
    QVERIFY(!controller.navigateTo(QStringLiteral("performance")));
    QVERIFY(!controller.navigateTo(QStringLiteral("more-settings")));
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("dock"));
    QCOMPARE(controller.selectedSidebarId(), QStringLiteral("customization"));
    QCOMPARE(navigationSpy.count(), signalCount);
    QVERIFY(!controller.canGoForward());
}

void SettingsControllerTest::historyRemainsBounded()
{
    SettingsController controller;

    for (int i = 0; i < 80; ++i)
        QVERIFY(controller.navigateTo(i % 2 == 0 ? QStringLiteral("customization") : QStringLiteral("dock")));

    int backSteps = 0;
    while (controller.canGoBack()) {
        controller.goBack();
        ++backSteps;
    }
    QCOMPARE(backSteps, 63);
    QCOMPARE(controller.currentDestinationId(), QStringLiteral("customization"));
    QVERIFY(!controller.canGoBack());
}

void SettingsControllerTest::usesInjectedProfileForIsSudo()
{
    SettingsUserProfile profile;
    profile.userName = QStringLiteral("test-user");
    profile.administrator = true;
    SettingsController controller{profile};

    QVERIFY(controller.isSudo());
    QVERIFY(controller.property("isSudo").toBool());
}

void SettingsControllerTest::ownsBluetoothServiceWithoutStartingIt()
{
    SettingsController controller;

    QVERIFY(controller.bluetooth() != nullptr);
    QCOMPARE(controller.bluetooth()->state(), Astrea::System::SystemServiceState::Stopped);
    QCOMPARE(controller.property("bluetooth").value<Astrea::System::BluetoothService *>(),
             controller.bluetooth());
}

void SettingsControllerTest::preservesInjectedBluetoothServiceWithoutStartingIt()
{
    auto backend = std::make_unique<ControllerBluetoothBackend>();
    ControllerBluetoothBackend *backendPointer = backend.get();
    auto service = std::make_unique<Astrea::System::BluetoothService>(std::move(backend));
    Astrea::System::BluetoothService *servicePointer = service.get();
    auto navigationModel = std::make_unique<SettingsNavigationModel>();

    SettingsController controller(std::move(navigationModel), {}, {}, std::move(service));

    QCOMPARE(controller.bluetooth(), servicePointer);
    QCOMPARE(backendPointer->startCount, 0);
    QCOMPARE(controller.bluetooth()->state(), Astrea::System::SystemServiceState::Stopped);
}

void SettingsControllerTest::exposesRustBackedVisualEffectsController()
{
    SettingsController controller;

    QVERIFY(controller.visualEffects() != nullptr);
    QCOMPARE(controller.property("visualEffects")
                 .value<SettingsVisualEffectsController *>(),
             controller.visualEffects());
    QCOMPARE(QString::fromLatin1(controller.visualEffects()->metaObject()->className()),
             QStringLiteral("SettingsVisualEffectsController"));
}

QTEST_MAIN(SettingsControllerTest)
#include "SettingsControllerTest.moc"
