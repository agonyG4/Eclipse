#include "core/SettingsController.hpp"
#include "system/SystemServiceState.hpp"
#include "system/bluetooth/BluetoothBackend.hpp"
#include "system/bluetooth/BluetoothService.hpp"
#include "theme/ThemeController.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml>
#include <QtTest>

#include <memory>

namespace {

class FakeI18n final : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE QString tr(const QString &, const QString &fallback) const { return fallback; }
};

class PageBluetoothBackend final : public Astrea::System::BluetoothBackend {
public:
    bool start(const Callbacks &callbacks, QString *) override
    {
        m_callbacks = callbacks;
        ++startCount;
        return true;
    }
    void stop() override { ++stopCount; }
    bool setPowered(bool value) override
    {
        poweredTargets.append(value);
        return true;
    }
    bool requestScan(const QString &owner) override
    {
        scanRequests.append(owner);
        return true;
    }
    void releaseScan(const QString &owner) override { scanReleases.append(owner); }
    bool connectDevice(const QString &path) override
    {
        connectedPaths.append(path);
        return true;
    }
    bool disconnectDevice(const QString &path) override
    {
        disconnectedPaths.append(path);
        return true;
    }
    bool pairDevice(const QString &path) override
    {
        pairedPaths.append(path);
        return true;
    }
    bool cancelPairing() override
    {
        ++cancelPairingCount;
        return true;
    }
    bool setDeviceTrusted(const QString &path, bool trusted) override
    {
        trustedPaths.append(path);
        trustedTargets.append(trusted);
        return true;
    }
    bool forgetDevice(const QString &path) override
    {
        forgottenPaths.append(path);
        return true;
    }
    bool submitAgentText(quint64 requestId, const QString &text) override
    {
        if (!m_agentRequestActive || requestId != m_agentRequestId)
            return false;
        submittedRequestIds.append(requestId);
        submittedTexts.append(text);
        return true;
    }
    bool confirmAgentRequest(quint64 requestId, bool accepted) override
    {
        if (!m_agentRequestActive || requestId != m_agentRequestId)
            return false;
        confirmedRequestIds.append(requestId);
        confirmationValues.append(accepted);
        return true;
    }
    bool rejectAgentRequest(quint64 requestId) override
    {
        if (!m_agentRequestActive || requestId != m_agentRequestId)
            return false;
        rejectedRequestIds.append(requestId);
        return true;
    }

    void publish(const Astrea::System::BluetoothSnapshot &snapshot)
    {
        m_agentRequestActive = snapshot.agentRequestActive;
        m_agentRequestId = snapshot.agentRequestId;
        if (m_callbacks.snapshotChanged)
            m_callbacks.snapshotChanged(snapshot);
    }

    int startCount = 0;
    int stopCount = 0;
    int cancelPairingCount = 0;
    QList<bool> poweredTargets;
    QStringList scanRequests;
    QStringList scanReleases;
    QStringList connectedPaths;
    QStringList disconnectedPaths;
    QStringList pairedPaths;
    QStringList trustedPaths;
    QList<bool> trustedTargets;
    QStringList forgottenPaths;
    QList<quint64> submittedRequestIds;
    QStringList submittedTexts;
    QList<quint64> confirmedRequestIds;
    QList<bool> confirmationValues;
    QList<quint64> rejectedRequestIds;

private:
    Callbacks m_callbacks;
    bool m_agentRequestActive = false;
    quint64 m_agentRequestId = 0;
};

void registerBluetoothEnums()
{
    static const int registration = qmlRegisterUncreatableMetaObject(
        Astrea::System::staticMetaObject, "Astrea.System", 1, 0, "System",
        QStringLiteral("Astrea.System contains shared system enums"));
    Q_UNUSED(registration)
}

