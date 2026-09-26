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
        return scanRequestAccepted;
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
    bool scanRequestAccepted = true;
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

class BluetoothPageFixture final {
public:
    BluetoothPageFixture()
    {
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
    void failedScanAcquisitionIsNotReleased();
    void emptyStatesFollowAuthoritativeDeviceComposition();
    void pageDestructionCancelsActivePairing();
    void presentsSharedDevicesAndDispatchesActions();
    void resolvesDeviceKindFallbacksAndKeepsDuplicateNamesPathKeyed();
    void usesOnePlainTextIdentityAcrossAgentAndForgetPrompts();
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
                                                bool paired, bool connected = false,
                                                Astrea::System::BluetoothDeviceKind deviceKind =
                                                    Astrea::System::BluetoothDeviceKind::Unknown)
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
    device.deviceKind = deviceKind;
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

void SettingsBluetoothQmlTest::failedScanAcquisitionIsNotReleased()
{
    BluetoothPageFixture fixture;
    fixture.backend()->scanRequestAccepted = false;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));
    QCOMPARE(fixture.backend()->scanRequests,
             QStringList{QStringLiteral("settings-bluetooth-page")});

    fixture.destroyPage();
    QVERIFY(fixture.backend()->scanReleases.isEmpty());
}

void SettingsBluetoothQmlTest::emptyStatesFollowAuthoritativeDeviceComposition()
{
    BluetoothPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));
    QObject *myDevicesEmpty = fixture.findObject(QStringLiteral("myDevicesEmptyState"));
    QObject *otherDevicesEmpty = fixture.findObject(QStringLiteral("otherDevicesEmptyState"));
    QVERIFY(myDevicesEmpty != nullptr);
    QVERIFY(otherDevicesEmpty != nullptr);

    auto snapshot = readyBluetoothSnapshot();
    snapshot.powered = true;

    fixture.publish(snapshot);
    QVERIFY(myDevicesEmpty->property("visible").toBool());
    QVERIFY(otherDevicesEmpty->property("visible").toBool());

    snapshot.devices = {bluetoothDevice(QStringLiteral("paired"), QStringLiteral("Paired"), true)};
    fixture.publish(snapshot);
    QVERIFY(!myDevicesEmpty->property("visible").toBool());
    QVERIFY(otherDevicesEmpty->property("visible").toBool());

    snapshot.devices = {bluetoothDevice(QStringLiteral("unpaired"), QStringLiteral("Unpaired"), false)};
    fixture.publish(snapshot);
    QVERIFY(myDevicesEmpty->property("visible").toBool());
    QVERIFY(!otherDevicesEmpty->property("visible").toBool());

    snapshot.devices = {
        bluetoothDevice(QStringLiteral("paired"), QStringLiteral("Paired"), true),
        bluetoothDevice(QStringLiteral("unpaired"), QStringLiteral("Unpaired"), false),
    };
    fixture.publish(snapshot);
    QVERIFY(!myDevicesEmpty->property("visible").toBool());
    QVERIFY(!otherDevicesEmpty->property("visible").toBool());
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

