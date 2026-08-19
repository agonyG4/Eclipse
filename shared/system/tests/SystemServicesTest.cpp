#include "system/SystemServiceState.hpp"
#include "system/audio/AudioService.hpp"
#include "system/audio/AudioOutputModel.hpp"
#include "system/network/NetworkService.hpp"
#include "system/network/WifiNetworkModel.hpp"
#include "system/bluetooth/BluetoothService.hpp"
#include "system/bluetooth/BluetoothDeviceModel.hpp"

#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

#include <memory>

using namespace Astrea::System;

namespace {

class FakeAudioBackend final : public AudioBackend {
public:
    bool start(const Callbacks &callbacks, QString *errorOut) override
    {
        Q_UNUSED(errorOut)
        m_callbacks = callbacks;
        ++startCount;
        return startResult;
    }

    void stop() override
    {
        ++stopCount;
        m_callbacks = {};
    }

    bool setDefaultOutput(quint32 nodeId) override
    {
        defaultNodeId = nodeId;
        return actionResult;
    }

    bool setVolume(quint32 nodeId, double linear) override
    {
        volumeNodeId = nodeId;
        volumeLinear = linear;
        return actionResult;
    }

    bool setMute(quint32 nodeId, bool muted) override
    {
        muteNodeId = nodeId;
        muteValue = muted;
        return actionResult;
    }

    void publish(const QVector<AudioOutput> &outputs, quint32 defaultNodeId)
    {
        if (m_callbacks.outputsChanged)
            m_callbacks.outputsChanged(outputs, defaultNodeId);
    }

    void publishDefaultState(bool available, bool ready, const QString &error = {})
    {
        if (m_callbacks.defaultStateChanged)
            m_callbacks.defaultStateChanged(available, ready, error);
    }

    bool startResult = true;
    bool actionResult = true;
    int startCount = 0;
    int stopCount = 0;
    quint32 defaultNodeId = 0;
    quint32 volumeNodeId = 0;
    double volumeLinear = 0.0;
    quint32 muteNodeId = 0;
    bool muteValue = false;

private:
    Callbacks m_callbacks;
};

class FakeNetworkBackend final : public NetworkBackend {
public:
    bool start(const Callbacks &callbacks, QString *errorOut) override
    {
        Q_UNUSED(errorOut)
        m_callbacks = callbacks;
        ++startCount;
        return startResult;
    }

    void stop() override
    {
        ++stopCount;
        m_callbacks = {};
    }

    bool setWifiEnabled(bool enabled) override
    {
        wifiEnabled = enabled;
        return actionResult;
    }

    bool requestWifiScan() override
    {
        ++scanCount;
        return actionResult;
    }

    void publish(const NetworkSnapshot &snapshot)
    {
        if (m_callbacks.snapshotChanged)
            m_callbacks.snapshotChanged(snapshot);
    }

    bool startResult = true;
    bool actionResult = true;
    int startCount = 0;
    int stopCount = 0;
    int scanCount = 0;
    bool wifiEnabled = false;

private:
    Callbacks m_callbacks;
};

class FakeBluetoothBackend final : public BluetoothBackend {
public:
    bool start(const Callbacks &callbacks, QString *errorOut) override
    {
        Q_UNUSED(errorOut)
        m_callbacks = callbacks;
        ++startCount;
        return startResult;
    }

    void stop() override
    {
        ++stopCount;
        m_callbacks = {};
    }

    bool setPowered(bool powered) override
    {
        poweredValue = powered;
        return actionResult;
    }

    bool startDiscovery() override
    {
        ++startDiscoveryCount;
        return actionResult;
    }

    bool stopDiscovery() override
    {
        ++stopDiscoveryCount;
        return actionResult;
    }

    bool connectDevice(const QString &path) override
    {
        ++connectCount;
        connectedPath = path;
        return actionResult;
    }

    bool disconnectDevice(const QString &path) override
    {
        disconnectedPath = path;
        return actionResult;
    }

    void publish(const BluetoothSnapshot &snapshot)
    {
        if (m_callbacks.snapshotChanged)
            m_callbacks.snapshotChanged(snapshot);
    }