class BluetoothPageFixture final {
public:
    BluetoothPageFixture()
    {
        registerBluetoothEnums();
        auto backend = std::make_unique<PageBluetoothBackend>();
        m_backend = backend.get();
        auto service = std::make_unique<Astrea::System::BluetoothService>(std::move(backend));
        m_controller = std::make_unique<SettingsController>(
            std::make_unique<SettingsNavigationModel>(), SettingsUserProfile{},
            SettingsIconResolver{}, std::move(service));
        m_controller->bluetooth()->start();

        m_engine.addImportPath(QStringLiteral("qrc:/"));
        m_engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"),
                                                   m_controller.get());
        m_engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &m_i18n);
        m_engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &m_theme);
        m_component = std::make_unique<QQmlComponent>(
            &m_engine,
            QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/system/Bluetooth.qml")));
        m_window.resize(1000, 760);
    }

    ~BluetoothPageFixture()
    {
        destroyPage();
    }

    QQuickItem *createPage()
    {
        QObject *object = m_component->create();
        if (!object)
            return nullptr;
        m_page = qobject_cast<QQuickItem *>(object);
        if (!m_page) {
            delete object;
            return nullptr;
        }
        m_page->setParentItem(m_window.contentItem());
        m_page->setSize(m_window.size());
        m_window.show();
        QCoreApplication::processEvents();
        return m_page;
    }

    QString componentErrors() const { return m_component->errorString(); }
    PageBluetoothBackend *backend() const { return m_backend; }
    SettingsController *controller() const { return m_controller.get(); }
    QQuickItem *page() const { return m_page; }
    QQuickWindow *window() { return &m_window; }
    QObject *findObject(const QString &objectName) const
    {
        if (!m_page)
            return nullptr;
        if (QObject *object = m_page->findChild<QObject *>(objectName))
            return object;
        return findVisualObject(m_page, objectName);
    }
    void publish(const Astrea::System::BluetoothSnapshot &snapshot)
    {
        m_backend->publish(snapshot);
        QCoreApplication::processEvents();
    }
    void destroyPage()
    {
        if (!m_page)
            return;
        m_page->setParentItem(nullptr);
        delete m_page;
        m_page = nullptr;
    }

private:
    static QObject *findVisualObject(QQuickItem *item, const QString &objectName)
    {
        if (!item)
            return nullptr;
        if (item->objectName() == objectName)
            return item;
        for (QQuickItem *child : item->childItems()) {
            if (QObject *match = findVisualObject(child, objectName))
                return match;
        }
        return nullptr;
    }

    FakeI18n m_i18n;
    ThemeController m_theme;
    std::unique_ptr<SettingsController> m_controller;
    QQmlApplicationEngine m_engine;
    std::unique_ptr<QQmlComponent> m_component;
    QQuickWindow m_window;
    QQuickItem *m_page = nullptr;
    PageBluetoothBackend *m_backend = nullptr;
};

} // namespace

class SettingsBluetoothQmlTest final : public QObject {
    Q_OBJECT

private slots:
    void pageLoadsOffscreenWithTypedBluetoothEnums();
    void showsBluetoothAvailabilityStatesSeparately();
    void powerToggleUsesAuthoritativeStateAndRespectsPendingAndPairing();
    void ownsOneScanLeaseUntilPageDestructionWhilePoweredOff();
    void pageDestructionCancelsActivePairing();
    void presentsSharedDevicesAndDispatchesActions();
    void successfulPairingMovesWithAuthoritativeModelWithoutChaining();
    void separatesPairingAndOperationErrors();
    void handlesPinAndPasskeyInputWithCapturedRequestIds();
    void handlesPasskeyConfirmationAndAuthorizationPrompts();
    void updatesDisplayOnlyAgentPromptsAndClosesOnBackendCompletion();
    void forgetRequiresConfirmationAndUsesCapturedDevice();
};

namespace {

Astrea::System::BluetoothSnapshot readyBluetoothSnapshot()
{
    Astrea::System::BluetoothSnapshot snapshot;
    snapshot.state = Astrea::System::SystemServiceState::Ready;
    snapshot.available = true;
    snapshot.ready = true;
    snapshot.adapterAvailable = true;
    snapshot.adapterPath = QStringLiteral("/org/bluez/hci0");
    snapshot.adapterName = QStringLiteral("Test Adapter");
    return snapshot;
}

Astrea::System::BluetoothDevice bluetoothDevice(const QString &id, const QString &name,
                                                bool paired, bool connected = false)
{
    Astrea::System::BluetoothDevice device;
    device.id = id;
    device.objectPath = QStringLiteral("/org/bluez/hci0/") + id;
    device.address = QStringLiteral("AA:BB:CC:DD:EE:01");
    device.name = name;
    device.paired = paired;
    device.connected = connected;
    device.discovered = !paired;
    device.icon = QStringLiteral("audio-headphones");
    return device;
}

} // namespace

