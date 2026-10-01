#include "system/SystemServiceState.hpp"
#include "system/audio/AudioService.hpp"
#include "system/audio/AudioOutputModel.hpp"
#include "system/audio/PipeWireAudioState.hpp"
#include "system/audio/spatial/SpatialAudioBlockBridge.hpp"
#include "system/network/NetworkService.hpp"
#include "system/network/WifiNetworkModel.hpp"
#include "system/network/NetworkManagerState.hpp"
#include "system/bluetooth/BluetoothService.hpp"
#include "system/bluetooth/BluetoothDeviceModel.hpp"

#include <QDBusObjectPath>
#include <QHash>
#include <QJsonObject>
#include <QCoreApplication>
#include <QSet>
#include <QSignalSpy>
#include <QTest>

#include <array>
#include <memory>
#include <string_view>
#include <utility>

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
        ++setVolumeCount;
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
    int setVolumeCount = 0;
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

    bool applyDns(NetworkDnsMode mode, const QStringList &servers, quint64 requestId,
                  QString *errorOut) override
    {
        Q_UNUSED(errorOut)
        dnsRequests.append({mode, servers, requestId});
        return actionResult;
    }

    void cancelDnsOperation(quint64 requestId) override
    {
        cancelledDnsRequests.append(requestId);
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

    void finishDns(quint64 requestId, NetworkDnsOutcome outcome, const QString &error = {})
    {
        if (!m_callbacks.operationFinished)
            return;
        NetworkOperationResult result;
        result.kind = NetworkOperationKind::DnsConfiguration;
        result.requestId = requestId;
        result.dnsOutcome = outcome;
        result.success = outcome == NetworkDnsOutcome::Applied;
        result.error = error;
        m_callbacks.operationFinished(result);
    }

    bool startResult = true;
    bool actionResult = true;
    int startCount = 0;
    int stopCount = 0;
    int scanCount = 0;
    bool wifiEnabled = false;
    QVector<quint64> wifiRequestIds;
    struct DnsRequest {
        NetworkDnsMode mode;
        QStringList servers;
        quint64 requestId;
    };
    QVector<DnsRequest> dnsRequests;
    QVector<quint64> cancelledDnsRequests;

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

struct FakeSpatialProcessor {
    int calls = 0;
    bool shouldFail = false;
};

int processSpatialFake(void *context, const float *const *inputs, float *const *outputs,
                       std::uint32_t frames)
{
    auto *fake = static_cast<FakeSpatialProcessor *>(context);
    ++fake->calls;
    if (fake->shouldFail)
        return 1;
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        outputs[0][frame] = inputs[2][frame];
        outputs[1][frame] = inputs[7][frame];
    }
    return 0;
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
    void audioAbsoluteVolumeMutationClampsAndUsesCubicScale();
    void audioAbsoluteVolumeRequiresAuthoritativeStateAndBackendAcceptance();
    void audioOutputModelUsesHumanNamesAndDeterministicSorting();
    void networkWifiAvailabilityAndScanningAreAuthoritative();
    void networkOldWirelessRefreshCannotCompleteNewRefresh();
    void networkPrimaryWifiReplacesFallbackDiscoveryAdapter();
    void networkPrimaryDeviceChangeReselectsWifiAdapter();
    void networkStateSeparatesWirelessAndPrimaryState();
    void networkSnapshotProjectsHardwareAndAccessPointSecurity();
    void networkSnapshotProjectsPrimaryRuntimeDetails();
    void networkProfileAndDnsProjectionAreIdentityQualified();
    void networkPrimaryRuntimeDetailsArePathQualifiedAndCleared();
    void networkServiceProjectsAndClearsPrimaryRuntimeDetails();
    void networkServiceProjectsActiveAndConfiguredDns();
    void networkDnsOperationIsIndependentAndDoesNotDegradeHealth();
    void networkDnsTimeoutRejectsLateCompletion();
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
    void spatialAudioPipeWireBoundaryUsesCanonicalPlanarPortsAndSilencesErrors();
    void bluetoothFacadeContractAndForwardsOperations();
    void bluetoothPhase2ContractAndForwardsOperations();
    void bluetoothServiceCategoryIsCarriedAndNotified();
    void bluetoothDeviceKindIsCarriedAndNotified();
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

void SystemServicesTest::audioAbsoluteVolumeMutationClampsAndUsesCubicScale()
{
    auto backend = std::make_unique<FakeAudioBackend>();
    auto *backendPtr = backend.get();
    AudioService service(std::move(backend));
    QVERIFY(service.start());

    backendPtr->publish({output(9, QStringLiteral("speakers"))}, 9);
    QTRY_COMPARE_WITH_TIMEOUT(service.outputsModel()->rowCount(), 1, 500);
    backendPtr->currentCallbacks().volumeChanged(1.0, false);
    QTRY_VERIFY_WITH_TIMEOUT(service.defaultStateAvailable(), 500);
    QCOMPARE(service.volume(), 100.0);

    QSignalSpy volumeSpy(&service, &AudioService::volumeChanged);
    QVERIFY(service.setVolume(50.0));
    QCOMPARE(backendPtr->setVolumeCount, 1);
    QCOMPARE(backendPtr->volumeNodeId, quint32(9));
    QCOMPARE(backendPtr->volumeLinear, 0.125);
    QCOMPARE(service.volume(), 50.0);
    QCOMPARE(volumeSpy.count(), 1);

    QVERIFY(service.setVolume(50.0));
    QCOMPARE(backendPtr->setVolumeCount, 1);
    QCOMPARE(volumeSpy.count(), 1);

    QVERIFY(service.setVolume(-20.0));
    QCOMPARE(backendPtr->volumeLinear, 0.0);
    QCOMPARE(service.volume(), 0.0);
    QVERIFY(service.setVolume(180.0));
    QCOMPARE(backendPtr->volumeLinear, 3.375);
    QCOMPARE(service.volume(), 150.0);

    service.adjustVolume(-75.0);
    QCOMPARE(backendPtr->setVolumeCount, 4);
    QCOMPARE(backendPtr->volumeLinear, 0.421875);
    QCOMPARE(service.volume(), 75.0);
}

void SystemServicesTest::audioAbsoluteVolumeRequiresAuthoritativeStateAndBackendAcceptance()
{
    auto noDefaultBackend = std::make_unique<FakeAudioBackend>();
    auto *noDefaultBackendPtr = noDefaultBackend.get();
    AudioService noDefaultService(std::move(noDefaultBackend));
    QVERIFY(noDefaultService.start());
    QSignalSpy noDefaultVolumeSpy(&noDefaultService, &AudioService::volumeChanged);
    QVERIFY(!noDefaultService.setVolume(75.0));
    QCOMPARE(noDefaultBackendPtr->setVolumeCount, 0);
    QCOMPARE(noDefaultService.volume(), 100.0);
    QCOMPARE(noDefaultVolumeSpy.count(), 0);

    auto incompleteBackend = std::make_unique<FakeAudioBackend>();
    auto *incompleteBackendPtr = incompleteBackend.get();
    AudioService incompleteService(std::move(incompleteBackend));
    QVERIFY(incompleteService.start());
    incompleteBackendPtr->publish({output(11, QStringLiteral("headphones"))}, 11);
    QTRY_COMPARE_WITH_TIMEOUT(incompleteService.outputsModel()->rowCount(), 1, 500);
    QVERIFY(!incompleteService.defaultStateAvailable());
    QVERIFY(!incompleteService.setVolume(75.0));
    QCOMPARE(incompleteBackendPtr->setVolumeCount, 0);
    QCOMPARE(incompleteService.volume(), 100.0);

    auto rejectedBackend = std::make_unique<FakeAudioBackend>();
    auto *rejectedBackendPtr = rejectedBackend.get();
    AudioService rejectedService(std::move(rejectedBackend));
    QVERIFY(rejectedService.start());
    rejectedBackendPtr->publish({output(13, QStringLiteral("monitor"))}, 13);
    QTRY_COMPARE_WITH_TIMEOUT(rejectedService.outputsModel()->rowCount(), 1, 500);
    rejectedBackendPtr->currentCallbacks().volumeChanged(0.125, false);
    QTRY_VERIFY_WITH_TIMEOUT(rejectedService.defaultStateAvailable(), 500);
    rejectedBackendPtr->actionResult = false;
    QSignalSpy rejectedVolumeSpy(&rejectedService, &AudioService::volumeChanged);

    QVERIFY(!rejectedService.setVolume(75.0));
    QCOMPARE(rejectedBackendPtr->setVolumeCount, 1);
    QCOMPARE(rejectedService.volume(), 50.0);
    QCOMPARE(rejectedVolumeSpy.count(), 0);
}

void SystemServicesTest::audioOutputModelUsesHumanNamesAndDeterministicSorting()
{
    AudioOutputModel model;
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

    AudioOutput technical(5, QStringLiteral("technical.name"),
                          QStringLiteral("   "), QString(), QStringLiteral("Audio/Sink"),
                          false, true);
    AudioOutput speakers(3, QStringLiteral("alsa_output.usb-desk"), QString(),
                         QStringLiteral(" Desk Speakers "), QStringLiteral("Audio/Sink"),
                         false, false);
    AudioOutput dac(8, QStringLiteral("alsa_output.usb-dac"),
                    QStringLiteral(" USB DAC "), QStringLiteral("DAC"),
                    QStringLiteral("Audio/Sink"), false, false);
    AudioOutput defaultOutput(2, QStringLiteral("alsa_output.default"),
                              QStringLiteral(" Default output "), QString(),
                              QStringLiteral("Audio/Sink"), false, false);

    model.replace({technical, dac, defaultOutput, speakers}, 2);
    QCOMPARE(model.rowCount(), 4);
    const auto displayName = [&model](int row) {
        return model.data(model.index(row, 0), AudioOutputModel::DisplayNameRole).toString();
    };
    QCOMPARE(displayName(0), QStringLiteral("Default output"));
    QCOMPARE(displayName(1), QStringLiteral("Desk Speakers"));
    QCOMPARE(displayName(2), QStringLiteral("technical.name"));
    QCOMPARE(displayName(3), QStringLiteral("USB DAC"));
    QCOMPARE(model.data(model.index(3, 0), AudioOutputModel::NameRole).toString(),
             QStringLiteral("alsa_output.usb-dac"));
    QCOMPARE(model.data(model.index(3, 0), AudioOutputModel::DescriptionRole).toString(),
             QStringLiteral(" USB DAC "));
    QCOMPARE(model.data(model.index(3, 0), AudioOutputModel::NickRole).toString(),
             QStringLiteral("DAC"));
    QVERIFY(model.data(model.index(2, 0), AudioOutputModel::VirtualRole).toBool());
    QCOMPARE(model.roleNames().value(AudioOutputModel::DisplayNameRole), QByteArray("displayName"));

    model.replace({speakers, technical, defaultOutput, dac}, 2);
    QCOMPARE(resetSpy.count(), 1);

    AudioOutput sameNameHigherId(21, QStringLiteral("node-a"), QStringLiteral("Alpha"), {},
                                 QStringLiteral("Audio/Sink"), false, false);
    AudioOutput sameNameLowerId(19, QStringLiteral("node-b"), QStringLiteral("alpha"), {},
                                QStringLiteral("Audio/Sink"), false, false);
    model.replace({sameNameHigherId, sameNameLowerId}, 0);
    QCOMPARE(model.data(model.index(0, 0), AudioOutputModel::NodeIdRole).toUInt(), quint32(19));
    QCOMPARE(model.data(model.index(1, 0), AudioOutputModel::NodeIdRole).toUInt(), quint32(21));
}

void SystemServicesTest::networkWifiAvailabilityAndScanningAreAuthoritative()
{
    auto backend = std::make_unique<FakeNetworkBackend>();
    auto *backendPtr = backend.get();
    NetworkService service(std::move(backend));
    QVERIFY(service.start());
    QSignalSpy hardwareSpy(&service, &NetworkService::wifiHardwareEnabledChanged);

    NetworkSnapshot snapshot;
    snapshot.daemonAvailable = true;
    snapshot.wifiEnabled = true;
    snapshot.wifiHardwareEnabled = true;
    snapshot.wifiAvailable = false;
    snapshot.wifiScanning = false;
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(service.wifiEnabled(), 500);
    QVERIFY(service.wifiHardwareEnabled());
    QCOMPARE(hardwareSpy.count(), 1);
    QVERIFY(!service.wifiAvailable());
    QVERIFY(!service.wifiScanning());

    snapshot.wifiAvailable = true;
    snapshot.wifiHardwareEnabled = false;
    snapshot.wifiScanning = true;
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(service.wifiAvailable() && service.wifiScanning(), 500);
    QVERIFY(!service.wifiHardwareEnabled());
    QCOMPARE(hardwareSpy.count(), 2);
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
    state.setManagerProperties({{QStringLiteral("WirelessEnabled"), true},
                                {QStringLiteral("WirelessHardwareEnabled"), true}});
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
    QVERIFY(snapshot.wifiAvailable);
    QVERIFY(snapshot.wifiHardwareEnabled);
    QVERIFY(snapshot.wifiEnabled);
    QCOMPARE(snapshot.wifiNetworks.size(), 1);
    QVERIFY(snapshot.wifiNetworks.constFirst().active);
    QCOMPARE(snapshot.wifiNetworks.constFirst().accessPointPath,
             QStringLiteral("/ap/active"));
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

void SystemServicesTest::networkSnapshotProjectsHardwareAndAccessPointSecurity()
{
    constexpr uint accessPointWpsFlag = 0x00000002;
    NetworkManagerState state;
    state.setManagerProperties({{QStringLiteral("WirelessEnabled"), false},
                                {QStringLiteral("WirelessHardwareEnabled"), false}});
    state.upsertDevice(QStringLiteral("/device/wifi"),
                       {{QStringLiteral("DeviceType"), 2},
                        {QStringLiteral("Interface"), QStringLiteral("wlan-test")}});
    state.setWirelessProperties(QStringLiteral("/device/wifi"), {});
    const auto addPoint = [&state](const QString &path, const QByteArray &ssid, uint flags,
                                   uint wpaFlags, uint rsnFlags) {
        state.upsertAccessPoint(path,
                                {{QStringLiteral("Ssid"), ssid},
                                 {QStringLiteral("Strength"), 50},
                                 {QStringLiteral("Frequency"), 2412},
                                 {QStringLiteral("HwAddress"), path},
                                 {QStringLiteral("Flags"), flags},
                                 {QStringLiteral("WpaFlags"), wpaFlags},
                                 {QStringLiteral("RsnFlags"), rsnFlags}});
    };
    addPoint(QStringLiteral("/ap/open"), QByteArrayLiteral("Open"), 0, 0, 0);
    addPoint(QStringLiteral("/ap/wps"), QByteArrayLiteral("WPS"), accessPointWpsFlag, 0, 0);
    addPoint(QStringLiteral("/ap/privacy"), QByteArrayLiteral("Privacy"), 1, 0, 0);
    addPoint(QStringLiteral("/ap/wpa"), QByteArrayLiteral("WPA"), 0, 1, 0);
    addPoint(QStringLiteral("/ap/rsn"), QByteArrayLiteral("RSN"), 0, 0, 1);

    const NetworkSnapshot snapshot = state.snapshot();
    QVERIFY(snapshot.wifiAvailable);
    QVERIFY(!snapshot.wifiHardwareEnabled);
    QVERIFY(!snapshot.wifiEnabled);
    QCOMPARE(snapshot.wifiNetworks.size(), 5);

    QHash<QString, WifiNetwork> projectedBySsid;
    for (const WifiNetwork &network : snapshot.wifiNetworks)
        projectedBySsid.insert(network.ssid, network);
    QVERIFY(!projectedBySsid.value(QStringLiteral("Open")).secured);
    QVERIFY(!projectedBySsid.value(QStringLiteral("WPS")).secured);
    QVERIFY(projectedBySsid.value(QStringLiteral("Privacy")).secured);
    QVERIFY(projectedBySsid.value(QStringLiteral("WPA")).secured);
    QVERIFY(projectedBySsid.value(QStringLiteral("RSN")).secured);
    QCOMPARE(projectedBySsid.value(QStringLiteral("WPS")).accessPointPath,
             QStringLiteral("/ap/wps"));

    WifiNetworkModel model;
    model.replace(snapshot.wifiNetworks);
    const auto roleNames = model.roleNames();
    const int accessPointPathRole = roleNames.key(QByteArrayLiteral("accessPointPath"), -1);
    QVERIFY(accessPointPathRole >= 0);
    bool foundExactPath = false;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        if (model.data(index, WifiNetworkModel::SsidRole).toString() == QStringLiteral("WPS")) {
            QCOMPARE(model.data(index, accessPointPathRole).toString(), QStringLiteral("/ap/wps"));
            foundExactPath = true;
        }
    }
    QVERIFY(foundExactPath);
}

void SystemServicesTest::networkSnapshotProjectsPrimaryRuntimeDetails()
{
    NetworkManagerState state;
    state.upsertDevice(QStringLiteral("/device/ethernet0"),
                       {{QStringLiteral("DeviceType"), 1},
                        {QStringLiteral("Interface"), QStringLiteral("enp4s0")},
                        {QStringLiteral("HwAddress"), QStringLiteral("AA:BB:CC:DD:EE:FF")},
                        {QStringLiteral("Mtu"), 1500},
                        {QStringLiteral("InterfaceFlags"), 0x00010002u}});
    state.setPrimaryProperties(QStringLiteral("/active/wired"),
                               {{QStringLiteral("Id"), QStringLiteral("Wired connection 1")},
                                {QStringLiteral("State"), 2},
                                {QStringLiteral("Devices"), QVariantList{
                                     QVariant::fromValue(QDBusObjectPath(
                                         QStringLiteral("/device/ethernet0"))) }},
                                {QStringLiteral("Ip4Config"), QVariant::fromValue(
                                     QDBusObjectPath(QStringLiteral("/ip4/one")))},
                                {QStringLiteral("Ip6Config"), QVariant::fromValue(
                                     QDBusObjectPath(QStringLiteral("/ip6/one")))}});
    const quint64 epoch = state.primaryEpoch();
    QVERIFY(state.applyWiredPropertiesReply(
        QStringLiteral("/active/wired"), epoch, QStringLiteral("/device/ethernet0"),
        {{QStringLiteral("Speed"), 1000}}));
    QVERIFY(state.applyPrimaryIpConfigReply(
        QStringLiteral("/active/wired"), epoch, QStringLiteral("/ip4/one"), true,
        {{QStringLiteral("AddressData"), QVariantList{
             QVariantMap{{QStringLiteral("address"), QStringLiteral("192.168.1.50")},
                         {QStringLiteral("prefix"), 24}},
             QVariantMap{{QStringLiteral("address"), QStringLiteral("192.168.1.50")},
                         {QStringLiteral("prefix"), 24}},
             QVariantMap{{QStringLiteral("address"), QStringLiteral("not-an-address")},
                         {QStringLiteral("prefix"), 24}},
             QVariantMap{{QStringLiteral("address"), QStringLiteral("192.168.1.51")},
                         {QStringLiteral("prefix"), 99}}}},
         {QStringLiteral("Gateway"), QStringLiteral("192.168.1.1")}}));
    QVERIFY(state.applyPrimaryIpConfigReply(
        QStringLiteral("/active/wired"), epoch, QStringLiteral("/ip6/one"), false,
        {{QStringLiteral("AddressData"), QVariantList{
             QVariantMap{{QStringLiteral("address"), QStringLiteral("2804:abcd::1234")},
                         {QStringLiteral("prefix"), 64}},
             QVariantMap{{QStringLiteral("address"), QStringLiteral("fe80::1")},
                         {QStringLiteral("prefix"), 64}}}},
         {QStringLiteral("Gateway"), QStringLiteral("fe80::1")}}));

    const NetworkSnapshot snapshot = state.snapshot();
    QVERIFY(snapshot.connected);
    QCOMPARE(snapshot.connectionType, NetworkConnectionType::Wired);
    QCOMPARE(snapshot.hardwareAddress, QStringLiteral("AA:BB:CC:DD:EE:FF"));
    QCOMPARE(snapshot.mtu, quint32(1500));
    QVERIFY(snapshot.linkCarrier);
    QCOMPARE(snapshot.linkSpeedMbps, quint32(1000));
    QCOMPARE(snapshot.ipv4Addresses, QStringList{QStringLiteral("192.168.1.50/24")});
    QCOMPARE(snapshot.ipv4Gateway, QStringLiteral("192.168.1.1"));
    QCOMPARE(snapshot.ipv6Addresses,
             (QStringList{QStringLiteral("2804:abcd::1234/64"), QStringLiteral("fe80::1/64")}));
    QCOMPARE(snapshot.ipv6Gateway, QStringLiteral("fe80::1"));

    state.updateDeviceProperties(QStringLiteral("/device/ethernet0"),
                                 QStringLiteral("org.freedesktop.NetworkManager.Device.Wired"),
                                 {{QStringLiteral("Speed"), 2500}}, {});
    QVERIFY(state.updatePrimaryIpConfigProperties(
        QStringLiteral("/ip4/one"), QStringLiteral("org.freedesktop.NetworkManager.IP4Config"),
        {{QStringLiteral("Gateway"), QStringLiteral("192.168.1.254")}}, {}));
    QVERIFY(state.updatePrimaryIpConfigProperties(
        QStringLiteral("/ip4/one"), QStringLiteral("org.freedesktop.NetworkManager.IP4Config"),
        {{QStringLiteral("AddressData"), QVariantList{
             QVariantMap{{QStringLiteral("address"), QStringLiteral("192.168.1.52")},
                         {QStringLiteral("prefix"), 24}}}}}, {}));
    const NetworkSnapshot updated = state.snapshot();
    QCOMPARE(updated.linkSpeedMbps, quint32(2500));
    QCOMPARE(updated.ipv4Addresses, QStringList{QStringLiteral("192.168.1.52/24")});
    QCOMPARE(updated.ipv4Gateway, QStringLiteral("192.168.1.254"));
    QVERIFY(state.updatePrimaryIpConfigProperties(
        QStringLiteral("/ip4/one"), QStringLiteral("org.freedesktop.NetworkManager.IP4Config"),
        {}, {QStringLiteral("AddressData"), QStringLiteral("Gateway")}));
    QVERIFY(state.snapshot().ipv4Addresses.isEmpty());
    QVERIFY(state.snapshot().ipv4Gateway.isEmpty());
    QVERIFY(state.updatePrimaryIpConfigProperties(
        QStringLiteral("/ip4/one"), QStringLiteral("org.freedesktop.NetworkManager.IP4Config"),
        {{QStringLiteral("AddressData"), QStringLiteral("malformed")}}, {}));
    QVERIFY(state.snapshot().ipv4Addresses.isEmpty());

    NetworkManagerState empty;
    empty.upsertDevice(QStringLiteral("/device/ethernet0"),
                       {{QStringLiteral("DeviceType"), 1},
                        {QStringLiteral("Interface"), QStringLiteral("enp4s0")},
                        {QStringLiteral("InterfaceFlags"), 0u}});
    empty.setPrimaryProperties(QStringLiteral("/active/wired"),
                               {{QStringLiteral("State"), 2},
                                {QStringLiteral("Devices"), QVariantList{
                                     QVariant::fromValue(QDBusObjectPath(
                                         QStringLiteral("/device/ethernet0"))) }},
                                {QStringLiteral("Ip4Config"), QVariant::fromValue(
                                     QDBusObjectPath(QStringLiteral("/ip4/empty")))}});
    QVERIFY(empty.applyPrimaryIpConfigReply(
        QStringLiteral("/active/wired"), empty.primaryEpoch(), QStringLiteral("/ip4/empty"),
        true, {{QStringLiteral("AddressData"), QVariantList{}}}));
    const NetworkSnapshot emptySnapshot = empty.snapshot();
    QVERIFY(!emptySnapshot.linkCarrier);
    QVERIFY(emptySnapshot.ipv4Addresses.isEmpty());
    QVERIFY(emptySnapshot.ipv4Gateway.isEmpty());

    NetworkManagerState other;
    other.upsertDevice(QStringLiteral("/device/virtual"),
                       {{QStringLiteral("DeviceType"), 42},
                        {QStringLiteral("Interface"), QStringLiteral("tun-test")},
                        {QStringLiteral("HwAddress"), QStringLiteral("02:00:00:00:00:01")},
                        {QStringLiteral("Mtu"), 1400}});
    other.setPrimaryProperties(QStringLiteral("/active/other"),
                               {{QStringLiteral("State"), 2},
                                {QStringLiteral("Devices"), QVariantList{
                                     QVariant::fromValue(QDBusObjectPath(
                                         QStringLiteral("/device/virtual"))) }},
                                {QStringLiteral("Ip4Config"), QVariant::fromValue(
                                     QDBusObjectPath(QStringLiteral("/ip4/other")))}});
    QVERIFY(other.applyPrimaryIpConfigReply(
        QStringLiteral("/active/other"), other.primaryEpoch(), QStringLiteral("/ip4/other"),
        true, {{QStringLiteral("Gateway"), QStringLiteral("10.0.0.1")}}));
    const NetworkSnapshot otherSnapshot = other.snapshot();
    QVERIFY(otherSnapshot.connected);
    QCOMPARE(otherSnapshot.connectionType, NetworkConnectionType::Other);
    QCOMPARE(otherSnapshot.interfaceName, QStringLiteral("tun-test"));
    QCOMPARE(otherSnapshot.hardwareAddress, QStringLiteral("02:00:00:00:00:01"));
    QCOMPARE(otherSnapshot.mtu, quint32(1400));
    QCOMPARE(otherSnapshot.linkSpeedMbps, quint32(0));
    QCOMPARE(otherSnapshot.ipv4Gateway, QStringLiteral("10.0.0.1"));
}

void SystemServicesTest::networkProfileAndDnsProjectionAreIdentityQualified()
{
    NetworkManagerState state;
    state.upsertDevice(QStringLiteral("/device/ethernet0"),
                       {{QStringLiteral("DeviceType"), 1},
                        {QStringLiteral("Interface"), QStringLiteral("enp1s0")}});
    const auto activeProperties = [](const QString &profilePath, const QString &ip4Path,
                                     const QString &ip6Path) {
        return QVariantMap{
            {QStringLiteral("State"), 2},
            {QStringLiteral("Connection"), QVariant::fromValue(QDBusObjectPath(profilePath))},
            {QStringLiteral("Devices"), QVariantList{QVariant::fromValue(
                 QDBusObjectPath(QStringLiteral("/device/ethernet0"))) }},
            {QStringLiteral("Ip4Config"), QVariant::fromValue(QDBusObjectPath(ip4Path))},
            {QStringLiteral("Ip6Config"), QVariant::fromValue(QDBusObjectPath(ip6Path))},
        };
    };
    state.setPrimaryProperties(QStringLiteral("/active/one"),
                               activeProperties(QStringLiteral("/settings/one"),
                                                QStringLiteral("/ip4/one"),
                                                QStringLiteral("/ip6/one")));
    const quint64 epoch = state.primaryEpoch();
    QVERIFY(state.applyPrimaryIpConfigReply(
        QStringLiteral("/active/one"), epoch, QStringLiteral("/ip4/one"), true,
        {{QStringLiteral("NameserverData"), QVariantList{
             QVariantMap{{QStringLiteral("address"), QStringLiteral("1.1.1.1")}},
             QVariantMap{{QStringLiteral("address"), QStringLiteral("not-an-address")}},
             QVariantMap{{QStringLiteral("address"), QStringLiteral("2606:4700::1")}}}},
         {QStringLiteral("DnsPriority"), -5}}));
    QVERIFY(state.applyPrimaryIpConfigReply(
        QStringLiteral("/active/one"), epoch, QStringLiteral("/ip6/one"), false,
        {{QStringLiteral("NameserverData"), QVariantList{
             QVariantMap{{QStringLiteral("address"), QStringLiteral("2606:4700:4700::1111")}},
             QVariantMap{{QStringLiteral("address"), QStringLiteral("1.0.0.1")}}}}}));

    NetworkConnectionSettings profile;
    profile.sections.insert(QStringLiteral("ipv4"),
        {{QStringLiteral("method"), QStringLiteral("auto")},
         {QStringLiteral("ignore-auto-dns"), false}});
    profile.sections.insert(QStringLiteral("ipv6"),
        {{QStringLiteral("method"), QStringLiteral("auto")},
         {QStringLiteral("ignore-auto-dns"), false}});
    QVERIFY(state.applyPrimaryProfileReply(QStringLiteral("/active/one"), epoch,
                                           QStringLiteral("/settings/one"), profile, 42));
    const NetworkSnapshot snapshot = state.snapshot();
    QCOMPARE(snapshot.activeConnectionPath, QStringLiteral("/active/one"));
    QCOMPARE(snapshot.profilePath, QStringLiteral("/settings/one"));
    QCOMPARE(snapshot.devicePath, QStringLiteral("/device/ethernet0"));
    QCOMPARE(snapshot.activeIpv4DnsServers, QStringList{QStringLiteral("1.1.1.1")});
    QCOMPARE(snapshot.activeIpv6DnsServers,
             QStringList{QStringLiteral("2606:4700:4700::1111")});
    QCOMPARE(snapshot.dnsMode, NetworkDnsMode::Automatic);
    QVERIFY(snapshot.dnsEditable);

    QVERIFY(state.updatePrimaryIpConfigProperties(
        QStringLiteral("/ip4/one"), QStringLiteral("org.freedesktop.NetworkManager.IP4Config"),
        {{QStringLiteral("NameserverData"), QVariantList{
             QVariantMap{{QStringLiteral("address"), QStringLiteral("9.9.9.9")}}}},
         {QStringLiteral("DnsPriority"), 10}}, {}));
    QCOMPARE(state.snapshot().activeIpv4DnsServers, QStringList{QStringLiteral("9.9.9.9")});
    QVERIFY(state.updatePrimaryIpConfigProperties(
        QStringLiteral("/ip4/one"), QStringLiteral("org.freedesktop.NetworkManager.IP4Config"),
        {}, {QStringLiteral("NameserverData"), QStringLiteral("DnsPriority")}));
    QVERIFY(state.snapshot().activeIpv4DnsServers.isEmpty());

    state.setPrimaryProperties(QStringLiteral("/active/two"),
                               activeProperties(QStringLiteral("/settings/two"),
                                                QStringLiteral("/ip4/two"),
                                                QStringLiteral("/ip6/two")));
    const quint64 nextEpoch = state.primaryEpoch();
    QVERIFY(!state.applyPrimaryProfileReply(QStringLiteral("/active/one"), epoch,
                                            QStringLiteral("/settings/one"), profile, 42));
    QVERIFY(!state.updatePrimaryIpConfigProperties(
        QStringLiteral("/ip4/one"), QStringLiteral("org.freedesktop.NetworkManager.IP4Config"),
        {{QStringLiteral("NameserverData"), QVariantList{
             QVariantMap{{QStringLiteral("address"), QStringLiteral("8.8.8.8")}}}}}, {}));
    const NetworkSnapshot switched = state.snapshot();
    QCOMPARE(switched.activeConnectionPath, QStringLiteral("/active/two"));
    QCOMPARE(switched.profilePath, QStringLiteral("/settings/two"));
    QVERIFY(switched.activeIpv4DnsServers.isEmpty());
    QVERIFY(switched.configuredIpv4DnsServers.isEmpty());
    QVERIFY(!switched.dnsEditable);
    QVERIFY(!state.applyPrimaryProfileReply(QStringLiteral("/active/two"), nextEpoch,
                                            QStringLiteral("/settings/one"), profile, 43));
}

void SystemServicesTest::networkPrimaryRuntimeDetailsArePathQualifiedAndCleared()
{
    NetworkManagerState state;
    state.upsertDevice(QStringLiteral("/device/ethernet0"),
                       {{QStringLiteral("DeviceType"), 1},
                        {QStringLiteral("Interface"), QStringLiteral("enp4s0")},
                        {QStringLiteral("HwAddress"), QStringLiteral("AA:00:00:00:00:01")},
                        {QStringLiteral("Mtu"), 1500},
                        {QStringLiteral("InterfaceFlags"), 0x00010000u}});
    state.upsertDevice(QStringLiteral("/device/ethernet1"),
                       {{QStringLiteral("DeviceType"), 1},
                        {QStringLiteral("Interface"), QStringLiteral("enp5s0")},
                        {QStringLiteral("HwAddress"), QStringLiteral("AA:00:00:00:00:02")},
                        {QStringLiteral("Mtu"), 9000},
                        {QStringLiteral("InterfaceFlags"), 0u}});
    const auto activeProperties = [](const QString &devicePath, const QString &ip4Path,
                                     const QString &ip6Path) {
        return QVariantMap{
            {QStringLiteral("State"), 2},
            {QStringLiteral("Devices"), QVariantList{
                 QVariant::fromValue(QDBusObjectPath(devicePath))}},
            {QStringLiteral("Ip4Config"), QVariant::fromValue(QDBusObjectPath(ip4Path))},
            {QStringLiteral("Ip6Config"), QVariant::fromValue(QDBusObjectPath(ip6Path))}};
    };

    state.setPrimaryProperties(QStringLiteral("/active/A"), activeProperties(
        QStringLiteral("/device/ethernet0"), QStringLiteral("/ip4/1"),
        QStringLiteral("/ip6/1")));
    const quint64 epochA = state.primaryEpoch();
    QVERIFY(state.applyWiredPropertiesReply(
        QStringLiteral("/active/A"), epochA, QStringLiteral("/device/ethernet0"),
        {{QStringLiteral("Speed"), 1000}}));
    const QVariantMap ipv4Properties{{QStringLiteral("AddressData"), QVariantList{
        QVariantMap{{QStringLiteral("address"), QStringLiteral("10.0.0.10")},
                    {QStringLiteral("prefix"), 24}}}}};
    const QVariantMap ipv6Properties{{QStringLiteral("AddressData"), QVariantList{
        QVariantMap{{QStringLiteral("address"), QStringLiteral("2001:db8::1")},
                    {QStringLiteral("prefix"), 64}}}}};
    QVERIFY(state.applyPrimaryIpConfigReply(
        QStringLiteral("/active/A"), epochA, QStringLiteral("/ip4/1"), true,
        ipv4Properties));
    QVERIFY(state.applyPrimaryIpConfigReply(
        QStringLiteral("/active/A"), epochA, QStringLiteral("/ip6/1"), false,
        ipv6Properties));

    state.setPrimaryProperties(QStringLiteral("/active/B"), activeProperties(
        QStringLiteral("/device/ethernet1"), QStringLiteral("/ip4/2"),
        QStringLiteral("/ip6/2")));
    const quint64 epochB = state.primaryEpoch();
    QVERIFY(epochB > epochA);
    QVERIFY(!state.applyWiredPropertiesReply(
        QStringLiteral("/active/A"), epochA, QStringLiteral("/device/ethernet0"),
        {{QStringLiteral("Speed"), 10000}}));
    QVERIFY(!state.applyPrimaryIpConfigReply(
        QStringLiteral("/active/A"), epochA, QStringLiteral("/ip4/1"), true,
        {{QStringLiteral("Gateway"), QStringLiteral("10.0.0.1")}}));
    QVERIFY(!state.applyPrimaryIpConfigReply(
        QStringLiteral("/active/A"), epochA, QStringLiteral("/ip6/1"), false,
        {{QStringLiteral("Gateway"), QStringLiteral("2001:db8::ff")}}));
    NetworkSnapshot switched = state.snapshot();
    QVERIFY(switched.ipv4Addresses.isEmpty());
    QVERIFY(switched.ipv4Gateway.isEmpty());
    QVERIFY(switched.ipv6Addresses.isEmpty());
    QVERIFY(switched.ipv6Gateway.isEmpty());
    QCOMPARE(switched.hardwareAddress, QStringLiteral("AA:00:00:00:00:02"));
    QCOMPARE(switched.mtu, quint32(9000));
    QCOMPARE(switched.linkSpeedMbps, quint32(0));

    QVERIFY(state.applyWiredPropertiesReply(
        QStringLiteral("/active/B"), epochB, QStringLiteral("/device/ethernet1"),
        {{QStringLiteral("Speed"), 2500}}));
    QVERIFY(state.applyPrimaryIpConfigReply(
        QStringLiteral("/active/B"), epochB, QStringLiteral("/ip4/2"), true,
        {{QStringLiteral("Gateway"), QStringLiteral("192.168.1.1")}}));
    QVERIFY(state.applyPrimaryIpConfigReply(
        QStringLiteral("/active/B"), epochB, QStringLiteral("/ip6/2"), false,
        {{QStringLiteral("Gateway"), QStringLiteral("fe80::1")}}));
    QCOMPARE(state.snapshot().linkSpeedMbps, quint32(2500));
    QCOMPARE(state.snapshot().ipv4Gateway, QStringLiteral("192.168.1.1"));
    QCOMPARE(state.snapshot().ipv6Gateway, QStringLiteral("fe80::1"));

    state.updatePrimaryProperties(
        {{QStringLiteral("Ip4Config"), QVariant::fromValue(QDBusObjectPath(
              QStringLiteral("/ip4/3")))},
         {QStringLiteral("Ip6Config"), QVariant::fromValue(QDBusObjectPath(
              QStringLiteral("/ip6/3")))}}, {});
    QVERIFY(!state.updatePrimaryIpConfigProperties(
        QStringLiteral("/ip4/2"), QStringLiteral("org.freedesktop.NetworkManager.IP4Config"),
        {{QStringLiteral("Gateway"), QStringLiteral("stale signal")}}, {}));
    QVERIFY(!state.applyPrimaryIpConfigReply(
        QStringLiteral("/active/B"), epochB, QStringLiteral("/ip4/2"), true,
        {{QStringLiteral("Gateway"), QStringLiteral("stale v4")}}));
    QVERIFY(!state.applyPrimaryIpConfigReply(
        QStringLiteral("/active/B"), epochB, QStringLiteral("/ip6/2"), false,
        {{QStringLiteral("Gateway"), QStringLiteral("stale v6")}}));
    QVERIFY(state.applyPrimaryIpConfigReply(
        QStringLiteral("/active/B"), epochB, QStringLiteral("/ip4/3"), true,
        {{QStringLiteral("Gateway"), QStringLiteral("192.168.2.1")}}));
    QVERIFY(state.applyPrimaryIpConfigReply(
        QStringLiteral("/active/B"), epochB, QStringLiteral("/ip6/3"), false,
        {{QStringLiteral("Gateway"), QStringLiteral("fe80::2")}}));
    QCOMPARE(state.snapshot().ipv4Gateway, QStringLiteral("192.168.2.1"));
    QCOMPARE(state.snapshot().ipv6Gateway, QStringLiteral("fe80::2"));

    state.updatePrimaryProperties({{QStringLiteral("State"), 1}}, {});
    const NetworkSnapshot disconnected = state.snapshot();
    QVERIFY(!disconnected.connected);
    QVERIFY(disconnected.hardwareAddress.isEmpty());
    QCOMPARE(disconnected.mtu, quint32(0));
    QVERIFY(!disconnected.linkCarrier);
    QCOMPARE(disconnected.linkSpeedMbps, quint32(0));
    QVERIFY(disconnected.ipv4Addresses.isEmpty());
    QVERIFY(disconnected.ipv4Gateway.isEmpty());
    QVERIFY(disconnected.ipv6Addresses.isEmpty());
    QVERIFY(disconnected.ipv6Gateway.isEmpty());

    NetworkManagerState deviceSwitch;
    deviceSwitch.upsertDevice(QStringLiteral("/device/ethernet0"),
                              {{QStringLiteral("DeviceType"), 1}});
    deviceSwitch.upsertDevice(QStringLiteral("/device/ethernet1"),
                              {{QStringLiteral("DeviceType"), 1}});
    deviceSwitch.setPrimaryProperties(QStringLiteral("/active/device-switch"), activeProperties(
        QStringLiteral("/device/ethernet0"), QStringLiteral("/ip4/one"),
        QStringLiteral("/ip6/one")));
    const quint64 deviceSwitchEpoch = deviceSwitch.primaryEpoch();
    QVERIFY(deviceSwitch.applyWiredPropertiesReply(
        QStringLiteral("/active/device-switch"), deviceSwitchEpoch,
        QStringLiteral("/device/ethernet0"), {{QStringLiteral("Speed"), 1000}}));
    deviceSwitch.updatePrimaryProperties(
        {{QStringLiteral("Devices"), QVariantList{
             QVariant::fromValue(QDBusObjectPath(QStringLiteral("/device/ethernet1")))}}}, {});
    QVERIFY(!deviceSwitch.applyWiredPropertiesReply(
        QStringLiteral("/active/device-switch"), deviceSwitchEpoch,
        QStringLiteral("/device/ethernet0"), {{QStringLiteral("Speed"), 10000}}));
    QVERIFY(deviceSwitch.applyWiredPropertiesReply(
        QStringLiteral("/active/device-switch"), deviceSwitchEpoch,
        QStringLiteral("/device/ethernet1"), {{QStringLiteral("Speed"), 2500}}));
    QCOMPARE(deviceSwitch.snapshot().linkSpeedMbps, quint32(2500));
}

void SystemServicesTest::networkServiceProjectsAndClearsPrimaryRuntimeDetails()
{
    auto backend = std::make_unique<FakeNetworkBackend>();
    auto *backendPtr = backend.get();
    NetworkService service(std::move(backend));
    QVERIFY(service.start());
    NetworkSnapshot snapshot;
    snapshot.daemonAvailable = true;
    snapshot.connected = true;
    snapshot.connectionType = NetworkConnectionType::Wired;
    snapshot.hardwareAddress = QStringLiteral("AA:BB:CC:DD:EE:FF");
    snapshot.mtu = 1500;
    snapshot.linkCarrier = true;
    snapshot.linkSpeedMbps = 1000;
    snapshot.ipv4Addresses = {QStringLiteral("192.168.1.50/24")};
    snapshot.ipv4Gateway = QStringLiteral("192.168.1.1");
    snapshot.ipv6Addresses = {QStringLiteral("2001:db8::1/64")};
    snapshot.ipv6Gateway = QStringLiteral("fe80::1");
    backendPtr->publish(snapshot);
    QTRY_COMPARE_WITH_TIMEOUT(service.hardwareAddress(), snapshot.hardwareAddress, 500);
    QCOMPARE(service.mtu(), quint32(1500));
    QVERIFY(service.linkCarrier());
    QCOMPARE(service.linkSpeedMbps(), quint32(1000));
    QCOMPARE(service.ipv4Addresses(), snapshot.ipv4Addresses);
    QCOMPARE(service.ipv4Gateway(), snapshot.ipv4Gateway);
    QCOMPARE(service.ipv6Addresses(), snapshot.ipv6Addresses);
    QCOMPARE(service.ipv6Gateway(), snapshot.ipv6Gateway);

    snapshot.connected = false;
    snapshot.connectionType = NetworkConnectionType::None;
    snapshot.hardwareAddress.clear();
    snapshot.mtu = 0;
    snapshot.linkCarrier = false;
    snapshot.linkSpeedMbps = 0;
    snapshot.ipv4Addresses.clear();
    snapshot.ipv4Gateway.clear();
    snapshot.ipv6Addresses.clear();
    snapshot.ipv6Gateway.clear();
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(!service.connected(), 500);
    QVERIFY(service.hardwareAddress().isEmpty());
    QCOMPARE(service.mtu(), quint32(0));
    QVERIFY(!service.linkCarrier());
    QCOMPARE(service.linkSpeedMbps(), quint32(0));
    QVERIFY(service.ipv4Addresses().isEmpty());
    QVERIFY(service.ipv4Gateway().isEmpty());
    QVERIFY(service.ipv6Addresses().isEmpty());
    QVERIFY(service.ipv6Gateway().isEmpty());
}

void SystemServicesTest::networkServiceProjectsActiveAndConfiguredDns()
{
    auto backend = std::make_unique<FakeNetworkBackend>();
    auto *backendPtr = backend.get();
    NetworkService service(std::move(backend));
    QVERIFY(service.start());
    NetworkSnapshot snapshot;
    snapshot.daemonAvailable = true;
    snapshot.connected = true;
    snapshot.connectionType = NetworkConnectionType::Wired;
    snapshot.activeConnectionPath = QStringLiteral("/active/1");
    snapshot.profilePath = QStringLiteral("/settings/1");
    snapshot.devicePath = QStringLiteral("/device/ethernet0");
    snapshot.dnsMode = NetworkDnsMode::Custom;
    snapshot.dnsEditable = true;
    snapshot.configuredIpv4DnsServers = {QStringLiteral("1.1.1.1")};
    snapshot.configuredIpv6DnsServers = {QStringLiteral("2606:4700:4700::1111")};
    snapshot.activeIpv4DnsServers = {QStringLiteral("1.1.1.1")};
    snapshot.activeIpv6DnsServers = {QStringLiteral("2606:4700:4700::1001")};
    backendPtr->publish(snapshot);

    QTRY_COMPARE_WITH_TIMEOUT(service.dnsMode(), NetworkDnsMode::Custom, 500);
    QVERIFY(service.dnsEditable());
    QCOMPARE(service.configuredIpv4DnsServers(), snapshot.configuredIpv4DnsServers);
    QCOMPARE(service.configuredIpv6DnsServers(), snapshot.configuredIpv6DnsServers);
    QCOMPARE(service.activeIpv4DnsServers(), snapshot.activeIpv4DnsServers);
    QCOMPARE(service.activeIpv6DnsServers(), snapshot.activeIpv6DnsServers);

    snapshot.connected = false;
    snapshot.connectionType = NetworkConnectionType::None;
    snapshot.dnsEditable = false;
    snapshot.activeIpv4DnsServers.clear();
    snapshot.activeIpv6DnsServers.clear();
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(!service.connected(), 500);
    QVERIFY(!service.dnsEditable());
    QVERIFY(service.activeIpv4DnsServers().isEmpty());
    QVERIFY(service.activeIpv6DnsServers().isEmpty());
}

void SystemServicesTest::networkDnsOperationIsIndependentAndDoesNotDegradeHealth()
{
    auto backend = std::make_unique<FakeNetworkBackend>();
    auto *backendPtr = backend.get();
    NetworkService service(std::move(backend));
    QVERIFY(service.start());
    NetworkSnapshot snapshot;
    snapshot.daemonAvailable = true;
    snapshot.connected = true;
    snapshot.connectionType = NetworkConnectionType::Wired;
    snapshot.activeConnectionPath = QStringLiteral("/active/1");
    snapshot.profilePath = QStringLiteral("/settings/1");
    snapshot.devicePath = QStringLiteral("/device/ethernet0");
    snapshot.dnsMode = NetworkDnsMode::Automatic;
    snapshot.dnsEditable = true;
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(service.dnsEditable(), 500);

    QVERIFY(!service.applyDns(NetworkDnsMode::Custom, {QStringLiteral("resolver.example")}));
    QVERIFY(backendPtr->dnsRequests.isEmpty());
    QVERIFY(service.applyDns(NetworkDnsMode::Custom, {QStringLiteral("1.1.1.1")}));
    QCOMPARE(backendPtr->dnsRequests.size(), 1);
    QVERIFY(service.dnsPending());
    QCOMPARE(service.dnsMode(), NetworkDnsMode::Automatic);
    QCOMPARE(service.state(), SystemServiceState::Ready);

    const quint64 firstRequest = backendPtr->dnsRequests.constLast().requestId;
    backendPtr->finishDns(firstRequest, NetworkDnsOutcome::Failed,
                          QStringLiteral("Could not save DNS settings"));
    QTRY_VERIFY_WITH_TIMEOUT(!service.dnsPending(), 500);
    QCOMPARE(service.dnsErrorString(), QStringLiteral("Could not save DNS settings"));
    QCOMPARE(service.state(), SystemServiceState::Ready);
    QVERIFY(!service.dnsNeedsReconnect());

    QVERIFY(service.applyDns(NetworkDnsMode::Automatic, {}));
    const quint64 secondRequest = backendPtr->dnsRequests.constLast().requestId;
    backendPtr->finishDns(firstRequest, NetworkDnsOutcome::SavedNeedsReconnect);
    QCoreApplication::processEvents();
    QVERIFY(service.dnsPending());
    backendPtr->finishDns(secondRequest, NetworkDnsOutcome::SavedNeedsReconnect);
    QTRY_VERIFY_WITH_TIMEOUT(!service.dnsPending(), 500);
    QVERIFY(service.dnsNeedsReconnect());
    QCOMPARE(service.state(), SystemServiceState::Ready);
}

void SystemServicesTest::networkDnsTimeoutRejectsLateCompletion()
{
    auto backend = std::make_unique<FakeNetworkBackend>();
    auto *backendPtr = backend.get();
    NetworkService service(std::move(backend));
    QVERIFY(service.start());
    NetworkSnapshot snapshot;
    snapshot.daemonAvailable = true;
    snapshot.connected = true;
    snapshot.connectionType = NetworkConnectionType::Wired;
    snapshot.activeConnectionPath = QStringLiteral("/active/1");
    snapshot.profilePath = QStringLiteral("/settings/1");
    snapshot.devicePath = QStringLiteral("/device/ethernet0");
    snapshot.dnsEditable = true;
    backendPtr->publish(snapshot);
    QTRY_VERIFY_WITH_TIMEOUT(service.dnsEditable(), 500);
    QSignalSpy finished(&service, &NetworkService::dnsOperationFinished);
    QVERIFY(service.applyDns(NetworkDnsMode::Automatic, {}));
    const quint64 requestId = backendPtr->dnsRequests.constLast().requestId;

    QTRY_VERIFY_WITH_TIMEOUT(!service.dnsPending(), 7000);
    QCOMPARE(backendPtr->cancelledDnsRequests, QVector<quint64>{requestId});
    QCOMPARE(service.dnsErrorString(), QStringLiteral("DNS settings update timed out"));
    QCOMPARE(finished.size(), 1);
    backendPtr->finishDns(requestId, NetworkDnsOutcome::Applied);
    QCoreApplication::processEvents();
    QCOMPARE(finished.size(), 1);
    QCOMPARE(service.dnsErrorString(), QStringLiteral("DNS settings update timed out"));
    QCOMPARE(service.state(), SystemServiceState::Ready);
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
    const WifiNetwork duplicateA{QStringLiteral("Cafe"), 93, false, true, 5180,
                                 QStringLiteral("AA"), QStringLiteral("/ap/strong")};
    const WifiNetwork duplicateB{QStringLiteral("Cafe"), 38, true, false, 2412,
                                 QStringLiteral("BB"), QStringLiteral("/ap/active")};
    const WifiNetwork office{QStringLiteral("Office"), 60, false, false, 5180,
                             QStringLiteral("c")};
    model.replace({duplicateA, office, duplicateB});
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0, 0), WifiNetworkModel::SsidRole).toString(),
             QStringLiteral("Cafe"));
    QCOMPARE(model.data(model.index(0, 0), WifiNetworkModel::StrengthRole).toInt(), 38);
    QVERIFY(model.data(model.index(0, 0), WifiNetworkModel::ActiveRole).toBool());
    QVERIFY(!model.data(model.index(0, 0), WifiNetworkModel::SecuredRole).toBool());
    QCOMPARE(model.data(model.index(0, 0), WifiNetworkModel::FrequencyRole).toInt(), 2412);
    QCOMPARE(model.data(model.index(0, 0), WifiNetworkModel::BssidRole).toString(),
             QStringLiteral("BB"));
    QCOMPARE(model.data(model.index(0, 0), WifiNetworkModel::AccessPointPathRole).toString(),
             QStringLiteral("/ap/active"));
    const auto roles = model.roleNames();
    QCOMPARE(roles.value(WifiNetworkModel::SsidRole), QByteArray("ssid"));
    QCOMPARE(roles.value(WifiNetworkModel::StrengthRole), QByteArray("strength"));
    QCOMPARE(roles.value(WifiNetworkModel::ActiveRole), QByteArray("active"));
    QCOMPARE(roles.value(WifiNetworkModel::SecuredRole), QByteArray("secured"));
    QCOMPARE(roles.value(WifiNetworkModel::FrequencyRole), QByteArray("frequencyMHz"));
    QCOMPARE(roles.value(WifiNetworkModel::BssidRole), QByteArray("bssid"));
    QCOMPARE(roles.value(WifiNetworkModel::AccessPointPathRole), QByteArray("accessPointPath"));

    WifiNetwork noLongerActive = duplicateB;
    noLongerActive.active = false;
    model.replace({duplicateA, noLongerActive, office});
    QCOMPARE(model.data(model.index(0, 0), WifiNetworkModel::SsidRole).toString(),
             QStringLiteral("Cafe"));
    QCOMPARE(model.data(model.index(0, 0), WifiNetworkModel::StrengthRole).toInt(), 93);
    QVERIFY(!model.data(model.index(0, 0), WifiNetworkModel::ActiveRole).toBool());
    QVERIFY(model.data(model.index(0, 0), WifiNetworkModel::SecuredRole).toBool());
    QCOMPARE(model.data(model.index(0, 0), WifiNetworkModel::FrequencyRole).toInt(), 5180);
    QCOMPARE(model.data(model.index(0, 0), WifiNetworkModel::BssidRole).toString(),
             QStringLiteral("AA"));
    QCOMPARE(model.data(model.index(0, 0), WifiNetworkModel::AccessPointPathRole).toString(),
             QStringLiteral("/ap/strong"));
    QCOMPARE(resetSpy.count(), 2);
    model.replace({duplicateA, office, duplicateB});
    QCOMPARE(resetSpy.count(), 3);
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

