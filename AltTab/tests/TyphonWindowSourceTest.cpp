#include <QSignalSpy>
#include <QTest>

#include "platform/typhon/TyphonWindowSource.hpp"

using namespace Astrea::Typhon;

class FakeTyphonAdapter final : public TyphonProtocolAdapter {
    Q_OBJECT

public:
    explicit FakeTyphonAdapter(QObject *parent = nullptr)
        : TyphonProtocolAdapter(parent)
    {
    }

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
    void state(quint64 token, ToplevelStates value) { emit stateChanged(token, value, 0); }
    void focus(quint64 token, FocusSerial value) { emit focusSerialChanged(token, value); }
    void handleDone(quint64 token, Revision value) { emit handleCompleted(token, value); }
    void done(Revision revision, quint32 total) { emit managerCompleted(revision, total, false); }

    bool available = true;
    int starts = 0;
    int stops = 0;
};

static void completeInitialSnapshot(FakeTyphonAdapter &adapter, quint64 token,
                                    const QString &id, Revision revision)
{
    adapter.create(token);
    adapter.id(token, id);
    adapter.app(token, QStringLiteral("org.example.App"));
    adapter.title(token, QStringLiteral("Example"));
    adapter.pid(token, 100);
    adapter.kind(token, ToplevelKind::XdgToplevel);
    adapter.state(token, {});
    adapter.focus(token, 1);
    adapter.handleDone(token, revision);
    adapter.done(revision, 1);
}

static void updateCommittedSnapshot(FakeTyphonAdapter &adapter, quint64 token,
                                    Revision revision, const QString &title)
{
    adapter.app(token, QStringLiteral("org.example.App"));
    adapter.title(token, title);
    adapter.pid(token, 100);
    adapter.kind(token, ToplevelKind::XdgToplevel);
    adapter.state(token, {});
    adapter.focus(token, revision);
    adapter.handleDone(token, revision);
    adapter.done(revision, 1);
}

class TyphonWindowSourceTest final : public QObject {
    Q_OBJECT

private slots:
    void descriptorAndCapabilities();
    void snapshotMappingPreservesTyphonIdentity();
    void activeAndMinimizedFlagsMap();
    void unsupportedActivationIsDeterministic();
    void pendingRequestWaitsForInitialSnapshot();
    void pendingRequestQueueOverflowIsBounded();
    void duplicatePendingTokenGetsOneResponse();
    void pendingRequestFailsWhenStopped();
    void initialSnapshotPublishesChangedBeforeReady();
    void completedTokenCanBeReused();
    void generationFailureAllowsSameTokenReuse();
    void stress100SnapshotMappingCycles();
    void stress100RequestBeforeInitialSnapshotCycles();
    void stress100SameTokenRequestCycles();
    void stress100GenerationFailureSameTokenReuse();
    void stress100MinimizedWindowCycles();
};

void TyphonWindowSourceTest::descriptorAndCapabilities()
{
    TyphonWindowSource source;
    QCOMPARE(source.descriptor().name, QStringLiteral("typhon"));
    QCOMPARE(source.descriptor().protocolVersion, 1);
    const auto capabilities = source.capabilities();
    QVERIFY(capabilities.contains(BackendCapability::WindowList));
    QVERIFY(capabilities.contains(BackendCapability::EventStream));
    QVERIFY(capabilities.contains(BackendCapability::ActiveWindow));
    QVERIFY(!capabilities.contains(BackendCapability::WindowActivation));
    QVERIFY(!capabilities.contains(BackendCapability::ActiveOutput));
}

void TyphonWindowSourceTest::snapshotMappingPreservesTyphonIdentity()
{
    TyphonWindowSource source;
    const Toplevel window{QStringLiteral("18446744073709551615"), QStringLiteral("org.example.App"),
                          QStringLiteral("Example"), 42, ToplevelKind::XdgToplevel, {}, 0, 0};
    const Snapshot snapshot{{window}, 3, 1, false, 8};
    const auto mapped = source.mapSnapshotForTest(snapshot);
    QCOMPARE(mapped.windows.size(), 1);
    QCOMPARE(mapped.windows.first().windowId.value, window.id);
    QCOMPARE(mapped.windows.first().pid, qint64(42));
    QCOMPARE(mapped.windows.first().className, window.appId);
    QCOMPARE(mapped.windows.first().initialClass, QString());
    QCOMPARE(mapped.windows.first().title, window.title);
    QCOMPARE(mapped.windows.first().initialTitle, window.title);
    QCOMPARE(mapped.windows.first().backendGeneration, quint64(8));
    QCOMPARE(mapped.windows.first().focusHistoryId, 1000000);
}