void SettingsBluetoothQmlTest::pageLoadsOffscreenWithTypedBluetoothEnums()
{
    BluetoothPageFixture fixture;

    QQuickItem *page = fixture.createPage();
    QVERIFY2(page != nullptr, qPrintable(fixture.componentErrors()));
    QCOMPARE(page->objectName(), QStringLiteral("bluetoothPage"));
    QCOMPARE(fixture.backend()->scanRequests, QStringList{QStringLiteral("settings-bluetooth-page")});
}

void SettingsBluetoothQmlTest::showsBluetoothAvailabilityStatesSeparately()
{
    BluetoothPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));

    Astrea::System::BluetoothSnapshot snapshot;
    snapshot.state = Astrea::System::SystemServiceState::Unavailable;
    snapshot.errorString = QStringLiteral("BlueZ is not available");
    fixture.publish(snapshot);
    QCOMPARE(fixture.findObject(QStringLiteral("bluetoothServiceStatus"))->property("text").toString(),
             QStringLiteral("Bluetooth service is unavailable"));
    QVERIFY(!fixture.findObject(QStringLiteral("bluetoothPowerToggle"))->property("enabled").toBool());
    QCOMPARE(fixture.findObject(QStringLiteral("bluetoothServiceError"))->property("text").toString(),
             QStringLiteral("BlueZ is not available"));

    snapshot = readyBluetoothSnapshot();
    snapshot.adapterAvailable = false;
    fixture.publish(snapshot);
    QCOMPARE(fixture.findObject(QStringLiteral("bluetoothServiceStatus"))->property("text").toString(),
             QStringLiteral("No Bluetooth adapter found"));
    QVERIFY(!fixture.findObject(QStringLiteral("bluetoothPowerToggle"))->property("enabled").toBool());

    snapshot = readyBluetoothSnapshot();
    fixture.publish(snapshot);
    QCOMPARE(fixture.findObject(QStringLiteral("bluetoothServiceStatus"))->property("text").toString(),
             QStringLiteral("Bluetooth is off"));
}

void SettingsBluetoothQmlTest::powerToggleUsesAuthoritativeStateAndRespectsPendingAndPairing()
{
    BluetoothPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));
    QObject *toggle = fixture.findObject(QStringLiteral("bluetoothPowerToggle"));
    QVERIFY(toggle != nullptr);
    QQuickItem *toggleItem = qobject_cast<QQuickItem *>(toggle);
    QVERIFY(toggleItem != nullptr);

    auto snapshot = readyBluetoothSnapshot();
    snapshot.powered = true;
    fixture.publish(snapshot);
    QVERIFY(toggle->property("checked").toBool());
    toggleItem->forceActiveFocus();
    QTest::keyClick(fixture.window(), Qt::Key_Space);
    QCoreApplication::processEvents();
    QCOMPARE(fixture.backend()->poweredTargets, QList<bool>{false});
    QVERIFY(toggle->property("checked").toBool());

    snapshot.powerPending = true;
    fixture.publish(snapshot);
    QVERIFY(!toggle->property("enabled").toBool());
    QCOMPARE(fixture.findObject(QStringLiteral("bluetoothServiceStatus"))->property("text").toString(),
             QStringLiteral("Bluetooth power is changing…"));
    QTest::keyClick(fixture.window(), Qt::Key_Space);
    QCOMPARE(fixture.backend()->poweredTargets.size(), 1);

    snapshot.powerPending = false;
    snapshot.pairing = true;
    fixture.publish(snapshot);
    QVERIFY(!toggle->property("enabled").toBool());
}