void SettingsBluetoothQmlTest::resolvesDeviceKindFallbacksAndKeepsDuplicateNamesPathKeyed()
{
    using Astrea::System::BluetoothDeviceKind;

    BluetoothPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));

    const QList<QPair<BluetoothDeviceKind, QString>> fallbackCases = {
        {BluetoothDeviceKind::Unknown, QStringLiteral("Bluetooth device")},
        {BluetoothDeviceKind::Audio, QStringLiteral("Bluetooth audio device")},
        {BluetoothDeviceKind::Headset, QStringLiteral("Bluetooth headset")},
        {BluetoothDeviceKind::Headphones, QStringLiteral("Bluetooth headphones")},
        {BluetoothDeviceKind::Speaker, QStringLiteral("Bluetooth speaker")},
        {BluetoothDeviceKind::Keyboard, QStringLiteral("Bluetooth keyboard")},
        {BluetoothDeviceKind::Mouse, QStringLiteral("Bluetooth mouse")},
        {BluetoothDeviceKind::Gamepad, QStringLiteral("Bluetooth controller")},
        {BluetoothDeviceKind::Phone, QStringLiteral("Phone")},
        {BluetoothDeviceKind::Computer, QStringLiteral("Computer")},
        {BluetoothDeviceKind::Tablet, QStringLiteral("Tablet")},
        {BluetoothDeviceKind::Printer, QStringLiteral("Bluetooth printer")},
        {BluetoothDeviceKind::Camera, QStringLiteral("Bluetooth camera")},
        {BluetoothDeviceKind::Network, QStringLiteral("Bluetooth network device")},
    };

    QList<Astrea::System::BluetoothDevice> devices;
    for (qsizetype index = 0; index < fallbackCases.size(); ++index) {
        const QString id = QStringLiteral("kind_%1").arg(index);
        devices.append(bluetoothDevice(id, QString{}, false, false, fallbackCases[index].first));
    }

    auto named = bluetoothDevice(QStringLiteral("human_name"), QStringLiteral("WH-1000XM5"),
                                 false, false, BluetoothDeviceKind::Headphones);
    auto normalized = bluetoothDevice(QStringLiteral("normalized_name"),
                                      QStringLiteral("WH-1000XM3"), false, false,
                                      BluetoothDeviceKind::Headphones);
    auto technicalHeadset = bluetoothDevice(QStringLiteral("technical_headset"), QString{}, false,
                                            false, BluetoothDeviceKind::Headset);
    technicalHeadset.address = QStringLiteral("AA:BB:CC:DD:EE:FF");
    technicalHeadset.objectPath =
        QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
    auto technicalKeyboard = bluetoothDevice(QStringLiteral("technical_keyboard"), QString{}, false,
                                             false, BluetoothDeviceKind::Keyboard);
    technicalKeyboard.name = QString{};
    auto technicalUnknown = bluetoothDevice(QStringLiteral("technical_unknown"), QString{}, false);
    technicalUnknown.address = QStringLiteral("11:22:33:44:55:66");
    technicalUnknown.objectPath =
        QStringLiteral("/org/bluez/hci0/dev_11_22_33_44_55_66");
    auto duplicateA = bluetoothDevice(QStringLiteral("duplicate_a"), QStringLiteral("Studio Buds"),
                                      false, false, BluetoothDeviceKind::Headset);
    auto duplicateB = bluetoothDevice(QStringLiteral("duplicate_b"), QStringLiteral("Studio Buds"),
                                      false, false, BluetoothDeviceKind::Headset);
    devices.append({named, normalized, technicalHeadset, technicalKeyboard, technicalUnknown,
                    duplicateA, duplicateB});

    auto snapshot = readyBluetoothSnapshot();
    snapshot.powered = true;
    snapshot.devices = devices;
    fixture.publish(snapshot);

    for (qsizetype index = 0; index < fallbackCases.size(); ++index) {
        const QString id = QStringLiteral("kind_%1").arg(index);
        const QString objectName = QStringLiteral("deviceName_%1_other").arg(id);
        QObject *nameLabel = fixture.findObject(objectName);
        QVERIFY2(nameLabel != nullptr, qPrintable(objectName));
        QCOMPARE(nameLabel->property("text").toString(), fallbackCases[index].second);
    }
    QObject *humanName = fixture.findObject(QStringLiteral("deviceName_human_name_other"));
    QVERIFY(humanName != nullptr);
    QCOMPARE(humanName->property("text").toString(), QStringLiteral("WH-1000XM5"));
    QObject *normalizedName = fixture.findObject(QStringLiteral("deviceName_normalized_name_other"));
    QVERIFY(normalizedName != nullptr);
    QCOMPARE(normalizedName->property("text").toString(), QStringLiteral("WH-1000XM3"));
    const QString technicalHeadsetObjectName =
        QStringLiteral("deviceName_dev_AA_BB_CC_DD_EE_FF_other");
    QObject *technicalHeadsetLabel = fixture.findObject(technicalHeadsetObjectName);
    QVERIFY2(technicalHeadsetLabel != nullptr, qPrintable(technicalHeadsetObjectName));
    QCOMPARE(technicalHeadsetLabel->property("text").toString(),
             QStringLiteral("Bluetooth headset"));
    QObject *technicalKeyboardLabel =
        fixture.findObject(QStringLiteral("deviceName_technical_keyboard_other"));
    QVERIFY(technicalKeyboardLabel != nullptr);
    QCOMPARE(technicalKeyboardLabel->property("text").toString(),
             QStringLiteral("Bluetooth keyboard"));
    const QString technicalUnknownObjectName =
        QStringLiteral("deviceName_dev_11_22_33_44_55_66_other");
    QObject *technicalUnknownLabel = fixture.findObject(technicalUnknownObjectName);
    QVERIFY2(technicalUnknownLabel != nullptr, qPrintable(technicalUnknownObjectName));
    QCOMPARE(technicalUnknownLabel->property("text").toString(),
             QStringLiteral("Bluetooth device"));
    QVERIFY(fixture.findObject(QStringLiteral("deviceRow_dev_11_22_33_44_55_66_other"))
            != nullptr);
    QVERIFY(!fixture.findObject(QStringLiteral("otherDevicesEmptyState"))
                 ->property("visible").toBool());

    const QString technicalVisibleName = technicalHeadsetLabel->property("text").toString();
    QVERIFY(!technicalVisibleName.contains(QStringLiteral("LE_4A8C19FE")));
    QVERIFY(!technicalVisibleName.contains(technicalHeadset.address));
    QVERIFY(!technicalVisibleName.contains(technicalHeadset.objectPath));
    QCOMPARE(fixture.findObject(QStringLiteral("deviceName_duplicate_a_other"))
                 ->property("text").toString(),
             fixture.findObject(QStringLiteral("deviceName_duplicate_b_other"))
                 ->property("text").toString());

    QVERIFY(QMetaObject::invokeMethod(
        fixture.findObject(QStringLiteral("deviceAction_duplicate_b_other")), "clicked"));
    QCOMPARE(fixture.backend()->pairedPaths, QStringList{duplicateB.objectPath});
}

