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

    bool available = true;
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

QTEST_MAIN(TyphonProtocolClientTest)
#include "TyphonProtocolClientTest.moc"