void SettingsBluetoothQmlTest::ownsOneScanLeaseUntilPageDestructionWhilePoweredOff()
{
    BluetoothPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));

    auto snapshot = readyBluetoothSnapshot();
    snapshot.powered = false;
    fixture.publish(snapshot);
    QCOMPARE(fixture.backend()->scanRequests.size(), 1);
    QVERIFY(fixture.backend()->scanReleases.isEmpty());

    fixture.destroyPage();
    QCOMPARE(fixture.backend()->scanReleases,
             QStringList{QStringLiteral("settings-bluetooth-page")});
    fixture.destroyPage();
    QCOMPARE(fixture.backend()->scanReleases.size(), 1);
}

void SettingsBluetoothQmlTest::pageDestructionCancelsActivePairing()
{
    BluetoothPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));

    auto snapshot = readyBluetoothSnapshot();
    snapshot.pairing = true;
    snapshot.pairingDevicePath = QStringLiteral("/org/bluez/hci0/dev_1");
    snapshot.pairingDeviceName = QStringLiteral("Keyboard");
    fixture.publish(snapshot);
    fixture.destroyPage();

    QCOMPARE(fixture.backend()->cancelPairingCount, 1);
    QCOMPARE(fixture.backend()->scanReleases,
             QStringList{QStringLiteral("settings-bluetooth-page")});
}

void SettingsBluetoothQmlTest::presentsSharedDevicesAndDispatchesActions()
{
    BluetoothPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));

    auto connected = bluetoothDevice(QStringLiteral("headphones"), QStringLiteral("Headphones"),
                                     true, true);
    connected.batteryPercent = 82;
    connected.rssi = -48;
    auto paired = bluetoothDevice(QStringLiteral("keyboard"), QStringLiteral("Keyboard"), true);
    auto unpaired = bluetoothDevice(QStringLiteral("controller"), QStringLiteral("Gamepad"), false);
    auto unnamed = bluetoothDevice(QStringLiteral("unnamed"), QString(), false);
    auto snapshot = readyBluetoothSnapshot();
    snapshot.powered = true;
    snapshot.devices = {connected, paired, unpaired, unnamed};
    fixture.publish(snapshot);

    auto *connectedRow = fixture.findObject(QStringLiteral("deviceRow_headphones_mine"));
    QVERIFY(connectedRow != nullptr);
    QVERIFY(connectedRow->property("visible").toBool());
    QVERIFY(fixture.findObject(QStringLiteral("deviceRow_controller_mine")) != nullptr);
    QVERIFY(!fixture.findObject(QStringLiteral("deviceRow_controller_mine"))->property("visible").toBool());
    QVERIFY(fixture.findObject(QStringLiteral("deviceRow_controller_other"))->property("visible").toBool());
    QCOMPARE(fixture.findObject(QStringLiteral("deviceName_headphones_mine"))->property("text").toString(),
             QStringLiteral("Headphones"));
    QVERIFY(fixture.findObject(QStringLiteral("deviceMeta_headphones_mine"))->property("text").toString()
                .contains(QStringLiteral("Connected")));
    QVERIFY(fixture.findObject(QStringLiteral("deviceDetails_headphones_mine"))->property("text").toString()
                .contains(QStringLiteral("82%")));
    QVERIFY(fixture.findObject(QStringLiteral("deviceDetails_headphones_mine"))->property("text").toString()
                .contains(QStringLiteral("-48 dBm")));
    QVERIFY(fixture.findObject(QStringLiteral("deviceAction_controller_other")) != nullptr);
    QCOMPARE(fixture.findObject(QStringLiteral("deviceName_unnamed_other"))->property("text").toString(),
             QStringLiteral("Bluetooth device"));

    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("deviceAction_headphones_mine")),
                                      "clicked"));
    QCOMPARE(fixture.backend()->disconnectedPaths,
             QStringList{QStringLiteral("/org/bluez/hci0/headphones")});
    QVERIFY(fixture.findObject(QStringLiteral("deviceMeta_headphones_mine"))->property("text").toString()
                .contains(QStringLiteral("Connected")));

    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("deviceAction_keyboard_mine")),
                                      "clicked"));
    QCOMPARE(fixture.backend()->connectedPaths,
             QStringList{QStringLiteral("/org/bluez/hci0/keyboard")});

    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("deviceMore_headphones_mine")),
                                      "clicked"));
    QObject *trustAction = fixture.findObject(QStringLiteral("trustDeviceAction"));
    QCOMPARE(trustAction->property("label").toString(), QStringLiteral("Trust"));
    QVERIFY(QMetaObject::invokeMethod(trustAction, "triggered"));
    QCOMPARE(fixture.backend()->trustedPaths,
             QStringList{QStringLiteral("/org/bluez/hci0/headphones")});
    QCOMPARE(fixture.backend()->trustedTargets, QList<bool>{true});

    snapshot.devices[0].trusted = true;
    fixture.publish(snapshot);
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("deviceMore_headphones_mine")),
                                      "clicked"));
    QCOMPARE(trustAction->property("label").toString(), QStringLiteral("Untrust"));
    QVERIFY(QMetaObject::invokeMethod(trustAction, "triggered"));
    QCOMPARE(fixture.backend()->trustedTargets, (QList<bool>{true, false}));

    auto *pairAction = qobject_cast<QQuickItem *>(
        fixture.findObject(QStringLiteral("deviceAction_controller_other")));
    QVERIFY(pairAction != nullptr);
    QCOMPARE(pairAction->property("accessibleName").toString(), QStringLiteral("Pair Gamepad"));
    pairAction->forceActiveFocus();
    QTest::keyClick(fixture.window(), Qt::Key_Return);
    QCoreApplication::processEvents();
    QCOMPARE(fixture.backend()->pairedPaths,
             QStringList{QStringLiteral("/org/bluez/hci0/controller")});

    snapshot.pairing = true;
    snapshot.pairingDevicePath = QStringLiteral("/org/bluez/hci0/controller");
    snapshot.pairingDeviceName = QStringLiteral("Gamepad");
    fixture.publish(snapshot);
    QCOMPARE(fixture.findObject(QStringLiteral("deviceMeta_controller_other"))->property("text").toString(),
             QStringLiteral("Pairing"));
    QVERIFY(!fixture.findObject(QStringLiteral("deviceAction_unnamed_other"))->property("enabled").toBool());
    QVERIFY(fixture.findObject(QStringLiteral("cancelPairingButton")) != nullptr);
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("cancelPairingButton")),
                                      "clicked"));
    QCOMPARE(fixture.backend()->cancelPairingCount, 1);
}