    bool startResult = true;
    bool actionResult = true;
    int startCount = 0;
    int stopCount = 0;
    int startDiscoveryCount = 0;
    int stopDiscoveryCount = 0;
    int connectCount = 0;
    bool poweredValue = false;
    QString connectedPath;
    QString disconnectedPath;

private:
    Callbacks m_callbacks;
};

AudioOutput output(quint32 id, const QString &name, bool isDefault = false)
{
    return AudioOutput{id, name, name + QStringLiteral(" description"),
                       name + QStringLiteral(" nick"), QStringLiteral("Audio/Sink"),
                       isDefault, false};
}

} // namespace

class SystemServicesTest final : public QObject {
    Q_OBJECT

private slots:
    void serviceStatesAreRestartable();
    void audioUnavailableThenReconnects();
    void audioRuntimeFailureReconnects();
    void audioUsesCubicUiScaleAndStableModelOrder();
    void networkWifiAvailabilityAndScanningAreAuthoritative();
    void wifiModelDeduplicatesAndAvoidsSemanticReset();
    void bluetoothModelReconcilesAndScanOwnershipIsReferenceCounted();
    void bluetoothRejectsUnpairedConnect();
    void bluetoothScanningFollowsBackendState();
    void bluetoothPowerWaitsForAuthoritativeState();
    void networkRatesUseReadableUnits();
    void healthJsonContainsNoSecrets();
};

void SystemServicesTest::serviceStatesAreRestartable()
{
    auto backend = std::make_unique<FakeAudioBackend>();
    auto *backendPtr = backend.get();
    AudioService service(std::move(backend));
    QSignalSpy stateSpy(&service, &AudioService::stateChanged);

    QCOMPARE(service.state(), SystemServiceState::Stopped);
    QVERIFY(service.start());
    QCOMPARE(service.state(), SystemServiceState::Ready);
    QVERIFY(service.start());
    QCOMPARE(backendPtr->startCount, 1);
    service.stop();
    QCOMPARE(service.state(), SystemServiceState::Stopped);
    service.stop();
    QCOMPARE(backendPtr->stopCount, 1);
    QVERIFY(service.start());
    QCOMPARE(backendPtr->startCount, 2);
    QVERIFY(stateSpy.count() >= 3);
}

void SystemServicesTest::audioUnavailableThenReconnects()
{
    auto backend = std::make_unique<FakeAudioBackend>();
    auto *backendPtr = backend.get();
    backendPtr->startResult = false;
    AudioService service(std::move(backend));

    QVERIFY(service.start());
    QCOMPARE(service.state(), SystemServiceState::Unavailable);
    backendPtr->startResult = true;
    QTRY_COMPARE_WITH_TIMEOUT(service.state(), SystemServiceState::Ready, 1500);
    QVERIFY(backendPtr->startCount >= 2);
}

void SystemServicesTest::audioRuntimeFailureReconnects()
{
    auto backend = std::make_unique<FakeAudioBackend>();
    auto *backendPtr = backend.get();
    AudioService service(std::move(backend));
    QVERIFY(service.start());
    backendPtr->publishDefaultState(false, false, QStringLiteral("PipeWire disconnected"));
    QTRY_VERIFY_WITH_TIMEOUT(service.state() != SystemServiceState::Ready, 500);
    QTRY_VERIFY_WITH_TIMEOUT(backendPtr->startCount >= 2, 1500);
    QCOMPARE(service.state(), SystemServiceState::Ready);
}

void SystemServicesTest::audioUsesCubicUiScaleAndStableModelOrder()
{
    QCOMPARE(AudioService::uiPercentToLinear(0.0), 0.0);
    QCOMPARE(AudioService::uiPercentToLinear(50.0), 0.125);
    QCOMPARE(AudioService::uiPercentToLinear(100.0), 1.0);
    QCOMPARE(AudioService::uiPercentToLinear(150.0), 3.375);
    QCOMPARE(AudioService::linearToUiPercent(0.125), 50.0);
    QCOMPARE(AudioService::linearToUiPercent(1.0), 100.0);
    QCOMPARE(AudioService::linearToUiPercent(3.375), 150.0);

    AudioOutputModel model;
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    model.replace({output(4, QStringLiteral("zeta")),
                   output(2, QStringLiteral("alpha"), true),
                   output(3, QStringLiteral("beta"))}, 2);
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.data(model.index(0, 0), AudioOutputModel::NameRole).toString(),
             QStringLiteral("alpha"));
    QVERIFY(model.data(model.index(0, 0), AudioOutputModel::DefaultRole).toBool());
    model.replace({output(4, QStringLiteral("zeta")),
                   output(2, QStringLiteral("alpha"), true),
                   output(3, QStringLiteral("beta"))}, 2);
    QCOMPARE(resetSpy.count(), 1);
}