void SystemServicesTest::bluetoothServiceCategoryIsCarriedAndNotified()
{
    using Astrea::System::BluetoothServiceCategory;
    using Astrea::System::bluetoothServiceCategoryFromBridgeCode;

    static_assert(static_cast<int>(BluetoothServiceCategory::Unknown) == 0);
    static_assert(static_cast<int>(BluetoothServiceCategory::Audio) == 1);
    static_assert(static_cast<int>(BluetoothServiceCategory::MediaControl) == 2);
    static_assert(static_cast<int>(BluetoothServiceCategory::Calls) == 3);
    static_assert(static_cast<int>(BluetoothServiceCategory::HumanInterface) == 4);
    static_assert(static_cast<int>(BluetoothServiceCategory::FileTransfer) == 5);
    static_assert(static_cast<int>(BluetoothServiceCategory::Contacts) == 6);
    static_assert(static_cast<int>(BluetoothServiceCategory::Network) == 7);
    static_assert(static_cast<int>(BluetoothServiceCategory::Battery) == 8);
    static_assert(static_cast<int>(BluetoothServiceCategory::Serial) == 9);

    QCOMPARE(bluetoothServiceCategoryFromBridgeCode(2), BluetoothServiceCategory::MediaControl);
    QCOMPARE(bluetoothServiceCategoryFromBridgeCode(99), BluetoothServiceCategory::Unknown);

    auto backend = std::make_unique<FakeBluetoothBackend>();
    auto *backendPtr = backend.get();
    BluetoothService service(std::move(backend));
    QVERIFY(service.start());
    QSignalSpy agentSpy(&service, &BluetoothService::agentRequestChanged);

    BluetoothSnapshot snapshot;
    snapshot.agentRequestActive = true;
    snapshot.agentRequestId = 7;
    snapshot.agentRequestKind = BluetoothAgentRequestKind::ServiceAuthorization;
    snapshot.agentServiceUuid = QStringLiteral("0000110e-0000-1000-8000-00805f9b34fb");
    snapshot.agentServiceCategory = BluetoothServiceCategory::MediaControl;
    backendPtr->publish(snapshot);

    QCOMPARE(service.agentServiceUuid(), snapshot.agentServiceUuid);
    QCOMPARE(service.agentServiceCategory(), BluetoothServiceCategory::MediaControl);
    QCOMPARE(agentSpy.count(), 1);

    snapshot.agentServiceCategory = BluetoothServiceCategory::Calls;
    backendPtr->publish(snapshot);
    QCOMPARE(service.agentServiceCategory(), BluetoothServiceCategory::Calls);
    QCOMPARE(agentSpy.count(), 2);
}