void TyphonWindowSourceTest::activeAndMinimizedFlagsMap()
{
    TyphonWindowSource source;
    Toplevel window;
    window.id = QStringLiteral("1");
    window.appId = QStringLiteral("app");
    window.title = QStringLiteral("title");
    window.states = ToplevelStates(ToplevelStateFlag::Active) | ToplevelStateFlag::Minimized;
    const auto mapped = source.mapSnapshotForTest(Snapshot{{window}, 1, 1, false, 1});
    QVERIFY(mapped.windows.first().isActive);
    QVERIFY(mapped.windows.first().isMinimized);
    QVERIFY(!mapped.windows.first().isHidden);
    QCOMPARE(mapped.activeWindowId.value, QStringLiteral("1"));
}

void TyphonWindowSourceTest::unsupportedActivationIsDeterministic()
{
    TyphonWindowSource source;
    QSignalSpy activationSpy(&source, &CompositorBackend::activationFinished);
    source.activateWindow({WindowId{QStringLiteral("1")}, 9});
    QCOMPARE(activationSpy.count(), 1);
    const ActivationResult result = activationSpy.at(0).at(1).value<ActivationResult>();
    QVERIFY(!result.success);
    QCOMPARE(result.error, QStringLiteral("Typhon window activation is unsupported"));
}

void TyphonWindowSourceTest::pendingRequestWaitsForInitialSnapshot()
{
    auto *adapter = new FakeTyphonAdapter;
    auto *connection = new TyphonToplevelConnection(adapter);
    TyphonWindowSource source(connection);
    QSignalSpy readySpy(&source, &CompositorBackend::snapshotReady);

    source.start();
    adapter->advertiseManager();
    source.requestSnapshot(42);
    QCOMPARE(readySpy.count(), 0);

    completeInitialSnapshot(*adapter, 1, QStringLiteral("42"), 1);

    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).value<RequestToken>(), RequestToken(42));
    const WindowSnapshot result = readySpy.at(0).at(1).value<WindowSnapshot>();
    const auto cached = source.cachedSnapshot();
    QVERIFY(cached.has_value());
    QCOMPARE(result.revision, cached->revision);
    QCOMPARE(result.windows.size(), cached->windows.size());
    QCOMPARE(result.windows.first().windowId.value, cached->windows.first().windowId.value);
    source.stop();
}

void TyphonWindowSourceTest::pendingRequestQueueOverflowIsBounded()
{
    auto *adapter = new FakeTyphonAdapter;
    auto *connection = new TyphonToplevelConnection(adapter);
    TyphonWindowSource source(connection);
    QSignalSpy readySpy(&source, &CompositorBackend::snapshotReady);
    QSignalSpy errorSpy(&source, &CompositorBackend::backendError);

    source.start();
    adapter->advertiseManager();
    for (RequestToken token = 1; token <= 17; ++token)
        source.requestSnapshot(token);

    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).value<RequestToken>(), RequestToken(17));
    QVERIFY(readySpy.at(0).at(1).value<WindowSnapshot>().windows.isEmpty());
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.at(0).at(0).toInt(), static_cast<int>(BackendError::ConnectionFailed));

    completeInitialSnapshot(*adapter, 1, QStringLiteral("1"), 1);
    QCOMPARE(readySpy.count(), 17);
    for (int i = 1; i <= 16; ++i)
        QCOMPARE(readySpy.at(i).at(0).value<RequestToken>(), RequestToken(i));

    source.requestSnapshot(17);
    QCOMPARE(readySpy.count(), 18);
    QCOMPARE(readySpy.at(17).at(0).value<RequestToken>(), RequestToken(17));
    QCOMPARE(readySpy.at(17).at(1).value<WindowSnapshot>().revision, Revision(1));
    source.stop();
}

void TyphonWindowSourceTest::duplicatePendingTokenGetsOneResponse()
{
    auto *adapter = new FakeTyphonAdapter;
    auto *connection = new TyphonToplevelConnection(adapter);
    TyphonWindowSource source(connection);
    QSignalSpy readySpy(&source, &CompositorBackend::snapshotReady);

    source.start();
    adapter->advertiseManager();
    source.requestSnapshot(42);
    source.requestSnapshot(42);
    completeInitialSnapshot(*adapter, 1, QStringLiteral("1"), 1);

    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).value<RequestToken>(), RequestToken(42));

    source.requestSnapshot(42);
    QCOMPARE(readySpy.count(), 2);
    QCOMPARE(readySpy.at(1).at(0).value<RequestToken>(), RequestToken(42));
    QCOMPARE(readySpy.at(1).at(1).value<WindowSnapshot>().revision, Revision(1));
    source.stop();
}