void SettingsBluetoothQmlTest::usesOnePlainTextIdentityAcrossAgentAndForgetPrompts()
{
    using Astrea::System::BluetoothAgentRequestKind;
    using Astrea::System::BluetoothDeviceKind;
    using Astrea::System::BluetoothServiceCategory;

    BluetoothPageFixture fixture;
    QVERIFY2(fixture.createPage() != nullptr, qPrintable(fixture.componentErrors()));

    auto headset = bluetoothDevice(QStringLiteral("unnamed_headset"), QString{}, true, false,
                                   BluetoothDeviceKind::Headset);
    auto markedUp = bluetoothDevice(QStringLiteral("marked_up"),
                                    QStringLiteral("<b>Owner Device</b> & friends"), true);
    auto snapshot = readyBluetoothSnapshot();
    snapshot.powered = true;
    snapshot.devices = {headset, markedUp};
    fixture.publish(snapshot);

    QObject *headsetName = fixture.findObject(QStringLiteral("deviceName_unnamed_headset_mine"));
    QVERIFY(headsetName != nullptr);
    QCOMPARE(headsetName->property("text").toString(), QStringLiteral("Bluetooth headset"));
    QCOMPARE(headsetName->property("textFormat").toInt(), 0);

    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("deviceMore_unnamed_headset_mine")),
                                      "clicked"));
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("forgetDeviceAction")),
                                      "triggered"));
    QObject *forgetPrompt = fixture.findObject(QStringLiteral("forgetDevicePrompt"));
    QCOMPARE(forgetPrompt->property("text").toString(), QStringLiteral("Forget Bluetooth headset?"));
    QCOMPARE(forgetPrompt->property("textFormat").toInt(), 0);
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("forgetDeviceCancelButton")),
                                      "clicked"));

    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("deviceMore_marked_up_mine")),
                                      "clicked"));
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("forgetDeviceAction")),
                                      "triggered"));
    const QString markedUpForgetText =
        QStringLiteral("Forget <b>Owner Device</b> & friends?");
    QCOMPARE(forgetPrompt->property("text").toString(), markedUpForgetText);
    QCOMPARE(forgetPrompt->property("textFormat").toInt(), 0);
    QVERIFY(QMetaObject::invokeMethod(fixture.findObject(QStringLiteral("forgetDeviceConfirmButton")),
                                      "clicked"));
    QCOMPARE(fixture.backend()->forgottenPaths, QStringList{markedUp.objectPath});

    snapshot.agentRequestActive = true;
    snapshot.agentRequestId = 120;
    snapshot.agentRequestKind = BluetoothAgentRequestKind::PinCodeInput;
    snapshot.agentDevicePath = headset.objectPath;
    snapshot.agentDeviceName = headset.name;
    snapshot.agentDeviceKind = headset.deviceKind;
    fixture.publish(snapshot);
    QObject *agentPrompt = fixture.findObject(QStringLiteral("agentPromptText"));
    QCOMPARE(agentPrompt->property("text").toString(),
             QStringLiteral("Enter the PIN for \"Bluetooth headset\""));
    QCOMPARE(agentPrompt->property("textFormat").toInt(), 0);

    snapshot.agentRequestId = 121;
    snapshot.agentRequestKind = BluetoothAgentRequestKind::Authorization;
    fixture.publish(snapshot);
    QCOMPARE(agentPrompt->property("text").toString(),
             QStringLiteral("Allow \"Bluetooth headset\" to pair?"));
    QCOMPARE(agentPrompt->property("textFormat").toInt(), 0);

    snapshot.agentRequestId = 122;
    snapshot.agentRequestKind = BluetoothAgentRequestKind::ServiceAuthorization;
    snapshot.agentServiceCategory = BluetoothServiceCategory::Audio;
    fixture.publish(snapshot);
    QCOMPARE(agentPrompt->property("text").toString(),
             QStringLiteral("Allow \"Bluetooth headset\" to connect for Bluetooth audio?"));
    QCOMPARE(agentPrompt->property("textFormat").toInt(), 0);

    snapshot.agentRequestId = 123;
    snapshot.agentRequestKind = BluetoothAgentRequestKind::Authorization;
    snapshot.agentDeviceName = markedUp.name;
    snapshot.agentDeviceKind = BluetoothDeviceKind::Unknown;
    fixture.publish(snapshot);
    QCOMPARE(agentPrompt->property("text").toString(),
             QStringLiteral("Allow \"<b>Owner Device</b> & friends\" to pair?"));
    QCOMPARE(agentPrompt->property("textFormat").toInt(), 0);
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

    struct ServiceAuthorizationCase
    {
        QString uuid;
        Astrea::System::BluetoothServiceCategory category;
        QString capability;
        QString technicalName;
    };
    const QList<ServiceAuthorizationCase> serviceCases = {
        {QStringLiteral("0000184d-0000-1000-8000-00805f9b34fb"),
         Astrea::System::BluetoothServiceCategory::Audio,
         QStringLiteral("connect for Bluetooth audio"), QStringLiteral("Microphone Control")},
        {QStringLiteral("0000110f-0000-1000-8000-00805f9b34fb"),
         Astrea::System::BluetoothServiceCategory::MediaControl,
         QStringLiteral("control media playback"), QStringLiteral("A/V Remote Control Controller")},
        {QStringLiteral("00001204-0000-1000-8000-00805f9b34fb"),
         Astrea::System::BluetoothServiceCategory::Calls,
         QStringLiteral("connect for calls and microphone audio"), QStringLiteral("Generic Telephony")},
        {QStringLiteral("00001812-0000-1000-8000-00805f9b34fb"),
         Astrea::System::BluetoothServiceCategory::HumanInterface,
         QStringLiteral("control this computer as an input device"),
         QStringLiteral("Human Interface Device Service")},
        {QStringLiteral("00001825-0000-1000-8000-00805f9b34fb"),
         Astrea::System::BluetoothServiceCategory::FileTransfer,
         QStringLiteral("transfer files"), QStringLiteral("Object Transfer Service")},
        {QStringLiteral("0000112e-0000-1000-8000-00805f9b34fb"),
         Astrea::System::BluetoothServiceCategory::Contacts,
         QStringLiteral("access contacts"), QStringLiteral("Phonebook Access Client")},
        {QStringLiteral("00001820-0000-1000-8000-00805f9b34fb"),
         Astrea::System::BluetoothServiceCategory::Network,
         QStringLiteral("use network access"), QStringLiteral("Internet Protocol Support Service")},
        {QStringLiteral("0000180f-0000-1000-8000-00805f9b34fb"),
         Astrea::System::BluetoothServiceCategory::Battery,
         QStringLiteral("access battery information"), QStringLiteral("Battery Service")},
        {QStringLiteral("00001101-0000-1000-8000-00805f9b34fb"),
         Astrea::System::BluetoothServiceCategory::Serial,
         QStringLiteral("use a serial connection"), QStringLiteral("Serial Port")},
        {QStringLiteral("f000110e-0451-4000-b000-000000000000"),
         Astrea::System::BluetoothServiceCategory::Unknown,
         QStringLiteral("connect using a Bluetooth service"), QStringLiteral("A/V Remote Control")},
    };

    quint64 nextRequestId = 52;
    for (const ServiceAuthorizationCase &serviceCase : serviceCases) {
        for (const bool accepted : {true, false}) {
            snapshot.agentRequestId = nextRequestId++;
            snapshot.agentRequestKind = Astrea::System::BluetoothAgentRequestKind::ServiceAuthorization;
            snapshot.agentServiceUuid = serviceCase.uuid;
            snapshot.agentServiceCategory = serviceCase.category;
            fixture.publish(snapshot);

            const QString promptText =
                fixture.findObject(QStringLiteral("agentPromptText"))->property("text").toString();
            QVERIFY(promptText.contains(QStringLiteral("Phone")));
            QVERIFY2(promptText.contains(serviceCase.capability), qPrintable(promptText));
            QVERIFY(!promptText.contains(serviceCase.uuid));
            QVERIFY(!promptText.contains(serviceCase.technicalName));
            QCOMPARE(fixture.page()->property("displayedRequestId").toULongLong(), nextRequestId - 1);

            QObject *button = fixture.findObject(accepted ? QStringLiteral("agentAllowButton")
                                                          : QStringLiteral("agentRejectButton"));
            QVERIFY(button != nullptr);
            QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
            QCOMPARE(fixture.backend()->confirmedRequestIds.last(), nextRequestId - 1);
            QCOMPARE(fixture.backend()->confirmationValues.last(), accepted);
        }
    }

    snapshot.agentRequestId = nextRequestId;
    snapshot.agentRequestKind = Astrea::System::BluetoothAgentRequestKind::Authorization;
    snapshot.agentServiceUuid.clear();
    snapshot.agentServiceCategory = Astrea::System::BluetoothServiceCategory::Unknown;
    fixture.publish(snapshot);
    QCOMPARE(fixture.page()->property("displayedRequestId").toULongLong(), nextRequestId);
    QCOMPARE(fixture.page()->property("displayedServiceUuid").toString(), QString{});
    QCOMPARE(fixture.page()->property("displayedServiceCategory").value<Astrea::System::BluetoothServiceCategory>(),
             Astrea::System::BluetoothServiceCategory::Unknown);
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