void SettingsBluetoothQmlTest::separatesPairingAndOperationErrors()
{
    BluetoothPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));

    auto snapshot = readyBluetoothSnapshot();
    snapshot.powered = true;
    snapshot.pairing = true;
    snapshot.pairingDevicePath = QStringLiteral("/org/bluez/hci0/controller");
    snapshot.pairingDeviceName = QStringLiteral("Gamepad");
    snapshot.pairingError = QStringLiteral("Pairing was rejected");
    snapshot.operationError = QStringLiteral("Trust operation failed");
    fixture.publish(snapshot);

    QCOMPARE(fixture.findObject(QStringLiteral("bluetoothPairingError"))->property("text").toString(),
             QStringLiteral("Pairing was rejected"));
    QCOMPARE(fixture.findObject(QStringLiteral("bluetoothOperationError"))->property("text").toString(),
             QStringLiteral("Trust operation failed"));
    QVERIFY(fixture.findObject(QStringLiteral("bluetoothServiceStatus"))->property("text").toString()
                .contains(QStringLiteral("ready"), Qt::CaseInsensitive));
    QVERIFY(!fixture.findObject(QStringLiteral("bluetoothServiceError"))->property("visible").toBool());
}

void SettingsBluetoothQmlTest::successfulPairingMovesWithAuthoritativeModelWithoutChaining()
{
    BluetoothPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));

    auto controller = bluetoothDevice(QStringLiteral("controller"), QStringLiteral("Gamepad"), false);
    auto snapshot = readyBluetoothSnapshot();
    snapshot.powered = true;
    snapshot.devices = {controller};
    fixture.publish(snapshot);
    QVERIFY(QMetaObject::invokeMethod(
        fixture.findObject(QStringLiteral("deviceAction_controller_other")), "clicked"));
    QCOMPARE(fixture.backend()->pairedPaths,
             QStringList{QStringLiteral("/org/bluez/hci0/controller")});

    controller.paired = true;
    controller.discovered = false;
    snapshot.devices = {controller};
    fixture.publish(snapshot);
    QVERIFY(fixture.findObject(QStringLiteral("deviceRow_controller_mine"))->property("visible").toBool());
    QVERIFY(!fixture.findObject(QStringLiteral("deviceRow_controller_other"))->property("visible").toBool());
    QVERIFY(fixture.backend()->connectedPaths.isEmpty());
    QVERIFY(fixture.backend()->trustedPaths.isEmpty());
}