void TyphonWindowSourceTest::pendingRequestFailsWhenStopped()
{
    auto *adapter = new FakeTyphonAdapter;
    auto *connection = new TyphonToplevelConnection(adapter);
    TyphonWindowSource source(connection);
    QSignalSpy readySpy(&source, &CompositorBackend::snapshotReady);
    QSignalSpy errorSpy(&source, &CompositorBackend::backendError);

    source.start();
    adapter->advertiseManager();
    source.requestSnapshot(42);
    source.stop();

    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).value<RequestToken>(), RequestToken(42));
    QVERIFY(readySpy.at(0).at(1).value<WindowSnapshot>().windows.isEmpty());
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorSpy.at(0).at(0).toInt(), static_cast<int>(BackendError::ConnectionFailed));
}

void TyphonWindowSourceTest::initialSnapshotPublishesChangedBeforeReady()
{
    auto *adapter = new FakeTyphonAdapter;
    auto *connection = new TyphonToplevelConnection(adapter);
    TyphonWindowSource source(connection);
    QStringList eventOrder;
    connect(&source, &CompositorBackend::snapshotChanged, &source,
            [&eventOrder](const WindowSnapshot &) { eventOrder.append(QStringLiteral("changed")); });
    connect(&source, &CompositorBackend::snapshotReady, &source,
            [&eventOrder](RequestToken, const WindowSnapshot &) {
                eventOrder.append(QStringLiteral("ready"));
            });

    source.start();
    adapter->advertiseManager();
    source.requestSnapshot(42);
    completeInitialSnapshot(*adapter, 1, QStringLiteral("1"), 1);

    QCOMPARE(eventOrder, QStringList({QStringLiteral("changed"), QStringLiteral("ready")}));
    source.stop();
}

void TyphonWindowSourceTest::completedTokenCanBeReused()
{
    auto *adapter = new FakeTyphonAdapter;
    auto *connection = new TyphonToplevelConnection(adapter);
    TyphonWindowSource source(connection);
    QSignalSpy readySpy(&source, &CompositorBackend::snapshotReady);
    QSignalSpy errorSpy(&source, &CompositorBackend::backendError);

    source.start();
    adapter->advertiseManager();
    completeInitialSnapshot(*adapter, 1, QStringLiteral("1"), 1);

    source.requestSnapshot(42);
    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).value<RequestToken>(), RequestToken(42));
    QCOMPARE(readySpy.at(0).at(1).value<WindowSnapshot>().revision, Revision(1));

    updateCommittedSnapshot(*adapter, 1, 2, QStringLiteral("Updated"));
    source.requestSnapshot(42);

    QCOMPARE(readySpy.count(), 2);
    QCOMPARE(readySpy.at(1).at(0).value<RequestToken>(), RequestToken(42));
    QCOMPARE(readySpy.at(1).at(1).value<WindowSnapshot>().revision, Revision(2));
    QCOMPARE(readySpy.at(1).at(1).value<WindowSnapshot>().windows.first().title,
             QStringLiteral("Updated"));
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(source.state(), BackendState::Ready);
    source.stop();
}

void TyphonWindowSourceTest::generationFailureAllowsSameTokenReuse()
{
    auto *adapter = new FakeTyphonAdapter;
    auto *connection = new TyphonToplevelConnection(adapter);
    TyphonWindowSource source(connection);
    QSignalSpy readySpy(&source, &CompositorBackend::snapshotReady);
    QSignalSpy errorSpy(&source, &CompositorBackend::backendError);

    source.start();
    adapter->advertiseManager();
    source.requestSnapshot(42);
    QCOMPARE(readySpy.count(), 0);

    connection->stop();

    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).value<RequestToken>(), RequestToken(42));
    QVERIFY(readySpy.at(0).at(1).value<WindowSnapshot>().windows.isEmpty());
    QCOMPARE(errorSpy.count(), 1);

    connection->start();
    adapter->advertiseManager();
    source.requestSnapshot(42);
    QCOMPARE(readySpy.count(), 1);
    completeInitialSnapshot(*adapter, 2, QStringLiteral("2"), 2);

    QCOMPARE(readySpy.count(), 2);
    QCOMPARE(readySpy.at(1).at(0).value<RequestToken>(), RequestToken(42));
    QCOMPARE(readySpy.at(1).at(1).value<WindowSnapshot>().revision, Revision(2));
    QCOMPARE(source.state(), BackendState::Ready);
    source.stop();
}