void SystemServicesTest::networkWifiAvailabilityAndScanningAreAuthoritative()
{
    auto backend = std::make_unique<FakeNetworkBackend>();
    auto *backendPtr = backend.get();
    NetworkService service(std::move(backend));
    QVERIFY(service.start());

    NetworkSnapshot snapshot;
    snapshot.daemonAvailable = true;
    snapshot.wifiEnabled = true;
    snapshot.wifiAvailable = false;
    snapshot.wifiScanning = false;
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(service.wifiEnabled(), 500);
    QVERIFY(!service.wifiAvailable());
    QVERIFY(!service.wifiScanning());

    snapshot.wifiAvailable = true;
    snapshot.wifiScanning = true;
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(service.wifiAvailable() && service.wifiScanning(), 500);
}

void SystemServicesTest::wifiModelDeduplicatesAndAvoidsSemanticReset()
{
    WifiNetworkModel model;
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    const WifiNetwork duplicateA{QStringLiteral("Cafe"), 40, false, true, 2412,
                                 QStringLiteral("a")};
    const WifiNetwork duplicateB{QStringLiteral("Cafe"), 80, true, true, 2412,
                                 QStringLiteral("b")};
    const WifiNetwork office{QStringLiteral("Office"), 60, false, false, 5180,
                             QStringLiteral("c")};
    model.replace({duplicateA, office, duplicateB});
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0, 0), WifiNetworkModel::SsidRole).toString(),
             QStringLiteral("Cafe"));
    QCOMPARE(model.data(model.index(0, 0), WifiNetworkModel::StrengthRole).toInt(), 80);
    QVERIFY(model.data(model.index(0, 0), WifiNetworkModel::ActiveRole).toBool());
    model.replace({duplicateA, office, duplicateB});
    QCOMPARE(resetSpy.count(), 1);
}

void SystemServicesTest::bluetoothModelReconcilesAndScanOwnershipIsReferenceCounted()
{
    auto backend = std::make_unique<FakeBluetoothBackend>();
    auto *backendPtr = backend.get();
    BluetoothService service(std::move(backend));
    QVERIFY(service.start());

    BluetoothSnapshot snapshot;
    snapshot.daemonAvailable = true;
    snapshot.adapterAvailable = true;
    snapshot.adapterPath = QStringLiteral("/org/bluez/hci1");
    snapshot.adapterName = QStringLiteral("Desk");
    snapshot.powered = true;
    snapshot.devices = {
        {QStringLiteral("id-a"), QStringLiteral("/org/bluez/hci1/dev_AA"),
         QStringLiteral("AA"), QStringLiteral("Mouse"), true, true, true, true,
         QStringLiteral("input-mouse"), -10, 80},
        {QStringLiteral("id-b"), QStringLiteral("/org/bluez/hci1/dev_BB"),
         QStringLiteral("BB"), QStringLiteral("Keyboard"), true, false, false, true,
         QStringLiteral("input-keyboard"), -40, -1},
    };
    backendPtr->publish(snapshot);
    QTRY_COMPARE(service.devicesModel()->rowCount(), 2);
    QCOMPARE(service.devicesModel()->data(service.devicesModel()->index(0, 0),
                                           BluetoothDeviceModel::NameRole).toString(),
             QStringLiteral("Mouse"));

    QVERIFY(service.requestScan(QStringLiteral("topbar")));
    snapshot.scanning = true;
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(service.scanning(), 500);
    QVERIFY(service.requestScan(QStringLiteral("popup")));
    QCOMPARE(backendPtr->startDiscoveryCount, 1);
    service.releaseScan(QStringLiteral("topbar"));
    QCOMPARE(backendPtr->stopDiscoveryCount, 0);
    service.releaseScan(QStringLiteral("popup"));
    QCOMPARE(backendPtr->stopDiscoveryCount, 1);
    service.releaseScan(QStringLiteral("popup"));
}

