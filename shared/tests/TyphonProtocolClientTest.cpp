#include <QSignalSpy>
#include <QTest>

#include "platform/typhon/TyphonToplevelConnection.hpp"

using namespace Astrea::Typhon;

class FakeTyphonAdapter final : public TyphonProtocolAdapter {
    Q_OBJECT
public:
    explicit FakeTyphonAdapter(QObject *parent = nullptr) : TyphonProtocolAdapter(parent) {}

    void start() override { ++starts; }
    void stop() override { ++stops; }
    bool isAvailable() const override { return available; }
    TyphonActionCapabilityState actionCapability() const override { return capability; }
    std::optional<ToplevelActionError> requestAction(quint64 handleToken,
                                                     TyphonActionToken token,
                                                     ToplevelAction action) override
    {
        actionRequests.append({handleToken, token, action});
        return requestError;
    }

    void advertiseManager(bool present = true) { emit registryDiscovered(present); }
    void create(quint64 token) { emit handleCreated(token); }
    void id(quint64 token, const QString &value) { emit identifierChanged(token, value); }
    void app(quint64 token, const QString &value) { emit appIdChanged(token, value); }
    void title(quint64 token, const QString &value) { emit titleChanged(token, value); }
    void pid(quint64 token, quint32 value) { emit pidChanged(token, value); }
    void kind(quint64 token, ToplevelKind value) { emit kindChanged(token, value); }
    void state(quint64 token, ToplevelStates value, quint32 raw = 0) { emit stateChanged(token, value, raw); }
    void focus(quint64 token, FocusSerial value) { emit focusSerialChanged(token, value); }
    void handleDone(quint64 token, Revision value) { emit handleCompleted(token, value); }
    void close(quint64 token) { emit handleClosed(token); }
    void done(Revision revision, quint32 total, bool truncated = false)
    { emit managerCompleted(revision, total, truncated); }
    void fail(const QString &reason) { emit managerFailed(reason); }
    void disconnectDisplay() { emit displayDisconnected(); }
    void actionDone(TyphonActionToken token, ToplevelAction action, ToplevelActionResult result)
    { emit actionCompleted(token.hi, token.lo, action, result); }

    bool available = true;
    TyphonActionCapabilityState capability = TyphonActionCapabilityState::ActionReadyV2;
    std::optional<ToplevelActionError> requestError;
    struct ActionRequest {
        quint64 handleToken = 0;
        TyphonActionToken token;
        ToplevelAction action = ToplevelAction::Activate;
    };
    QVector<ActionRequest> actionRequests;
    int starts = 0;
    int stops = 0;
};

namespace {

void complete(FakeTyphonAdapter &adapter, quint64 token, const QString &id, Revision revision)
{
    adapter.create(token);
    adapter.id(token, id);
    adapter.app(token, QStringLiteral("org.example.App"));
    adapter.title(token, QStringLiteral("Example"));
    adapter.pid(token, 100);
    adapter.kind(token, ToplevelKind::XdgToplevel);
    adapter.state(token, ToplevelStates{});
    adapter.focus(token, 1);
    adapter.handleDone(token, revision);
}

} // namespace

class TyphonProtocolClientTest final : public QObject {
    Q_OBJECT

private slots:
    void registryAndInitialDoneReachReady();
    void noGlobalBecomesUnsupported();
    void revisionSpanningTurnsPublishesOnce();
    void disconnectClearsSnapshotAndIncrementsGeneration();
    void managerFailureDegradesAndClearsSnapshot();
    void stopStopsAdapterAndReconnectTimer();
    void stress100InitialConnectEnumerateStopCycles();
    void stress100DisconnectReconnectCycles();
    void stress100ManagerFailureReconnectCycles();
    void v1PublicationRemainsReadOnly();
    void v2ActionResultsAreManagerOwned();
    void disconnectSettlesPendingActionOnce();
    void staleWindowIdDoesNotRetarget();
};

void TyphonProtocolClientTest::registryAndInitialDoneReachReady()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    QSignalSpy snapshotSpy(&connection, &TyphonToplevelConnection::snapshotChanged);
    connection.start();
    QCOMPARE(connection.state(), TyphonConnectionState::WaitingForRegistry);
    adapter->advertiseManager();
    QCOMPARE(connection.state(), TyphonConnectionState::WaitingForInitialSnapshot);
    complete(*adapter, 1, QStringLiteral("1"), 1);
    QVERIFY(connection.state() != TyphonConnectionState::Ready);
    adapter->done(1, 1);
    QCOMPARE(connection.state(), TyphonConnectionState::Ready);
    QCOMPARE(snapshotSpy.count(), 1);
    QCOMPARE(connection.snapshot().windows.size(), 1);
}