void TyphonWindowSourceTest::stress100SnapshotMappingCycles()
{
    TyphonWindowSource source;
    for (quint64 cycle = 1; cycle <= 100; ++cycle) {
        Toplevel window;
        window.id = QString::number(cycle);
        window.appId = QStringLiteral("org.example.App");
        window.title = QStringLiteral("Example");
        window.pid = 10;
        window.focusSerial = cycle;
        const Snapshot snapshot{{window}, cycle, 1, false, cycle};
        const WindowSnapshot mapped = source.mapSnapshotForTest(snapshot);
        QCOMPARE(mapped.revision, cycle);
        QCOMPARE(mapped.backendGeneration, cycle);
        QCOMPARE(mapped.windows.size(), 1);
        QCOMPARE(mapped.windows.first().windowId.value, window.id);
    }
}

void TyphonWindowSourceTest::stress100RequestBeforeInitialSnapshotCycles()
{
    for (RequestToken cycle = 1; cycle <= 100; ++cycle) {
        auto *adapter = new FakeTyphonAdapter;
        auto *connection = new TyphonToplevelConnection(adapter);
        TyphonWindowSource source(connection);
        QSignalSpy readySpy(&source, &CompositorBackend::snapshotReady);

        source.start();
        adapter->advertiseManager();
        source.requestSnapshot(cycle);
        QCOMPARE(readySpy.count(), 0);
        completeInitialSnapshot(*adapter, cycle, QString::number(cycle), cycle);
        QCOMPARE(readySpy.count(), 1);
        QCOMPARE(readySpy.at(0).at(0).value<RequestToken>(), cycle);
        source.stop();
    }
}

void TyphonWindowSourceTest::stress100SameTokenRequestCycles()
{
    auto *adapter = new FakeTyphonAdapter;
    auto *connection = new TyphonToplevelConnection(adapter);
    TyphonWindowSource source(connection);
    QSignalSpy readySpy(&source, &CompositorBackend::snapshotReady);
    QSignalSpy errorSpy(&source, &CompositorBackend::backendError);

    source.start();
    adapter->advertiseManager();
    completeInitialSnapshot(*adapter, 1, QStringLiteral("1"), 1);

    for (Revision revision = 1; revision <= 100; ++revision) {
        source.requestSnapshot(42);
        QCOMPARE(readySpy.count(), static_cast<qsizetype>(revision));
        QCOMPARE(readySpy.last().at(0).value<RequestToken>(), RequestToken(42));
        QCOMPARE(readySpy.last().at(1).value<WindowSnapshot>().revision, revision);

        if (revision < 100)
            updateCommittedSnapshot(*adapter, 1, revision + 1,
                                    QStringLiteral("Revision %1").arg(revision + 1));
    }

    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(source.state(), BackendState::Ready);
    source.stop();
}

void TyphonWindowSourceTest::stress100GenerationFailureSameTokenReuse()
{
    for (int cycle = 1; cycle <= 100; ++cycle) {
        auto *adapter = new FakeTyphonAdapter;
        auto *connection = new TyphonToplevelConnection(adapter);
        TyphonWindowSource source(connection);
        QSignalSpy readySpy(&source, &CompositorBackend::snapshotReady);
        QSignalSpy errorSpy(&source, &CompositorBackend::backendError);

        source.start();
        adapter->advertiseManager();
        source.requestSnapshot(42);
        QCOMPARE(readySpy.count(), 0);

        connection->stop();
        QCOMPARE(readySpy.count(), 1);
        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(readySpy.at(0).at(1).value<WindowSnapshot>().windows.isEmpty());

        connection->start();
        adapter->advertiseManager();
        source.requestSnapshot(42);
        QCOMPARE(readySpy.count(), 1);
        completeInitialSnapshot(*adapter, 2, QString::number(cycle), cycle + 1);

        QCOMPARE(readySpy.count(), 2);
        QCOMPARE(readySpy.at(1).at(0).value<RequestToken>(), RequestToken(42));
        QCOMPARE(readySpy.at(1).at(1).value<WindowSnapshot>().revision,
                 Revision(cycle + 1));
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(source.state(), BackendState::Ready);
        source.stop();
    }
}

void TyphonWindowSourceTest::stress100MinimizedWindowCycles()
{
    TyphonWindowSource source;
    for (quint64 cycle = 1; cycle <= 100; ++cycle) {
        Toplevel window;
        window.id = QString::number(cycle);
        window.appId = QStringLiteral("org.example.App");
        window.title = QStringLiteral("Minimized");
        window.states = ToplevelStates(ToplevelStateFlag::Minimized);
        const WindowSnapshot mapped = source.mapSnapshotForTest(Snapshot{{window}, cycle, 1, false, cycle});
        QCOMPARE(mapped.windows.size(), 1);
        QVERIFY(mapped.windows.first().isMinimized);
        QVERIFY(!mapped.windows.first().isHidden);
        QVERIFY(!mapped.windows.first().isActive);
    }
}

QTEST_MAIN(TyphonWindowSourceTest)
#include "TyphonWindowSourceTest.moc"