void SettingsBluetoothQmlTest::handlesPinAndPasskeyInputWithCapturedRequestIds()
{
    BluetoothPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));

    auto snapshot = readyBluetoothSnapshot();
    snapshot.pairing = true;
    snapshot.pairingDevicePath = QStringLiteral("/org/bluez/hci0/keyboard");
    snapshot.pairingDeviceName = QStringLiteral("Keyboard");
    snapshot.agentRequestActive = true;
    snapshot.agentRequestId = 41;
    snapshot.agentRequestKind = Astrea::System::BluetoothAgentRequestKind::PinCodeInput;
    snapshot.agentDevicePath = snapshot.pairingDevicePath;
    snapshot.agentDeviceName = QStringLiteral("Keyboard");
    fixture.publish(snapshot);

    QObject *dialog = fixture.findObject(QStringLiteral("bluetoothAgentDialog"));
    QVERIFY(dialog != nullptr);
    QVERIFY(dialog->property("visible").toBool());
    QCOMPARE(fixture.findObject(QStringLiteral("agentPromptText"))->property("text").toString(),
             QStringLiteral("Enter the PIN for \"Keyboard\""));
    QObject *pinInput = fixture.findObject(QStringLiteral("agentTextInput"));
    QVERIFY(pinInput != nullptr);
    QVERIFY(pinInput->property("activeFocus").toBool());
    QVERIFY(!fixture.findObject(QStringLiteral("agentSubmitButton"))->property("enabled").toBool());
    pinInput->setProperty("text", QString(17, QLatin1Char('1')));
    QVERIFY(!fixture.findObject(QStringLiteral("agentSubmitButton"))->property("enabled").toBool());
    const QString maximumPin(16, QLatin1Char('4'));
    pinInput->setProperty("text", maximumPin);
    QVERIFY(fixture.findObject(QStringLiteral("agentSubmitButton"))->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("agentSubmitButton")), "clicked"));
    QCOMPARE(fixture.backend()->submittedRequestIds, QList<quint64>{41});
    QCOMPARE(fixture.backend()->submittedTexts, QStringList{maximumPin});

    snapshot.agentRequestId = 42;
    snapshot.agentRequestKind = Astrea::System::BluetoothAgentRequestKind::PasskeyInput;
    snapshot.agentDeviceName = QStringLiteral("New Keyboard");
    fixture.publish(snapshot);
    QCOMPARE(fixture.page()->property("displayedRequestId").toULongLong(), quint64(42));
    QCOMPARE(pinInput->property("text").toString(), QString());
    QCOMPARE(fixture.findObject(QStringLiteral("agentPromptText"))->property("text").toString(),
             QStringLiteral("Enter the passkey for \"New Keyboard\""));
    const qsizetype submissionsBeforeStaleResponse = fixture.backend()->submittedRequestIds.size();
    fixture.page()->setProperty("displayedRequestId", QVariant::fromValue<qulonglong>(41));
    pinInput->setProperty("text", QStringLiteral("000042"));
    QVERIFY(fixture.findObject(QStringLiteral("agentSubmitButton"))->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("agentSubmitButton")), "clicked"));
    QCOMPARE(fixture.backend()->submittedRequestIds.size(), submissionsBeforeStaleResponse);
    QCOMPARE(fixture.page()->property("displayedRequestId").toULongLong(), quint64(42));
    QCOMPARE(pinInput->property("text").toString(), QString());
    pinInput->setProperty("text", QStringLiteral("000042"));
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("agentSubmitButton")), "clicked"));
    QCOMPARE(fixture.backend()->submittedRequestIds, (QList<quint64>{41, 42}));
    QCOMPARE(fixture.backend()->submittedTexts,
             (QStringList{maximumPin, QStringLiteral("000042")}));

    snapshot.agentRequestId = 43;
    snapshot.agentRequestKind = Astrea::System::BluetoothAgentRequestKind::PasskeyInput;
    fixture.publish(snapshot);
    pinInput->setProperty("text", QStringLiteral("1234567"));
    QVERIFY(!fixture.findObject(QStringLiteral("agentSubmitButton"))->property("enabled").toBool());
    pinInput->setProperty("text", QStringLiteral("12x"));
    QVERIFY(!fixture.findObject(QStringLiteral("agentSubmitButton"))->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("agentRejectButton")), "clicked"));
    QCOMPARE(fixture.backend()->rejectedRequestIds, QList<quint64>{43});
}