void SystemServicesTest::bluetoothRejectsUnpairedConnect()
{
    auto backend = std::make_unique<FakeBluetoothBackend>();
    auto *backendPtr = backend.get();
    BluetoothService service(std::move(backend));
    QVERIFY(service.start());

    BluetoothSnapshot snapshot;
    snapshot.daemonAvailable = true;
    snapshot.adapterAvailable = true;
    snapshot.adapterPath = QStringLiteral("/org/bluez/hci0");
    snapshot.powered = true;
    snapshot.devices = {{QStringLiteral("device-a"),
                         QStringLiteral("/org/bluez/hci0/dev_AA"),
                         QStringLiteral("AA"), QStringLiteral("Mouse"), false, false,
                         false, true, QStringLiteral("input-mouse"), -20, 80}};
    backendPtr->publish(snapshot);
    QTRY_COMPARE_WITH_TIMEOUT(service.devicesModel()->rowCount(), 1, 500);

    QVERIFY(!service.connectDevice(QStringLiteral("/org/bluez/hci0/dev_AA")));
    QCOMPARE(backendPtr->connectCount, 0);
}

void SystemServicesTest::bluetoothScanningFollowsBackendState()
{
    auto backend = std::make_unique<FakeBluetoothBackend>();
    auto *backendPtr = backend.get();
    BluetoothService service(std::move(backend));
    QVERIFY(service.start());

    BluetoothSnapshot snapshot;
    snapshot.daemonAvailable = true;
    snapshot.adapterAvailable = true;
    snapshot.adapterPath = QStringLiteral("/org/bluez/hci0");
    snapshot.powered = true;
    snapshot.scanning = false;
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(service.adapterAvailable(), 500);

    QVERIFY(service.requestScan(QStringLiteral("topbar")));
    QVERIFY(!service.scanning());
    snapshot.scanning = true;
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(service.scanning(), 500);
}

void SystemServicesTest::bluetoothPowerWaitsForAuthoritativeState()
{
    auto backend = std::make_unique<FakeBluetoothBackend>();
    auto *backendPtr = backend.get();
    BluetoothService service(std::move(backend));
    QVERIFY(service.start());

    BluetoothSnapshot snapshot;
    snapshot.daemonAvailable = true;
    snapshot.adapterAvailable = true;
    snapshot.adapterPath = QStringLiteral("/org/bluez/hci0");
    snapshot.powered = false;
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(service.adapterAvailable(), 500);

    QVERIFY(service.setPowered(true));
    QVERIFY(service.powerPending());
    snapshot.powered = false;
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(service.powerPending(), 500);
    snapshot.powered = true;
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(!service.powerPending(), 500);
}

void SystemServicesTest::networkRatesUseReadableUnits()
{
    QCOMPARE(NetworkService::formatRate(0.0), QStringLiteral("0 B/s"));
    QCOMPARE(NetworkService::formatRate(1000.0), QStringLiteral("1.0 KB/s"));
    QCOMPARE(NetworkService::formatRate(1'000'000.0), QStringLiteral("1.0 MB/s"));
    QCOMPARE(NetworkService::formatRate(1'000'000'000.0), QStringLiteral("1.0 GB/s"));
}

void SystemServicesTest::healthJsonContainsNoSecrets()
{
    NetworkService service(std::make_unique<FakeNetworkBackend>());
    const QJsonObject health = service.healthJson();
    QVERIFY(health.contains(QStringLiteral("state")));
    QVERIFY(health.contains(QStringLiteral("available")));
    QVERIFY(!health.keys().contains(QStringLiteral("password")));
    QVERIFY(!health.keys().contains(QStringLiteral("secret")));
}

QTEST_GUILESS_MAIN(SystemServicesTest)
#include "SystemServicesTest.moc"
