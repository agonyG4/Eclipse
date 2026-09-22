#include "system/SystemServiceState.hpp"
#include "system/audio/AudioService.hpp"
#include "system/audio/AudioOutputModel.hpp"
#include "system/audio/PipeWireAudioState.hpp"
#include "system/network/NetworkService.hpp"
#include "system/network/WifiNetworkModel.hpp"
#include "system/network/NetworkManagerState.hpp"
#include "system/bluetooth/BluetoothService.hpp"
#include "system/bluetooth/BluetoothDeviceModel.hpp"

#include <QDBusObjectPath>
#include <QJsonObject>
#include <QCoreApplication>
#include <QSet>
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

    Callbacks currentCallbacks() const { return m_callbacks; }

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

    bool setWifiEnabled(bool enabled, quint64 requestId) override
    {
        wifiEnabled = enabled;
        wifiRequestIds.append(requestId);
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

    void finish(NetworkOperationKind kind, quint64 requestId, bool success,
                const QString &error = {})
    {
        if (m_callbacks.operationFinished)
            m_callbacks.operationFinished({kind, requestId, success, error});
    }

    bool startResult = true;
    bool actionResult = true;
    int startCount = 0;
    int stopCount = 0;
    int scanCount = 0;
    bool wifiEnabled = false;
    QVector<quint64> wifiRequestIds;

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

    bool requestScan(const QString &owner) override
    {
        scanOwners.insert(owner);
        return actionResult;
    }

    void releaseScan(const QString &owner) override
    {
        scanOwners.remove(owner);
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

    bool pairDevice(const QString &path) override
    {
        pairedPath = path;
        return actionResult;
    }

    bool cancelPairing() override
    {
        ++cancelPairingCount;
        return actionResult;
    }

    bool setDeviceTrusted(const QString &path, bool trusted) override
    {
        trustedPath = path;
        trustedValue = trusted;
        return actionResult;
    }

    bool forgetDevice(const QString &path) override
    {
        forgottenPath = path;
        return actionResult;
    }

    bool submitAgentText(quint64 requestId, const QString &text) override
    {
        agentRequestId = requestId;
        agentText = text;
        return actionResult;
    }

    bool confirmAgentRequest(quint64 requestId, bool accepted) override
    {
        agentRequestId = requestId;
        agentAccepted = accepted;
        return actionResult;
    }

    bool rejectAgentRequest(quint64 requestId) override
    {
        agentRequestId = requestId;
        ++rejectAgentCount;
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
    int connectCount = 0;
    bool poweredValue = false;
    QSet<QString> scanOwners;
    QString connectedPath;
    QString disconnectedPath;
    QString pairedPath;
    int cancelPairingCount = 0;
    QString trustedPath;
    bool trustedValue = false;
    QString forgottenPath;
    quint64 agentRequestId = 0;
    QString agentText;
    bool agentAccepted = false;
    int rejectAgentCount = 0;

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
    void audioLateAttemptCallbacksAreIgnored();
    void audioUsesCubicUiScaleAndStableModelOrder();
    void networkWifiAvailabilityAndScanningAreAuthoritative();
    void networkOldWirelessRefreshCannotCompleteNewRefresh();
    void networkPrimaryWifiReplacesFallbackDiscoveryAdapter();
    void networkPrimaryDeviceChangeReselectsWifiAdapter();
    void networkStateSeparatesWirelessAndPrimaryState();
    void networkScanUsesLastScanAndCoalesces();
    void networkLateRequestScanReplyCannotSettleNewRequest();
    void networkOldDeviceScanReplyRejectedAfterWifiSwitch();
    void networkScanRequestIdChangesAcrossRetries();
    void wifiModelDeduplicatesAndAvoidsSemanticReset();
    void pipewireStateUsesMetadataAndPerNodeCache();
    void pipewirePartialMutePatchPreservesVolume();
    void pipewirePartialVolumePatchPreservesMute();
    void pipewirePartialPatchPreservesChannelVolumes();
    void pipewireIncompleteDefaultStateIsUnavailable();
    void pipewireDefaultWriteDoesNotOptimisticallyMutateState();
    void bluetoothFacadeContractAndForwardsOperations();
    void bluetoothPhase2ContractAndForwardsOperations();
    void bluetoothHealthChangedCoversHealthJsonFields();
    void bluetoothBackendStartupFailureReportsUnavailable();
    void bluetoothDeviceModelPreservesOrderingAndRoles();
    void wifiPowerOldReplyCannotSettleReplacementRequest();
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

void SystemServicesTest::audioLateAttemptCallbacksAreIgnored()
{
    auto backend = std::make_unique<FakeAudioBackend>();
    auto *backendPtr = backend.get();
    AudioService service(std::move(backend));
    QVERIFY(service.start());
    const AudioBackend::Callbacks attemptA = backendPtr->currentCallbacks();
    attemptA.defaultStateChanged(false, false, QStringLiteral("attempt A failed"));
    QTRY_VERIFY_WITH_TIMEOUT(backendPtr->startCount >= 2, 1500);
    const AudioBackend::Callbacks attemptB = backendPtr->currentCallbacks();
    attemptA.volumeChanged(3.375, true);
    QCoreApplication::processEvents();
    QVERIFY(service.volume() != 150.0);
    attemptB.volumeChanged(0.125, false);
    QTRY_COMPARE_WITH_TIMEOUT(service.volume(), 50.0, 500);
    QVERIFY(!service.muted());
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

void SystemServicesTest::networkOldWirelessRefreshCannotCompleteNewRefresh()
{
    NetworkWirelessRefreshState refresh;
    refresh.begin(7, 1, {QStringLiteral("/device/wlan0")});
    refresh.begin(7, 2, {QStringLiteral("/device/wlan1"), QStringLiteral("/device/wlan2")});

    QVERIFY(refresh.acceptReply(7, 2, QStringLiteral("/device/wlan1")));
    QVERIFY(!refresh.complete());
    QVERIFY(!refresh.acceptReply(7, 1, QStringLiteral("/device/wlan0")));
    QVERIFY(!refresh.complete());
    QVERIFY(refresh.acceptReply(7, 2, QStringLiteral("/device/wlan2")));
    QVERIFY(refresh.complete());
}

void SystemServicesTest::networkPrimaryWifiReplacesFallbackDiscoveryAdapter()
{
    NetworkManagerState state;
    const auto wifi = [](const QString &interfaceName) {
        return QVariantMap{{QStringLiteral("DeviceType"), 2},
                           {QStringLiteral("Interface"), interfaceName}};
    };
    state.upsertDevice(QStringLiteral("/device/wlan0"), wifi(QStringLiteral("wlan0")));
    state.upsertDevice(QStringLiteral("/device/wlan1"), wifi(QStringLiteral("wlan1")));
    state.setWirelessProperties(QStringLiteral("/device/wlan0"),
                                {{QStringLiteral("ActiveAccessPoint"),
                                  QVariant::fromValue(QDBusObjectPath(QStringLiteral("/ap/0")))}});
    state.setWirelessProperties(QStringLiteral("/device/wlan1"),
                                {{QStringLiteral("ActiveAccessPoint"),
                                  QVariant::fromValue(QDBusObjectPath(QStringLiteral("/ap/1")))}});

    QCOMPARE(state.selectedWifiDevicePath(), QStringLiteral("/device/wlan0"));
    QCOMPARE(state.activeAccessPointPath(), QStringLiteral("/ap/0"));

    state.setPrimaryProperties(QStringLiteral("/active/connection"),
                               {{QStringLiteral("Devices"), QVariantList{
                                   QVariant::fromValue(QDBusObjectPath(QStringLiteral("/device/wlan1")))}}});
    QCOMPARE(state.selectedWifiDevicePath(), QStringLiteral("/device/wlan1"));
    QCOMPARE(state.activeAccessPointPath(), QStringLiteral("/ap/1"));
}

void SystemServicesTest::networkPrimaryDeviceChangeReselectsWifiAdapter()
{
    NetworkManagerState state;
    state.upsertDevice(QStringLiteral("/device/wlan0"),
                       {{QStringLiteral("DeviceType"), 2},
                        {QStringLiteral("Interface"), QStringLiteral("wlan0")} });
    state.upsertDevice(QStringLiteral("/device/wlan1"),
                       {{QStringLiteral("DeviceType"), 2},
                        {QStringLiteral("Interface"), QStringLiteral("wlan1")} });
    state.setWirelessProperties(QStringLiteral("/device/wlan0"),
                                {{QStringLiteral("ActiveAccessPoint"),
                                  QVariant::fromValue(QDBusObjectPath(QStringLiteral("/ap/0")))}});
    state.setWirelessProperties(QStringLiteral("/device/wlan1"),
                                {{QStringLiteral("ActiveAccessPoint"),
                                  QVariant::fromValue(QDBusObjectPath(QStringLiteral("/ap/1")))}});

    state.setPrimaryProperties(QStringLiteral("/active/connection"),
                               {{QStringLiteral("Devices"), QVariantList{
                                   QVariant::fromValue(QDBusObjectPath(QStringLiteral("/device/wlan1")))}}});
    QCOMPARE(state.selectedWifiDevicePath(), QStringLiteral("/device/wlan1"));
    state.updatePrimaryProperties(
        {{QStringLiteral("Devices"), QVariantList{
            QVariant::fromValue(QDBusObjectPath(QStringLiteral("/device/wlan0")))}}}, {});
    QCOMPARE(state.selectedWifiDevicePath(), QStringLiteral("/device/wlan0"));
    state.removeDevice(QStringLiteral("/device/wlan0"));
    QCOMPARE(state.selectedWifiDevicePath(), QStringLiteral("/device/wlan1"));
    QCOMPARE(state.activeAccessPointPath(), QStringLiteral("/ap/1"));
}

void SystemServicesTest::networkStateSeparatesWirelessAndPrimaryState()
{
    NetworkManagerState state;
    state.setManagerProperties({{QStringLiteral("WirelessEnabled"), true}});
    state.upsertDevice(QStringLiteral("/device/wifi"),
                       {{QStringLiteral("DeviceType"), 2},
                        {QStringLiteral("Interface"), QStringLiteral("wlan-test")},
                        {QStringLiteral("ActiveAccessPoint"),
                         QVariant::fromValue(QDBusObjectPath(QStringLiteral("/ap/active")))}});
    state.setWirelessProperties(QStringLiteral("/device/wifi"),
                                {{QStringLiteral("ActiveAccessPoint"),
                                  QVariant::fromValue(QDBusObjectPath(QStringLiteral("/ap/active")))},
                                 {QStringLiteral("LastScan"), qint64(10)}});
    state.upsertAccessPoint(QStringLiteral("/ap/active"),
                            {{QStringLiteral("Ssid"), QByteArray("Cafe")},
                             {QStringLiteral("Strength"), 40},
                             {QStringLiteral("HwAddress"), QStringLiteral("AA")}});
    state.setPrimaryProperties(QStringLiteral("/active/connection"),
                               {{QStringLiteral("Id"), QStringLiteral("Cafe")},
                                {QStringLiteral("State"), 2},
                                {QStringLiteral("Devices"), QVariantList{
                                    QVariant::fromValue(QDBusObjectPath(QStringLiteral("/device/wifi")))}}});
    const NetworkSnapshot snapshot = state.snapshot();
    QCOMPARE(snapshot.connectionType, NetworkConnectionType::Wifi);
    QVERIFY(snapshot.connected);
    QCOMPARE(snapshot.interfaceName, QStringLiteral("wlan-test"));
    QCOMPARE(snapshot.wifiNetworks.size(), 1);
    QVERIFY(snapshot.wifiNetworks.constFirst().active);
    QCOMPARE(state.activeAccessPointPath(), QStringLiteral("/ap/active"));
    const quint64 epochA = state.beginPrimaryRequest(QStringLiteral("/active/A"));
    const quint64 epochB = state.beginPrimaryRequest(QStringLiteral("/active/B"));
    QVERIFY(epochB > epochA);
    QVERIFY(!state.applyPrimaryReply(QStringLiteral("/active/A"), epochA,
                                     {{QStringLiteral("State"), 2}}));
    QVERIFY(state.applyPrimaryReply(QStringLiteral("/active/B"), epochB,
                                    {{QStringLiteral("State"), 2}}));
    QCOMPARE(state.primaryPath(), QStringLiteral("/active/B"));
}

void SystemServicesTest::networkScanUsesLastScanAndCoalesces()
{
    NetworkScanState scan;
    QVERIFY(scan.request(7, QStringLiteral("/wifi"), 100, 0, 41));
    QCOMPARE(scan.phase(), NetworkScanPhase::RequestPending);
    QVERIFY(scan.requestFinished(7, QStringLiteral("/wifi"), 41, true, 10));
    QCOMPARE(scan.phase(), NetworkScanPhase::WaitingForLastScan);
    QVERIFY(scan.request(7, QStringLiteral("/wifi"), 100, 20, 42));
    QVERIFY(scan.queuedDemand());
    QVERIFY(!scan.lastScanAdvanced(7, QStringLiteral("/wifi"), 100, 30));
    QVERIFY(scan.lastScanAdvanced(7, QStringLiteral("/wifi"), 101, 40));
    QCOMPARE(scan.phase(), NetworkScanPhase::Cooldown);
    QVERIFY(scan.cooldownExpired(3040));
    QCOMPARE(scan.phase(), NetworkScanPhase::Idle);
    QVERIFY(scan.request(7, QStringLiteral("/wifi"), 101, 3050, 43));
    scan.requestFinished(7, QStringLiteral("/wifi"), 43, false, 3060);
    QCOMPARE(scan.phase(), NetworkScanPhase::Idle);
    QVERIFY(scan.request(7, QStringLiteral("/wifi"), 101, 3070, 44));
    QVERIFY(scan.requestFinished(7, QStringLiteral("/wifi"), 44, true, 3080));
    QVERIFY(scan.request(7, QStringLiteral("/wifi"), 101, 3090, 45));
    scan.timeout(4000);
    QCOMPARE(scan.phase(), NetworkScanPhase::Cooldown);
    QVERIFY(scan.cooldownExpired(7000));
    QCOMPARE(scan.phase(), NetworkScanPhase::Idle);
    QVERIFY(scan.request(8, QStringLiteral("/wifi"), 200, 0, 46));
    scan.invalidate();
    QCOMPARE(scan.phase(), NetworkScanPhase::Idle);
    QVERIFY(!scan.lastScanAdvanced(8, QStringLiteral("/wifi"), 201, 500));
}

void SystemServicesTest::networkLateRequestScanReplyCannotSettleNewRequest()
{
    NetworkScanState scan;
    QVERIFY(scan.request(7, QStringLiteral("/wlan0"), 100, 0, 41));
    scan.timeout(3000);
    QVERIFY(scan.request(7, QStringLiteral("/wlan0"), 100, 3001, 42));
    QVERIFY(!scan.requestFinished(7, QStringLiteral("/wlan0"), 41, true, 3002));
    QCOMPARE(scan.phase(), NetworkScanPhase::RequestPending);
    QVERIFY(scan.requestFinished(7, QStringLiteral("/wlan0"), 42, true, 3003));
    QCOMPARE(scan.phase(), NetworkScanPhase::WaitingForLastScan);
}

void SystemServicesTest::networkOldDeviceScanReplyRejectedAfterWifiSwitch()
{
    NetworkScanState scan;
    QVERIFY(scan.request(7, QStringLiteral("/wlan0"), 100, 0, 51));
    scan.invalidate();
    QVERIFY(scan.request(7, QStringLiteral("/wlan1"), 200, 1, 52));
    QVERIFY(!scan.requestFinished(7, QStringLiteral("/wlan0"), 51, true, 2));
    QCOMPARE(scan.phase(), NetworkScanPhase::RequestPending);
    QVERIFY(scan.requestFinished(7, QStringLiteral("/wlan1"), 52, true, 3));
    QCOMPARE(scan.phase(), NetworkScanPhase::WaitingForLastScan);
}

void SystemServicesTest::networkScanRequestIdChangesAcrossRetries()
{
    NetworkScanState scan;
    QVERIFY(scan.request(7, QStringLiteral("/wifi"), 10, 0, 61));
    QCOMPARE(scan.requestId(), quint64(61));
    scan.timeout(3000);
    QVERIFY(scan.request(7, QStringLiteral("/wifi"), 10, 3001, 62));
    QVERIFY(scan.requestId() != quint64(61));
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

void SystemServicesTest::pipewireStateUsesMetadataAndPerNodeCache()
{
    PipeWireAudioState state;
    state.reset(11);
    PipeWireNodeAudioState first;
    first.nodeId = 4;
    first.output = output(4, QStringLiteral("sink-a"));
    first.volume = 0.125f;
    first.volumeKnown = true;
    state.upsertNode(first);
    PipeWireNodeAudioState second;
    second.nodeId = 9;
    second.output = output(9, QStringLiteral("sink-b"));
    second.volume = 1.0f;
    second.volumeKnown = true;
    second.muted = true;
    second.muteKnown = true;
    state.upsertNode(second);
    QCOMPARE(state.defaultNodeId(), quint32(0));
    state.setMetadataDefault(QStringLiteral("sink-b"));
    QCOMPARE(state.defaultNodeId(), quint32(9));
    float volume = 0.0f;
    bool muted = false;
    QVERIFY(state.defaultVolume(&volume, &muted));
    QCOMPARE(volume, 1.0f);
    QVERIFY(muted);
    state.removeNode(9);
    QCOMPARE(state.defaultNodeId(), quint32(0));
    state.reset(12);
    state.upsertNode(first);
    state.setMetadataDefault(QString(), 9);
    QCOMPARE(state.defaultNodeId(), quint32(0));
}

void SystemServicesTest::pipewirePartialMutePatchPreservesVolume()
{
    PipeWireAudioState state;
    state.reset(1);
    PipeWireNodeAudioState node;
    node.nodeId = 4;
    node.output = output(4, QStringLiteral("sink-a"));
    node.volume = 0.125f;
    node.volumeKnown = true;
    node.muted = false;
    node.muteKnown = true;
    state.upsertNode(node);
    state.setMetadataDefault(QStringLiteral("sink-a"), 4);

    PipeWireNodePropsPatch patch;
    patch.muted = true;
    state.applyNodePropsPatch(4, patch);
    float volume = 0.0f;
    bool muted = false;
    QVERIFY(state.defaultVolume(&volume, &muted));
    QCOMPARE(volume, 0.125f);
    QVERIFY(muted);
}

void SystemServicesTest::pipewirePartialVolumePatchPreservesMute()
{
    PipeWireAudioState state;
    state.reset(1);
    PipeWireNodeAudioState node;
    node.nodeId = 4;
    node.output = output(4, QStringLiteral("sink-a"));
    node.volume = 0.125f;
    node.volumeKnown = true;
    node.muted = true;
    node.muteKnown = true;
    state.upsertNode(node);
    state.setMetadataDefault(QStringLiteral("sink-a"), 4);

    PipeWireNodePropsPatch patch;
    patch.volume = 1.0f;
    state.applyNodePropsPatch(4, patch);
    float volume = 0.0f;
    bool muted = false;
    QVERIFY(state.defaultVolume(&volume, &muted));
    QCOMPARE(volume, 1.0f);
    QVERIFY(muted);
}

void SystemServicesTest::pipewirePartialPatchPreservesChannelVolumes()
{
    PipeWireAudioState state;
    state.reset(1);
    PipeWireNodeAudioState node;
    node.nodeId = 4;
    node.output = output(4, QStringLiteral("sink-a"));
    node.channelVolumes = {0.25f, 0.5f};
    node.channelVolumesKnown = true;
    node.muted = false;
    node.muteKnown = true;
    state.upsertNode(node);
    state.setMetadataDefault(QStringLiteral("sink-a"), 4);

    PipeWireNodePropsPatch patch;
    patch.muted = true;
    state.applyNodePropsPatch(4, patch);
    float volume = 0.0f;
    bool muted = false;
    QVERIFY(state.defaultVolume(&volume, &muted));
    QCOMPARE(volume, 0.375f);
    QVERIFY(muted);
}

void SystemServicesTest::pipewireIncompleteDefaultStateIsUnavailable()
{
    PipeWireAudioState state;
    state.reset(1);
    PipeWireNodeAudioState node;
    node.nodeId = 4;
    node.output = output(4, QStringLiteral("sink-a"));
    state.upsertNode(node);
    state.setMetadataDefault(QStringLiteral("sink-a"), 4);
    float volume = 0.0f;
    bool muted = false;
    QVERIFY(!state.defaultVolume(&volume, &muted));

    PipeWireNodePropsPatch muteOnly;
    muteOnly.muted = true;
    state.applyNodePropsPatch(4, muteOnly);
    QVERIFY(!state.defaultVolume(&volume, &muted));
    PipeWireNodePropsPatch volumeOnly;
    volumeOnly.volume = 1.0f;
    state.applyNodePropsPatch(4, volumeOnly);
    QVERIFY(state.defaultVolume(&volume, &muted));
}

void SystemServicesTest::pipewireDefaultWriteDoesNotOptimisticallyMutateState()
{
    PipeWireAudioState state;
    state.reset(1);
    PipeWireNodeAudioState first;
    first.nodeId = 4;
    first.output = output(4, QStringLiteral("sink-a"));
    state.upsertNode(first);
    PipeWireNodeAudioState second;
    second.nodeId = 9;
    second.output = output(9, QStringLiteral("sink-b"));
    state.upsertNode(second);
    state.setMetadataDefault(QStringLiteral("sink-a"), 4);

    QVERIFY(!state.requestMetadataDefault(9, false, [](const QString &) { return true; }));
    QCOMPARE(state.defaultNodeId(), quint32(4));
    QVERIFY(!state.requestMetadataDefault(9, true, [](const QString &) { return false; }));
    QCOMPARE(state.defaultNodeId(), quint32(4));
    QVERIFY(state.requestMetadataDefault(9, true, [](const QString &) { return true; }));
    QCOMPARE(state.defaultNodeId(), quint32(4));
    state.setMetadataDefault(QStringLiteral("sink-b"), 9);
    QCOMPARE(state.defaultNodeId(), quint32(9));
}

void SystemServicesTest::bluetoothFacadeContractAndForwardsOperations()
{
    auto backend = std::make_unique<FakeBluetoothBackend>();
    auto *backendPtr = backend.get();
    BluetoothService service(std::move(backend));

    QCOMPARE(service.state(), SystemServiceState::Stopped);
    QVERIFY(service.start());
    QCOMPARE(service.state(), SystemServiceState::Starting);
    QCOMPARE(backendPtr->startCount, 1);

    BluetoothSnapshot snapshot;
    snapshot.state = SystemServiceState::Ready;
    snapshot.available = true;
    snapshot.ready = true;
    snapshot.adapterAvailable = true;
    snapshot.adapterPath = QStringLiteral("/org/bluez/hci0");
    snapshot.adapterName = QStringLiteral("Astrea");
    BluetoothDevice headphones;
    headphones.id = QStringLiteral("/org/bluez/hci0/dev_AA");
    headphones.objectPath = headphones.id;
    headphones.address = QStringLiteral("AA:BB:CC:DD:EE:FF");
    headphones.name = QStringLiteral("Headphones");
    headphones.paired = true;
    headphones.discovered = true;
    headphones.rssi = -42;
    headphones.batteryPercent = 76;
    snapshot.devices.append(headphones);
    backendPtr->publish(snapshot);

    QCOMPARE(service.state(), SystemServiceState::Ready);
    QVERIFY(service.available());
    QVERIFY(service.ready());
    QVERIFY(service.adapterAvailable());
    QCOMPARE(service.adapterPath(), snapshot.adapterPath);
    QCOMPARE(service.adapterName(), snapshot.adapterName);
    QCOMPARE(service.devicesModel()->rowCount(), 1);
    QCOMPARE(service.devicesModel()->data(service.devicesModel()->index(0, 0),
                                         BluetoothDeviceModel::AddressRole).toString(),
             headphones.address);
    QCOMPARE(service.devicesModel()->data(service.devicesModel()->index(0, 0),
                                         BluetoothDeviceModel::RssiRole).toInt(), -42);
    QCOMPARE(service.devicesModel()->data(service.devicesModel()->index(0, 0),
                                         BluetoothDeviceModel::BatteryPercentRole).toInt(), 76);

    QVERIFY(service.setPowered(true));
    QVERIFY(backendPtr->poweredValue);
    QVERIFY(!service.powered());
    snapshot.powerPending = true;
    backendPtr->publish(snapshot);
    QVERIFY(service.powerPending());

    QVERIFY(service.requestScan(QStringLiteral("topbar")));
    QVERIFY(service.requestScan(QStringLiteral("bluetooth-popup")));
    QCOMPARE(backendPtr->scanOwners.size(), 2);
    service.releaseScan(QStringLiteral("topbar"));
    QCOMPARE(backendPtr->scanOwners, QSet<QString>{QStringLiteral("bluetooth-popup")});

    QVERIFY(service.connectDevice(headphones.objectPath));
    QCOMPARE(backendPtr->connectedPath, headphones.objectPath);
    QVERIFY(service.disconnectDevice(headphones.objectPath));
    QCOMPARE(backendPtr->disconnectedPath, headphones.objectPath);

    const QJsonObject health = service.healthJson();
    QVERIFY(health.value(QStringLiteral("available")).toBool());
    QVERIFY(health.value(QStringLiteral("ready")).toBool());
    QVERIFY(health.value(QStringLiteral("adapterAvailable")).toBool());
    QVERIFY(!health.contains(QStringLiteral("address")));

    service.stop();
    QCOMPARE(service.state(), SystemServiceState::Stopped);
    QCOMPARE(service.devicesModel()->rowCount(), 0);
    QCOMPARE(backendPtr->stopCount, 1);
}

void SystemServicesTest::bluetoothBackendStartupFailureReportsUnavailable()
{
    auto backend = std::make_unique<FakeBluetoothBackend>();
    backend->startResult = false;
    BluetoothService service(std::move(backend));

    QVERIFY(service.start());
    QCOMPARE(service.state(), SystemServiceState::Unavailable);
    QVERIFY(!service.available());
    QVERIFY(!service.ready());
    QCOMPARE(service.errorString(), QStringLiteral("Bluetooth backend unavailable"));
}

void SystemServicesTest::bluetoothPhase2ContractAndForwardsOperations()
{
    auto backend = std::make_unique<FakeBluetoothBackend>();
    auto *backendPtr = backend.get();
    BluetoothService service(std::move(backend));
    QVERIFY(service.start());

    QSignalSpy pairingSpy(&service, &BluetoothService::pairingChanged);
    QSignalSpy agentSpy(&service, &BluetoothService::agentRequestChanged);
    QSignalSpy operationSpy(&service, &BluetoothService::operationChanged);

    BluetoothSnapshot snapshot;
    snapshot.pairing = true;
    snapshot.pairingDevicePath = QStringLiteral("/org/bluez/hci0/dev_AA");
    snapshot.pairingDeviceName = QStringLiteral("Headphones");
    snapshot.pairingError = QStringLiteral("Pairing rejected");
    snapshot.agentRequestActive = true;
    snapshot.agentRequestId = 42;
    snapshot.agentRequestKind = BluetoothAgentRequestKind::PasskeyConfirmation;
    snapshot.agentDevicePath = snapshot.pairingDevicePath;
    snapshot.agentDeviceName = snapshot.pairingDeviceName;
    snapshot.agentPasskey = 7;
    snapshot.agentEntered = 2;
    snapshot.agentServiceUuid = QStringLiteral("0000110e-0000-1000-8000-00805f9b34fb");
    snapshot.agentDisplayPin = QStringLiteral("1234");
    snapshot.operationError = QStringLiteral("Trust rejected");
    backendPtr->publish(snapshot);

    QVERIFY(service.pairing());
    QCOMPARE(service.pairingDevicePath(), snapshot.pairingDevicePath);
    QCOMPARE(service.pairingDeviceName(), snapshot.pairingDeviceName);
    QCOMPARE(service.pairingError(), snapshot.pairingError);
    QCOMPARE(service.agentRequestId(), quint64(42));
    QCOMPARE(service.agentRequestKind(), BluetoothAgentRequestKind::PasskeyConfirmation);
    QCOMPARE(service.agentPasskey(), quint32(7));
    QCOMPARE(service.agentEntered(), 2);
    QVERIFY(pairingSpy.count() == 1);
    QVERIFY(agentSpy.count() == 1);
    QVERIFY(operationSpy.count() == 1);

    const QString path = snapshot.pairingDevicePath;
    QVERIFY(service.pairDevice(path));
    QCOMPARE(backendPtr->pairedPath, path);
    QVERIFY(service.cancelPairing());
    QCOMPARE(backendPtr->cancelPairingCount, 1);
    QVERIFY(service.setDeviceTrusted(path, true));
    QCOMPARE(backendPtr->trustedPath, path);
    QVERIFY(backendPtr->trustedValue);
    QVERIFY(service.forgetDevice(path));
    QCOMPARE(backendPtr->forgottenPath, path);
    QVERIFY(service.submitAgentText(42, QStringLiteral("123456")));
    QCOMPARE(backendPtr->agentRequestId, quint64(42));
    QCOMPARE(backendPtr->agentText, QStringLiteral("123456"));
    QVERIFY(service.confirmAgentRequest(42, true));
    QVERIFY(backendPtr->agentAccepted);
    QVERIFY(service.rejectAgentRequest(42));
    QCOMPARE(backendPtr->rejectAgentCount, 1);

    const QJsonObject health = service.healthJson();
    QVERIFY(!health.contains(QStringLiteral("pairingError")));
    QVERIFY(!health.contains(QStringLiteral("agentDisplayPin")));
}

void SystemServicesTest::bluetoothHealthChangedCoversHealthJsonFields()
{
    auto backend = std::make_unique<FakeBluetoothBackend>();
    auto *backendPtr = backend.get();
    BluetoothService service(std::move(backend));
    QVERIFY(service.start());

    QSignalSpy healthSpy(&service, &BluetoothService::healthChanged);
    BluetoothSnapshot snapshot;

    const auto publishAndExpectHealth = [&] {
        backendPtr->publish(snapshot);
        QCOMPARE(healthSpy.count(), 1);
        healthSpy.clear();
    };

    snapshot.available = true;
    publishAndExpectHealth();
    snapshot.ready = true;
    publishAndExpectHealth();
    snapshot.state = SystemServiceState::Ready;
    publishAndExpectHealth();
    snapshot.errorString = QStringLiteral("BlueZ unavailable");
    publishAndExpectHealth();
    snapshot.adapterAvailable = true;
    publishAndExpectHealth();
    snapshot.powered = true;
    publishAndExpectHealth();
    snapshot.scanning = true;
    publishAndExpectHealth();
    snapshot.connectedCount = 1;
    publishAndExpectHealth();

    snapshot.adapterName = QStringLiteral("Astrea");
    backendPtr->publish(snapshot);
    QCOMPARE(healthSpy.count(), 0);
}

void SystemServicesTest::bluetoothDeviceModelPreservesOrderingAndRoles()
{
    BluetoothDeviceModel model;
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    BluetoothDevice availableB;
    availableB.id = QStringLiteral("available-b");
    availableB.address = QStringLiteral("BB");
    availableB.name = QStringLiteral("beta");
    availableB.discovered = true;
    BluetoothDevice availableA = availableB;
    availableA.id = QStringLiteral("available-a");
    availableA.address = QStringLiteral("AA");
    availableA.name = QStringLiteral("Beta");
    BluetoothDevice paired;
    paired.id = QStringLiteral("paired");
    paired.address = QStringLiteral("CC");
    paired.name = QStringLiteral("Alpha");
    paired.paired = true;
    BluetoothDevice connected;
    connected.id = QStringLiteral("connected");
    connected.address = QStringLiteral("DD");
    connected.name = QStringLiteral("Zeta");
    connected.connected = true;

    model.replace({availableB, paired, connected, availableA});
    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.data(model.index(0, 0), BluetoothDeviceModel::IdRole).toString(),
             QStringLiteral("connected"));
    QCOMPARE(model.data(model.index(1, 0), BluetoothDeviceModel::IdRole).toString(),
             QStringLiteral("paired"));
    QCOMPARE(model.data(model.index(2, 0), BluetoothDeviceModel::IdRole).toString(),
             QStringLiteral("available-a"));
    QCOMPARE(model.data(model.index(3, 0), BluetoothDeviceModel::IdRole).toString(),
             QStringLiteral("available-b"));
    QCOMPARE(model.data(model.index(1, 0), BluetoothDeviceModel::PairedRole).toBool(), true);
    QCOMPARE(model.data(model.index(2, 0), BluetoothDeviceModel::DiscoveredRole).toBool(), true);
    const auto roles = model.roleNames();
    QCOMPARE(roles.value(BluetoothDeviceModel::IdRole), QByteArray("id"));
    QCOMPARE(roles.value(BluetoothDeviceModel::ObjectPathRole), QByteArray("objectPath"));
    QCOMPARE(roles.value(BluetoothDeviceModel::AddressRole), QByteArray("address"));
    QCOMPARE(roles.value(BluetoothDeviceModel::NameRole), QByteArray("name"));
    QCOMPARE(roles.value(BluetoothDeviceModel::PairedRole), QByteArray("paired"));
    QCOMPARE(roles.value(BluetoothDeviceModel::TrustedRole), QByteArray("trusted"));
    QCOMPARE(roles.value(BluetoothDeviceModel::ConnectedRole), QByteArray("connected"));
    QCOMPARE(roles.value(BluetoothDeviceModel::DiscoveredRole), QByteArray("discovered"));
    QCOMPARE(roles.value(BluetoothDeviceModel::IconRole), QByteArray("icon"));
    QCOMPARE(roles.value(BluetoothDeviceModel::RssiRole), QByteArray("rssi"));
    QCOMPARE(roles.value(BluetoothDeviceModel::BatteryPercentRole), QByteArray("batteryPercent"));

    model.replace({availableB, paired, connected, availableA});
    QCOMPARE(resetSpy.count(), 1);

    availableA.connected = true;
    model.replace({availableB, paired, connected, availableA});
    QCOMPARE(resetSpy.count(), 2);
    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.data(model.index(0, 0), BluetoothDeviceModel::IdRole).toString(),
             QStringLiteral("available-a"));
    QCOMPARE(model.data(model.index(0, 0), BluetoothDeviceModel::ConnectedRole).toBool(), true);

    model.replace({availableB, paired, connected});
    QCOMPARE(resetSpy.count(), 3);
    QCOMPARE(model.rowCount(), 3);
}

void SystemServicesTest::wifiPowerOldReplyCannotSettleReplacementRequest()
{
    auto backend = std::make_unique<FakeNetworkBackend>();
    auto *backendPtr = backend.get();
    NetworkService service(std::move(backend));
    QVERIFY(service.start());
    NetworkSnapshot snapshot;
    snapshot.daemonAvailable = true;
    snapshot.wifiAvailable = true;
    snapshot.wifiEnabled = false;
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(service.wifiAvailable(), 500);

    QVERIFY(service.setWifiEnabled(true));
    const quint64 onRequest = backendPtr->wifiRequestIds.constLast();
    QVERIFY(service.setWifiEnabled(false));
    const quint64 offRequest = backendPtr->wifiRequestIds.constLast();
    QVERIFY(offRequest != onRequest);
    backendPtr->finish(NetworkOperationKind::WifiEnabled, onRequest, false,
                       QStringLiteral("old Wi-Fi request failed"));
    QCoreApplication::processEvents();
    QVERIFY(service.wifiPending());
    backendPtr->finish(NetworkOperationKind::WifiEnabled, offRequest, true);
    snapshot.wifiEnabled = false;
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(!service.wifiPending(), 500);
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