void SystemServicesTest::bluetoothDeviceKindIsCarriedAndNotified()
{
    using Astrea::System::BluetoothDeviceKind;
    using Astrea::System::bluetoothDeviceKindFromBridgeCode;

    static_assert(static_cast<int>(BluetoothDeviceKind::Unknown) == 0);
    static_assert(static_cast<int>(BluetoothDeviceKind::Audio) == 1);
    static_assert(static_cast<int>(BluetoothDeviceKind::Headset) == 2);
    static_assert(static_cast<int>(BluetoothDeviceKind::Headphones) == 3);
    static_assert(static_cast<int>(BluetoothDeviceKind::Speaker) == 4);
    static_assert(static_cast<int>(BluetoothDeviceKind::Keyboard) == 5);
    static_assert(static_cast<int>(BluetoothDeviceKind::Mouse) == 6);
    static_assert(static_cast<int>(BluetoothDeviceKind::Gamepad) == 7);
    static_assert(static_cast<int>(BluetoothDeviceKind::Phone) == 8);
    static_assert(static_cast<int>(BluetoothDeviceKind::Computer) == 9);
    static_assert(static_cast<int>(BluetoothDeviceKind::Tablet) == 10);
    static_assert(static_cast<int>(BluetoothDeviceKind::Printer) == 11);
    static_assert(static_cast<int>(BluetoothDeviceKind::Camera) == 12);
    static_assert(static_cast<int>(BluetoothDeviceKind::Network) == 13);

    for (const auto &[kind, code] : std::array{
             std::pair{BluetoothDeviceKind::Unknown, 0},
             std::pair{BluetoothDeviceKind::Audio, 1},
             std::pair{BluetoothDeviceKind::Headset, 2},
             std::pair{BluetoothDeviceKind::Headphones, 3},
             std::pair{BluetoothDeviceKind::Speaker, 4},
             std::pair{BluetoothDeviceKind::Keyboard, 5},
             std::pair{BluetoothDeviceKind::Mouse, 6},
             std::pair{BluetoothDeviceKind::Gamepad, 7},
             std::pair{BluetoothDeviceKind::Phone, 8},
             std::pair{BluetoothDeviceKind::Computer, 9},
             std::pair{BluetoothDeviceKind::Tablet, 10},
             std::pair{BluetoothDeviceKind::Printer, 11},
             std::pair{BluetoothDeviceKind::Camera, 12},
             std::pair{BluetoothDeviceKind::Network, 13},
         }) {
        QCOMPARE(bluetoothDeviceKindFromBridgeCode(code), kind);
    }
    QCOMPARE(bluetoothDeviceKindFromBridgeCode(99), BluetoothDeviceKind::Unknown);

    auto backend = std::make_unique<FakeBluetoothBackend>();
    auto *backendPtr = backend.get();
    BluetoothService service(std::move(backend));
    QVERIFY(service.start());
    QSignalSpy pairingSpy(&service, &BluetoothService::pairingChanged);
    QSignalSpy agentSpy(&service, &BluetoothService::agentRequestChanged);

    BluetoothSnapshot snapshot;
    snapshot.pairing = true;
    snapshot.pairingDevicePath = QStringLiteral("/org/bluez/hci0/dev_headset");
    snapshot.agentRequestActive = true;
    snapshot.agentRequestId = 1;
    snapshot.pairingDeviceKind = BluetoothDeviceKind::Headset;
    snapshot.agentDeviceKind = BluetoothDeviceKind::Keyboard;
    backendPtr->publish(snapshot);
    QCOMPARE(service.pairingDeviceKind(), BluetoothDeviceKind::Headset);
    QCOMPARE(service.agentDeviceKind(), BluetoothDeviceKind::Keyboard);
    QCOMPARE(pairingSpy.count(), 1);
    QCOMPARE(agentSpy.count(), 1);

    snapshot.pairingDeviceKind = BluetoothDeviceKind::Speaker;
    backendPtr->publish(snapshot);
    QCOMPARE(service.pairingDeviceKind(), BluetoothDeviceKind::Speaker);
    QCOMPARE(pairingSpy.count(), 2);

    snapshot.agentDeviceKind = BluetoothDeviceKind::Computer;
    backendPtr->publish(snapshot);
    QCOMPARE(service.agentDeviceKind(), BluetoothDeviceKind::Computer);
    QCOMPARE(agentSpy.count(), 2);
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
    availableB.deviceKind = BluetoothDeviceKind::Keyboard;
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
    QCOMPARE(model.data(model.index(2, 0), BluetoothDeviceModel::DeviceKindRole)
                 .value<BluetoothDeviceKind>(), BluetoothDeviceKind::Keyboard);
    const auto roles = model.roleNames();
    QCOMPARE(roles.value(BluetoothDeviceModel::IdRole), QByteArray("id"));
    QCOMPARE(roles.value(BluetoothDeviceModel::ObjectPathRole), QByteArray("objectPath"));
    QCOMPARE(roles.value(BluetoothDeviceModel::AddressRole), QByteArray("address"));
    QCOMPARE(roles.value(BluetoothDeviceModel::NameRole), QByteArray("name"));
    QCOMPARE(roles.value(BluetoothDeviceModel::DeviceKindRole), QByteArray("deviceKind"));
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

void SystemServicesTest::spatialAudioPipeWireBoundaryUsesCanonicalPlanarPortsAndSilencesErrors()
{
    using Bridge = SpatialAudioBlockBridge;
    QVERIFY((Bridge::InputChannelNames ==
             std::array<std::string_view, 8>{"FL", "FR", "FC", "LFE", "RL", "RR", "SL", "SR"}));
    QVERIFY((Bridge::OutputChannelNames == std::array<std::string_view, 2>{"FL", "FR"}));
    QVERIFY(Bridge::supportsClockRate(1, 48'000));
    QVERIFY(Bridge::supportsClockRate(2, 96'000));
    QVERIFY(!Bridge::supportsClockRate(1, 44'100));

    std::array<std::array<float, 3>, 8> inputStorage{};
    std::array<const float *, 8> inputs{};
    for (std::size_t channel = 0; channel < inputStorage.size(); ++channel) {
        for (std::size_t frame = 0; frame < inputStorage[channel].size(); ++frame)
            inputStorage[channel][frame] = static_cast<float>(channel * 10 + frame);
        inputs[channel] = inputStorage[channel].data();
    }
    std::array<std::array<float, 3>, 2> outputStorage{};
    std::array<float *, 2> outputs = {outputStorage[0].data(), outputStorage[1].data()};
    FakeSpatialProcessor fake;
    QVERIFY(Bridge::process(inputs, outputs, 3, &processSpatialFake, &fake));
    QCOMPARE(fake.calls, 1);
    QCOMPARE(outputStorage[0][1], 21.0f);
    QCOMPARE(outputStorage[1][2], 72.0f);

    fake.shouldFail = true;
    QVERIFY(!Bridge::process(inputs, outputs, 3, &processSpatialFake, &fake));
    QVERIFY((outputStorage[0] == std::array<float, 3>{0.0f, 0.0f, 0.0f}));
    QVERIFY((outputStorage[1] == std::array<float, 3>{0.0f, 0.0f, 0.0f}));

    fake.shouldFail = false;
    inputs[4] = nullptr;
    QVERIFY(!Bridge::process(inputs, outputs, 3, &processSpatialFake, &fake));
    QVERIFY((outputStorage[0] == std::array<float, 3>{0.0f, 0.0f, 0.0f}));
    QVERIFY((outputStorage[1] == std::array<float, 3>{0.0f, 0.0f, 0.0f}));
}

QTEST_GUILESS_MAIN(SystemServicesTest)
#include "SystemServicesTest.moc"