void SettingsBluetoothQmlTest::handlesPasskeyConfirmationAndAuthorizationPrompts()
{
    BluetoothPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));

    auto snapshot = readyBluetoothSnapshot();
    snapshot.pairing = true;
    snapshot.agentRequestActive = true;
    snapshot.agentRequestId = 50;
    snapshot.agentRequestKind = Astrea::System::BluetoothAgentRequestKind::PasskeyConfirmation;
    snapshot.agentDeviceName = QStringLiteral("Phone");
    snapshot.agentPasskey = 42;
    fixture.publish(snapshot);
    QCOMPARE(fixture.findObject(QStringLiteral("agentPromptText"))->property("text").toString(),
             QStringLiteral("000042"));
    QCOMPARE(fixture.findObject(QStringLiteral("agentRejectButton"))->property("label").toString(),
             QStringLiteral("Cancel"));
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("agentConfirmButton")), "clicked"));
    QCOMPARE(fixture.backend()->confirmedRequestIds, QList<quint64>{50});
    QCOMPARE(fixture.backend()->confirmationValues, QList<bool>{true});

    snapshot.agentRequestId = 51;
    snapshot.agentRequestKind = Astrea::System::BluetoothAgentRequestKind::Authorization;
    fixture.publish(snapshot);
    QCOMPARE(fixture.findObject(QStringLiteral("agentPromptText"))->property("text").toString(),
             QStringLiteral("Allow \"Phone\" to pair?"));
    QCOMPARE(fixture.findObject(QStringLiteral("agentRejectButton"))->property("label").toString(),
             QStringLiteral("Reject"));
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("agentAllowButton")), "clicked"));
    QCOMPARE(fixture.backend()->confirmedRequestIds.last(), quint64(51));
    QVERIFY(fixture.backend()->confirmationValues.last());

    snapshot.agentRequestId = 52;
    snapshot.agentRequestKind = Astrea::System::BluetoothAgentRequestKind::ServiceAuthorization;
    snapshot.agentServiceUuid = QStringLiteral("0000110b-0000-1000-8000-00805f9b34fb");
    fixture.publish(snapshot);
    QVERIFY(fixture.findObject(QStringLiteral("agentPromptText"))->property("text").toString()
                .contains(snapshot.agentServiceUuid));
    QVERIFY(fixture.findObject(QStringLiteral("agentPromptText"))->property("text").toString()
                .contains(QStringLiteral("Phone")));
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("agentRejectButton")), "clicked"));
    QCOMPARE(fixture.backend()->confirmedRequestIds.last(), quint64(52));
    QVERIFY(!fixture.backend()->confirmationValues.last());
}