void TyphonProtocolClientTest::noGlobalBecomesUnsupported()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    connection.start();
    adapter->advertiseManager(false);
    QCOMPARE(connection.state(), TyphonConnectionState::Unsupported);
}

void TyphonProtocolClientTest::revisionSpanningTurnsPublishesOnce()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    QSignalSpy snapshotSpy(&connection, &TyphonToplevelConnection::snapshotChanged);
    connection.start();
    adapter->advertiseManager();
    adapter->create(1);
    adapter->id(1, QStringLiteral("1"));
    adapter->app(1, QStringLiteral("org.example.App"));
    adapter->title(1, QStringLiteral("split"));
    adapter->pid(1, 1);
    adapter->kind(1, ToplevelKind::XdgToplevel);
    adapter->state(1, {});
    adapter->focus(1, 1);
    QVERIFY(snapshotSpy.isEmpty());
    adapter->handleDone(1, 1);
    QVERIFY(snapshotSpy.isEmpty());
    adapter->done(1, 1);
    QCOMPARE(snapshotSpy.count(), 1);
}

void TyphonProtocolClientTest::disconnectClearsSnapshotAndIncrementsGeneration()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    connection.start();
    const quint64 firstGeneration = connection.connectionGeneration();
    adapter->advertiseManager();
    complete(*adapter, 1, QStringLiteral("1"), 1);
    adapter->done(1, 1);
    QVERIFY(connection.hasSnapshot());
    adapter->disconnectDisplay();
    QVERIFY(!connection.hasSnapshot());
    QCOMPARE(connection.state(), TyphonConnectionState::Disconnected);
    connection.stop();
    connection.start();
    QVERIFY(connection.connectionGeneration() > firstGeneration);
}

void TyphonProtocolClientTest::managerFailureDegradesAndClearsSnapshot()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    connection.start();
    adapter->advertiseManager();
    complete(*adapter, 1, QStringLiteral("1"), 1);
    adapter->done(1, 1);
    QVERIFY(connection.hasSnapshot());
    adapter->fail(QStringLiteral("terminal"));
    QVERIFY(!connection.hasSnapshot());
    QCOMPARE(connection.state(), TyphonConnectionState::Degraded);
}

void TyphonProtocolClientTest::stopStopsAdapterAndReconnectTimer()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    connection.start();
    connection.stop();
    QCOMPARE(adapter->starts, 1);
    QCOMPARE(adapter->stops, 1);
    QCOMPARE(connection.state(), TyphonConnectionState::Stopped);
    QVERIFY(!connection.reconnectPending());
}

void TyphonProtocolClientTest::stress100InitialConnectEnumerateStopCycles()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    for (quint64 cycle = 1; cycle <= 100; ++cycle) {
        connection.start();
        adapter->advertiseManager();
        complete(*adapter, cycle, QString::number(cycle), 1);
        adapter->done(1, 1);
        QCOMPARE(connection.state(), TyphonConnectionState::Ready);
        QVERIFY(connection.hasSnapshot());
        connection.stop();
        QCOMPARE(connection.state(), TyphonConnectionState::Stopped);
        QVERIFY(!connection.reconnectPending());
    }
    QCOMPARE(adapter->starts, 100);
}

void TyphonProtocolClientTest::stress100DisconnectReconnectCycles()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    for (int cycle = 0; cycle < 100; ++cycle) {
        connection.start();
        adapter->advertiseManager();
        complete(*adapter, static_cast<quint64>(cycle + 1), QString::number(cycle + 1), 1);
        adapter->done(1, 1);
        adapter->disconnectDisplay();
        QCOMPARE(connection.state(), TyphonConnectionState::Disconnected);
        QVERIFY(!connection.hasSnapshot());
        connection.stop();
    }
    QVERIFY(adapter->starts >= 100);
    QVERIFY(adapter->stops >= 100);
}

void TyphonProtocolClientTest::stress100ManagerFailureReconnectCycles()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    for (int cycle = 0; cycle < 100; ++cycle) {
        connection.start();
        adapter->advertiseManager();
        complete(*adapter, static_cast<quint64>(cycle + 1), QString::number(cycle + 1), 1);
        adapter->done(1, 1);
        adapter->fail(QStringLiteral("stress"));
        QCOMPARE(connection.state(), TyphonConnectionState::Degraded);
        QVERIFY(!connection.hasSnapshot());
        connection.stop();
    }
    QVERIFY(adapter->starts >= 100);
    QVERIFY(adapter->stops >= 100);
}