void SettingsBluetoothQmlTest::updatesDisplayOnlyAgentPromptsAndClosesOnBackendCompletion()
{
    BluetoothPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));

    auto snapshot = readyBluetoothSnapshot();
    snapshot.pairing = true;
    snapshot.agentRequestActive = true;
    snapshot.agentRequestId = 60;
    snapshot.agentRequestKind = Astrea::System::BluetoothAgentRequestKind::DisplayPinCode;
    snapshot.agentDeviceName = QStringLiteral("Phone");
    snapshot.agentDisplayPin = QStringLiteral("135790");
    fixture.publish(snapshot);
    QCOMPARE(fixture.findObject(QStringLiteral("agentPromptText"))->property("text").toString(),
             QStringLiteral("135790"));
    QVERIFY(!fixture.findObject(QStringLiteral("agentAllowButton"))->property("visible").toBool());
    QVERIFY(fixture.findObject(QStringLiteral("agentCancelPairingButton")) != nullptr);

    snapshot.agentRequestId = 61;
    snapshot.agentRequestKind = Astrea::System::BluetoothAgentRequestKind::DisplayPasskey;
    snapshot.agentPasskey = 7;
    snapshot.agentEntered = 3;
    fixture.publish(snapshot);
    QObject *dialog = fixture.findObject(QStringLiteral("bluetoothAgentDialog"));
    QVERIFY(dialog->property("visible").toBool());
    QCOMPARE(fixture.findObject(QStringLiteral("agentPromptText"))->property("text").toString(),
             QStringLiteral("000007"));
    QCOMPARE(fixture.findObject(QStringLiteral("agentProgressText"))->property("text").toString(),
             QStringLiteral("3 of 6 digits entered"));
    snapshot.agentEntered = 300;
    fixture.publish(snapshot);
    QVERIFY(dialog->property("visible").toBool());
    QCOMPARE(fixture.findObject(QStringLiteral("agentProgressText"))->property("text").toString(),
             QStringLiteral("300 of 6 digits entered"));
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("agentCancelPairingButton")),
                                      "clicked"));
    QCOMPARE(fixture.backend()->cancelPairingCount, 1);

    snapshot.agentRequestActive = false;
    snapshot.agentRequestId = 0;
    snapshot.agentRequestKind = Astrea::System::BluetoothAgentRequestKind::None;
    fixture.publish(snapshot);
    QVERIFY(!dialog->property("visible").toBool());
    QVERIFY(fixture.backend()->confirmedRequestIds.isEmpty());
    QVERIFY(fixture.backend()->rejectedRequestIds.isEmpty());
}

void SettingsBluetoothQmlTest::forgetRequiresConfirmationAndUsesCapturedDevice()
{
    BluetoothPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));
    auto first = bluetoothDevice(QStringLiteral("first"), QStringLiteral("First device"), true);
    auto second = bluetoothDevice(QStringLiteral("second"), QStringLiteral("Second device"), true);
    auto snapshot = readyBluetoothSnapshot();
    snapshot.devices = {first, second};
    fixture.publish(snapshot);

    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("deviceMore_first_mine")),
                                      "clicked"));
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("forgetDeviceAction")),
                                      "triggered"));
    QObject *dialog = fixture.findObject(QStringLiteral("forgetDeviceDialog"));
    QVERIFY(dialog != nullptr);
    QVERIFY(dialog->property("visible").toBool());
    QVERIFY(fixture.backend()->forgottenPaths.isEmpty());
    QVERIFY(fixture.findObject(QStringLiteral("forgetDevicePrompt"))->property("text").toString()
                .contains(QStringLiteral("First device")));
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("forgetDeviceCancelButton")),
                                      "clicked"));
    QVERIFY(fixture.backend()->forgottenPaths.isEmpty());
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("deviceMore_first_mine")),
                                      "clicked"));
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("forgetDeviceAction")),
                                      "triggered"));
    QVERIFY(dialog->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("forgetDeviceConfirmButton")),
                                      "clicked"));
    QCOMPARE(fixture.backend()->forgottenPaths, QStringList{first.objectPath});
    QVERIFY(fixture.findObject(QStringLiteral("deviceRow_first_mine")) != nullptr);

    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("deviceMore_second_mine")),
                                      "clicked"));
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("forgetDeviceAction")),
                                      "triggered"));
    QVERIFY(dialog->property("visible").toBool());
    QVERIFY(fixture.findObject(QStringLiteral("forgetDevicePrompt"))->property("text").toString()
                .contains(QStringLiteral("Second device")));

    QCOMPARE(fixture.backend()->forgottenPaths.size(), 1);

    snapshot.devices.removeLast();
    fixture.publish(snapshot);
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("forgetDeviceConfirmButton")),
                                      "clicked"));
    QCOMPARE(fixture.backend()->forgottenPaths,
             (QStringList{first.objectPath, second.objectPath}));
    QVERIFY(fixture.findObject(QStringLiteral("deviceRow_second_mine")) == nullptr);
}

QTEST_MAIN(SettingsBluetoothQmlTest)
#include "SettingsBluetoothQmlTest.moc"