void TyphonProtocolClientTest::v1PublicationRemainsReadOnly()
{
    auto *adapter = new FakeTyphonAdapter;
    adapter->capability = TyphonActionCapabilityState::ReadOnlyV1;
    TyphonToplevelConnection connection(adapter);
    connection.start();
    adapter->advertiseManager();
    complete(*adapter, 1, QStringLiteral("1"), 1);
    adapter->done(1, 1);
    QCOMPARE(connection.state(), TyphonConnectionState::Ready);

    const auto error = connection.requestAction(QStringLiteral("1"), ToplevelAction::Activate, 10);
    QVERIFY(error.has_value());
    QCOMPARE(error.value(), ToplevelActionError::UnsupportedProtocol);
    QCOMPARE(adapter->actionRequests.size(), 0);
}

void TyphonProtocolClientTest::v2ActionResultsAreManagerOwned()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    QSignalSpy completed(&connection, &TyphonToplevelConnection::actionFinished);
    connection.start();
    adapter->advertiseManager();
    complete(*adapter, 1, QStringLiteral("1"), 1);
    adapter->done(1, 1);

    for (const auto action : {ToplevelAction::Activate, ToplevelAction::Minimize,
                              ToplevelAction::Restore, ToplevelAction::Close}) {
        const auto error = connection.requestAction(QStringLiteral("1"), action, 20);
        QVERIFY(!error.has_value());
        QCOMPARE(adapter->actionRequests.size(), 1);
        const auto request = adapter->actionRequests.takeLast();
        adapter->actionDone(request.token, action, ToplevelActionResult::Accepted);
        QCOMPARE(completed.count(), 1);
        QCOMPARE(completed.last().at(0).value<quint64>(), quint64(20));
        QCOMPARE(completed.last().at(1).value<ToplevelAction>(), action);
        QCOMPARE(completed.last().at(2).value<ToplevelActionResult>(),
                 ToplevelActionResult::Accepted);
        completed.clear();
    }

    const auto noChangeError = connection.requestAction(QStringLiteral("1"),
                                                        ToplevelAction::Activate, 21);
    QVERIFY(!noChangeError.has_value());
    const auto noChangeRequest = adapter->actionRequests.takeLast();
    adapter->actionDone(noChangeRequest.token, ToplevelAction::Activate,
                        ToplevelActionResult::NoChange);
    QCOMPARE(completed.count(), 1);
    QCOMPARE(completed.last().at(2).value<ToplevelActionResult>(),
             ToplevelActionResult::NoChange);
    completed.clear();

    const auto unavailableError = connection.requestAction(QStringLiteral("1"),
                                                           ToplevelAction::Activate, 22);
    QVERIFY(!unavailableError.has_value());
    const auto unavailableRequest = adapter->actionRequests.takeLast();
    adapter->actionDone(unavailableRequest.token, ToplevelAction::Activate,
                        ToplevelActionResult::Unavailable);
    QCOMPARE(completed.count(), 1);
    QCOMPARE(completed.last().at(2).value<ToplevelActionResult>(),
             ToplevelActionResult::Unavailable);
}

void TyphonProtocolClientTest::disconnectSettlesPendingActionOnce()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    QSignalSpy failed(&connection, &TyphonToplevelConnection::actionFailed);
    connection.start();
    adapter->advertiseManager();
    complete(*adapter, 1, QStringLiteral("1"), 1);
    adapter->done(1, 1);
    QVERIFY(!connection.requestAction(QStringLiteral("1"), ToplevelAction::Activate, 30).has_value());
    adapter->disconnectDisplay();
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).value<quint64>(), quint64(30));
    QCOMPARE(failed.at(0).at(2).value<ToplevelActionError>(), ToplevelActionError::Disconnected);
    adapter->actionDone({1, 1}, ToplevelAction::Activate, ToplevelActionResult::Accepted);
    QCOMPARE(failed.count(), 1);
}

void TyphonProtocolClientTest::staleWindowIdDoesNotRetarget()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    connection.start();
    adapter->advertiseManager();
    complete(*adapter, 1, QStringLiteral("1"), 1);
    adapter->done(1, 1);
    adapter->close(1);
    adapter->done(2, 0);

    const auto error = connection.requestAction(QStringLiteral("1"), ToplevelAction::Activate, 40);
    QVERIFY(error.has_value());
    QCOMPARE(error.value(), ToplevelActionError::ToplevelNotLive);
    QCOMPARE(adapter->actionRequests.size(), 0);
}

QTEST_MAIN(TyphonProtocolClientTest)
#include "TyphonProtocolClientTest.moc"
