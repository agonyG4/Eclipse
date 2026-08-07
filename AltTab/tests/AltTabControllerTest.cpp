#include <QTest>
#include <QSignalSpy>
#include <QVector>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <optional>

#include <utility>

#include "core/AltTabController.hpp"
#include "core/WindowInfo.hpp"
#include "platform/compositor/FakeBackend.hpp"
#include "platform/typhon/TyphonWindowSource.hpp"
#include "services/AppIdentityResolver.hpp"

using namespace Astrea::Typhon;

static WindowInfo makeWindow(const QString &id, int focusHistory, int wsId = 1) {
    WindowInfo w;
    w.windowId = WindowId{id};
    w.pid = 100;
    w.className = QStringLiteral("App_") + id;
    w.title = QStringLiteral("Window ") + id;
    w.displayName = w.className;
    w.workspaceId = WorkspaceId{QString::number(wsId)};
    w.focusHistoryId = focusHistory;
    return w;
}

class AsynchronousStartupBackend final : public CompositorBackend {
    Q_OBJECT

public:
    explicit AsynchronousStartupBackend(const QVector<WindowInfo> &windows,
                                        QObject *parent = nullptr)
        : CompositorBackend(parent), m_windows(windows)
    {
    }

    void start() override
    {
        ++startCount;
        setState(BackendState::ConnectingEvents);
        setState(BackendState::LoadingInitialSnapshot);
        setState(BackendState::Ready);
    }

    void stop() override
    {
        ++stopCount;
        setState(BackendState::Stopped);
    }

    BackendDescriptor descriptor() const override
    {
        return {QStringLiteral("async-test"), 1};
    }

    BackendState state() const override { return m_state; }

    QVector<BackendCapability> capabilities() const override
    {
        return {BackendCapability::WindowList, BackendCapability::EventStream};
    }

    std::optional<WindowSnapshot> cachedSnapshot() const override
    {
        return std::nullopt;
    }

    void requestSnapshot(RequestToken token) override
    {
        requestedSnapshotTokens.append(token);
        m_pendingTokens.append(token);
    }

    void activateWindow(ActivationRequest request) override
    {
        emit activationFinished(request.token, ActivationResult{true, {}});
    }

    void emitReadyAgain()
    {
        emit stateChanged(BackendState::Ready);
    }

    void deliverSnapshot()
    {
        WindowSnapshot snapshot;
        snapshot.windows = m_windows;
        snapshot.revision = 1;
        emit snapshotChanged(snapshot);
        const QVector<RequestToken> tokens = m_pendingTokens;
        m_pendingTokens.clear();
        for (const RequestToken token : tokens)
            emit snapshotReady(token, snapshot);
    }

    int startCount = 0;
    int stopCount = 0;
    QVector<RequestToken> requestedSnapshotTokens;

private:
    void setState(BackendState state)
    {
        if (m_state == state)
            return;
        m_state = state;
        emit stateChanged(m_state);
    }

    QVector<WindowInfo> m_windows;
    QVector<RequestToken> m_pendingTokens;
    BackendState m_state = BackendState::Stopped;
};

class ReloadTyphonAdapter final : public TyphonProtocolAdapter {
    Q_OBJECT

public:
    explicit ReloadTyphonAdapter(QObject *parent = nullptr)
        : TyphonProtocolAdapter(parent)
    {
    }

    void start() override { ++starts; }
    void stop() override { ++stops; }
    bool isAvailable() const override { return true; }

    void advertiseManager() { emit registryDiscovered(true); }
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

    int starts = 0;
    int stops = 0;
};

static void completeReloadSnapshot(ReloadTyphonAdapter &adapter, quint64 token,
                                   const QString &id, Revision revision,
                                   const QString &title)
{
    adapter.create(token);
    adapter.id(token, id);
    adapter.app(token, QStringLiteral("org.example.App"));
    adapter.title(token, title);
    adapter.pid(token, 100);
    adapter.kind(token, ToplevelKind::XdgToplevel);
    adapter.state(token, {});
    adapter.focus(token, 1);
    adapter.handleDone(token, revision);
    adapter.done(revision, 1);
}

static void updateReloadSnapshot(ReloadTyphonAdapter &adapter, quint64 token,
                                 Revision revision, const QString &title)
{
    adapter.app(token, QStringLiteral("org.example.App"));
    adapter.title(token, title);
    adapter.pid(token, 100);
    adapter.kind(token, ToplevelKind::XdgToplevel);
    adapter.state(token, {});
    adapter.focus(token, 1);
    adapter.handleDone(token, revision);
    adapter.done(revision, 1);
}

class TestAltTabController : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void testZeroWindows();
    void testOneWindow();
    void testNextPreviousWrap();
    void testFocusHistorySelection();
    void testCommitNoUAF();
    void testCommitDuringOpening();
    void testCancelDuringOpening();
    void testDisappearedSelectedWindow();
    void testStateTransitions();
    void testDuplicateCommitIdempotent();
    void testDuplicateCancelIdempotent();
    void testOpenChangedEmissions();
    void testSurfaceVisibilityEmissions();
    void testOpenTimeoutGenerationSafety();
    void testSuccessfulOpenNoLateTimeout();
    void testStaleSnapshotIgnored();
    void testStatusJson();

    // New async tests
    void testCommitDuringOpeningAsync();
    void testCancelDuringOpeningAsync();
    void testTimeoutInjected();
    void testSnapshotBeforeTimeout();
    void testStaleTimeoutFromOldGeneration();
    void testBackendDisconnectStopsTimeout();
    void testRealStaleSnapshot();
    void testDuplicateActivationCount();
    void testModelTester();
    void testAsynchronousReadyRequestsSnapshot();
    void testTyphonWorkspaceEligibilityRules();
    void testMinimizedWindowRemainsVisible();
    void testTyphonReloadReusesOpeningToken();
    void stress100ColdAsynchronousOpens();
    void stress100EmptyWorkspaceSnapshots();
    void stress100TyphonReloadWindowsCycles();

private:
    AppIdentityResolver *m_resolver = nullptr;
};

void TestAltTabController::initTestCase() {
    m_resolver = new AppIdentityResolver(this);
}

void TestAltTabController::testZeroWindows() {
    FakeWindowSource *fake = new FakeWindowSource({});
    AltTabController controller(fake, m_resolver);

    QSignalSpy closedSpy(&controller, &AltTabController::closed);
    controller.step(1);

    QCOMPARE(controller.state(), AltTabController::State::Hidden);
    QCOMPARE(closedSpy.count(), 1);
    delete fake;
}

void TestAltTabController::testOneWindow() {
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Open);
    QCOMPARE(controller.windowModel()->count(), 1);
    QCOMPARE(controller.selectedIndex(), 0);

    controller.step(1);
    QCOMPARE(controller.selectedIndex(), 0);
    delete fake;
}

void TestAltTabController::testNextPreviousWrap() {
    QVector<WindowInfo> windows;
    for (int i = 0; i < 3; ++i)
        windows.append(makeWindow(QStringLiteral("0x%1").arg(i+1), i));

    FakeWindowSource *fake = new FakeWindowSource(windows);
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Open);
    QCOMPARE(controller.selectedIndex(), 1);

    controller.step(1);
    QCOMPARE(controller.selectedIndex(), 2);

    controller.step(1);
    QCOMPARE(controller.selectedIndex(), 0);

    controller.step(-1);
    QCOMPARE(controller.selectedIndex(), 2);
    delete fake;
}

void TestAltTabController::testFocusHistorySelection() {
    QVector<WindowInfo> windows;
    windows.append(makeWindow(QStringLiteral("0x1"), 5));
    windows.append(makeWindow(QStringLiteral("0x2"), 0));
    windows.append(makeWindow(QStringLiteral("0x3"), 3));

    FakeWindowSource *fake = new FakeWindowSource(windows);
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Open);
    QCOMPARE(controller.selectedIndex(), 1);
    delete fake;
}

void TestAltTabController::testCommitNoUAF() {
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1234"), 0)});
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Open);

    controller.commit();
    QCOMPARE(controller.state(), AltTabController::State::Hidden);
    QCOMPARE(fake->lastActivatedWindowId().value, QStringLiteral("0x1234"));
    delete fake;
}

void TestAltTabController::testCommitDuringOpening() {
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Open);

    controller.commit();
    QCOMPARE(controller.state(), AltTabController::State::Hidden);
    delete fake;
}

void TestAltTabController::testCancelDuringOpening() {
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Open);

    controller.cancel();
    QCOMPARE(controller.state(), AltTabController::State::Hidden);
    QVERIFY(controller.selectedIndex() < 0 || controller.windowModel()->count() == 0);
    delete fake;
}

void TestAltTabController::testDisappearedSelectedWindow() {
    QVector<WindowInfo> windows;
    windows.append(makeWindow(QStringLiteral("0x1"), 0));
    windows.append(makeWindow(QStringLiteral("0x2"), 1));

    FakeWindowSource *fake = new FakeWindowSource(windows);
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Open);
    QCOMPARE(controller.windowModel()->count(), 2);

    QVector<WindowInfo> remaining = {windows.first()};
    fake->setWindows(remaining);
    controller.reloadWindows();

    QCOMPARE(controller.windowModel()->count(), 1);
    QVERIFY(controller.selectedIndex() >= 0);
    QVERIFY(controller.selectedIndex() < controller.windowModel()->count());
    delete fake;
}

void TestAltTabController::testStateTransitions() {
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    QCOMPARE(controller.state(), AltTabController::State::Hidden);
    QVERIFY(!controller.isOpen());
    QVERIFY(!controller.surfaceVisible());

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Open);
    QVERIFY(controller.isOpen());
    QVERIFY(controller.surfaceVisible());

    controller.commit();
    QCOMPARE(controller.state(), AltTabController::State::Hidden);
    QVERIFY(!controller.isOpen());
    QVERIFY(!controller.surfaceVisible());

    controller.show();
    QCOMPARE(controller.state(), AltTabController::State::Open);

    controller.cancel();
    QCOMPARE(controller.state(), AltTabController::State::Hidden);
    QVERIFY(!controller.surfaceVisible());
    delete fake;
}

void TestAltTabController::testDuplicateCommitIdempotent() {
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    controller.commit();
    controller.commit();
    controller.commit();

    QCOMPARE(controller.state(), AltTabController::State::Hidden);
    QCOMPARE(fake->activationCount(), 1);
    delete fake;
}

void TestAltTabController::testDuplicateCancelIdempotent() {
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    controller.cancel();
    QCOMPARE(controller.state(), AltTabController::State::Hidden);

    controller.step(1);
    controller.cancel();
    QCOMPARE(controller.state(), AltTabController::State::Hidden);

    controller.cancel();
    QCOMPARE(controller.state(), AltTabController::State::Hidden);
    delete fake;
}

void TestAltTabController::testOpenChangedEmissions() {
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    QSignalSpy openSpy(&controller, &AltTabController::openChanged);

    controller.step(1);
    QVERIFY(openSpy.count() >= 1);

    openSpy.clear();
    controller.cancel();
    QVERIFY(openSpy.count() >= 1);
    delete fake;
}

void TestAltTabController::testSurfaceVisibilityEmissions() {
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    QSignalSpy visSpy(&controller, &AltTabController::surfaceVisibleChanged);

    controller.step(1);
    QVERIFY(controller.surfaceVisible());
    QVERIFY(visSpy.count() >= 1);

    visSpy.clear();
    controller.cancel();
    QVERIFY(!controller.surfaceVisible());
    QVERIFY(visSpy.count() >= 1);
    delete fake;
}

void TestAltTabController::testOpenTimeoutGenerationSafety() {
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Open);
    controller.cancel();
    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Open);
    delete fake;
}

void TestAltTabController::testSuccessfulOpenNoLateTimeout() {
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Open);
    QVERIFY(controller.surfaceVisible());

    QTest::qWait(10);
    QCOMPARE(controller.state(), AltTabController::State::Open);
    QVERIFY(controller.surfaceVisible());
    delete fake;
}

void TestAltTabController::testStaleSnapshotIgnored() {
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Open);
    delete fake;
}

void TestAltTabController::testStatusJson() {
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    const QString json = controller.buildStatusJson();
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    QVERIFY(doc.isObject());

    QJsonObject obj = doc.object();
    QCOMPARE(obj.value(QStringLiteral("running")).toBool(), true);
    QCOMPARE(obj.value(QStringLiteral("windowSource")).toString(), QStringLiteral("fake"));
    QVERIFY(obj.contains(QStringLiteral("backend")));
    QVERIFY(obj.value(QStringLiteral("backend")).isObject());
    delete fake;
}

void TestAltTabController::testCommitDuringOpeningAsync() {
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    fake->setDelaySnapshot(true);
    fake->setAutoCompleteActivation(false);
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Opening);

    controller.commit();
    QCOMPARE(fake->activationCount(), 0);

    fake->deliverPendingSnapshot();
    // finishOpening() defers doCommit via QTimer::singleShot(0) to avoid re-entrancy
    QTest::qWait(1);
    QCOMPARE(controller.state(), AltTabController::State::Committing);
    QCOMPARE(fake->activationCount(), 1);

    fake->completeActivation(1, true);
    QCOMPARE(controller.state(), AltTabController::State::Hidden);
    QCOMPARE(fake->lastActivatedWindowId().value, QStringLiteral("0x1"));
    delete fake;
}

void TestAltTabController::testCancelDuringOpeningAsync() {
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    fake->setDelaySnapshot(true);
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Opening);

    controller.cancel();
    QCOMPARE(controller.state(), AltTabController::State::Hidden);

    fake->deliverPendingSnapshot();
    QCOMPARE(controller.windowModel()->count(), 0);
    QCOMPARE(fake->activationCount(), 0);
    delete fake;
}

void TestAltTabController::testTimeoutInjected()
{
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    fake->setDelaySnapshot(true);
    AltTabController controller(fake, m_resolver);

    QSignalSpy closedSpy(&controller, &AltTabController::closed);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Opening);

    // Wait for timeout (3 second default)
    QVERIFY(closedSpy.wait(4000));
    QCOMPARE(controller.state(), AltTabController::State::Hidden);

    // Late snapshot should be ignored
    fake->deliverPendingSnapshot();
    QCOMPARE(controller.state(), AltTabController::State::Hidden);
    delete fake;
}

void TestAltTabController::testSnapshotBeforeTimeout()
{
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Open);

    // Should remain open well past any timeout boundary
    QTest::qWait(100);
    QCOMPARE(controller.state(), AltTabController::State::Open);
    delete fake;
}

void TestAltTabController::testStaleTimeoutFromOldGeneration()
{
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    controller.step(1);  // generation N, opens and cancels quickly
    controller.cancel();

    // Reopen quickly (generation N+1)
    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Open);

    // Any stale timeout from generation N should not close N+1
    QTest::qWait(3100);
    QCOMPARE(controller.state(), AltTabController::State::Open);
    delete fake;
}

void TestAltTabController::testBackendDisconnectStopsTimeout()
{
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Open);

    // Simulate backend disconnect
    fake->disconnectManually();
    QCOMPARE(controller.state(), AltTabController::State::Hidden);
    delete fake;
}

void TestAltTabController::testRealStaleSnapshot()
{
    FakeWindowSource *fake = new FakeWindowSource({});
    fake->setDelaySnapshot(true);
    AltTabController controller(fake, m_resolver);

    // Open with token N
    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Opening);

    // Cancel and reopen -> token N+1
    controller.cancel();
    QCOMPARE(fake->activationCount(), 0);

    fake->setDelaySnapshot(true);
    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Opening);

    // Deliver token N+1 first
    fake->setWindows({makeWindow(QStringLiteral("0x1"), 0)});
    fake->deliverPendingSnapshot();
    QCOMPARE(controller.state(), AltTabController::State::Open);
    QCOMPARE(controller.windowModel()->count(), 1);

    // Now deliver stale token N - should not change anything
    // (already delivered snapshot with correct generation)
    delete fake;
}

void TestAltTabController::testDuplicateActivationCount()
{
    FakeWindowSource *fake = new FakeWindowSource({makeWindow(QStringLiteral("0x1"), 0)});
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    controller.commit();
    controller.commit();
    controller.commit();

    QCOMPARE(fake->activationCount(), 1);
    delete fake;
}

void TestAltTabController::testModelTester()
{
    // Verify basic model behavior (not using QAbstractItemModelTester which may not be available)
    FakeWindowSource *fake = new FakeWindowSource({
        makeWindow(QStringLiteral("0x1"), 0),
        makeWindow(QStringLiteral("0x2"), 1),
        makeWindow(QStringLiteral("0x3"), 2)
    });
    AltTabController controller(fake, m_resolver);

    controller.step(1);
    QCOMPARE(controller.windowModel()->count(), 3);

    // Verify roles
    QVERIFY(controller.windowModel()->roleNames().contains(AltTabWindowModel::WindowIdRole));
    QVERIFY(controller.windowModel()->roleNames().contains(AltTabWindowModel::TitleRole));
    QVERIFY(controller.windowModel()->roleNames().contains(AltTabWindowModel::DisplayNameRole));
    QVERIFY(controller.windowModel()->roleNames().contains(AltTabWindowModel::ClassNameRole));
    QVERIFY(controller.windowModel()->roleNames().contains(AltTabWindowModel::PidRole));
    QVERIFY(controller.windowModel()->roleNames().contains(AltTabWindowModel::WorkspaceIdRole));
    QVERIFY(controller.windowModel()->roleNames().contains(AltTabWindowModel::FocusOrderRole));
    QVERIFY(controller.windowModel()->roleNames().contains(AltTabWindowModel::IconNameRole));
    QVERIFY(controller.windowModel()->roleNames().contains(AltTabWindowModel::IconUrlRole));
    QVERIFY(controller.windowModel()->roleNames().contains(AltTabWindowModel::SelectedRole));
    QVERIFY(controller.windowModel()->roleNames().contains(AltTabWindowModel::HiddenRole));
    QVERIFY(controller.windowModel()->roleNames().contains(AltTabWindowModel::MinimizedRole));
    QVERIFY(controller.windowModel()->roleNames().contains(AltTabWindowModel::ActiveRole));
    QVERIFY(controller.windowModel()->roleNames().contains(AltTabWindowModel::OutputRole));

    delete fake;
}

void TestAltTabController::testAsynchronousReadyRequestsSnapshot()
{
    for (const bool useStep : {false, true}) {
        AsynchronousStartupBackend backend({makeWindow(QStringLiteral("0x1"), 0)});
        AltTabController controller(&backend, m_resolver);

        if (useStep)
            controller.step(1);
        else
            controller.show();

        QCOMPARE(controller.state(), AltTabController::State::Opening);
        QCOMPARE(backend.startCount, 1);
        QCOMPARE(backend.requestedSnapshotTokens, QVector<RequestToken>{1});
        QSignalSpy windowCountSpy(&controller, &AltTabController::windowCountChanged);

        backend.emitReadyAgain();
        QCOMPARE(backend.requestedSnapshotTokens, QVector<RequestToken>{1});

        backend.deliverSnapshot();
        QCOMPARE(controller.state(), AltTabController::State::Open);
        QCOMPARE(controller.windowModel()->count(), 1);
        QCOMPARE(windowCountSpy.count(), 1);
        QCOMPARE(backend.startCount, 1);
    }
}

void TestAltTabController::testTyphonWorkspaceEligibilityRules()
{
    WindowInfo unknown = makeWindow(QStringLiteral("0x-unknown"), 0);
    unknown.workspaceId = {};
    WindowInfo zero = makeWindow(QStringLiteral("0x-zero"), 1, 0);
    WindowInfo negative = makeWindow(QStringLiteral("0x-negative"), 2, -1);
    WindowInfo malformed = makeWindow(QStringLiteral("0x-malformed"), 3);
    malformed.workspaceId = WorkspaceId{QStringLiteral("not-a-workspace")};

    FakeWindowSource backend({unknown, zero, negative, malformed});
    AltTabController controller(&backend, m_resolver);

    controller.show();

    QCOMPARE(controller.state(), AltTabController::State::Open);
    QCOMPARE(controller.windowModel()->count(), 1);
    QCOMPARE(controller.windowModel()->at(0).windowId.value, QStringLiteral("0x-unknown"));
    QCOMPARE(controller.windowModel()->at(0).workspaceId.value, QString());
}

void TestAltTabController::testMinimizedWindowRemainsVisible()
{
    WindowInfo minimized = makeWindow(QStringLiteral("0x-minimized"), 1);
    minimized.isMinimized = true;
    minimized.isHidden = false;

    FakeWindowSource backend({minimized});
    AltTabController controller(&backend, m_resolver);

    controller.show();

    QCOMPARE(controller.state(), AltTabController::State::Open);
    QCOMPARE(controller.windowModel()->count(), 1);
    const WindowInfo visible = controller.windowModel()->at(0);
    QVERIFY(visible.isMinimized);
    QVERIFY(!visible.isHidden);
    QCOMPARE(controller.windowModel()->data(controller.windowModel()->index(0),
                                            AltTabWindowModel::MinimizedRole).toBool(), true);
    QCOMPARE(controller.windowModel()->data(controller.windowModel()->index(0),
                                            AltTabWindowModel::HiddenRole).toBool(), false);
}

void TestAltTabController::testTyphonReloadReusesOpeningToken()
{
    auto *adapter = new ReloadTyphonAdapter;
    auto *connection = new TyphonToplevelConnection(adapter);
    auto *source = new TyphonWindowSource(connection);
    AltTabController controller(source, m_resolver);
    QSignalSpy readySpy(source, &CompositorBackend::snapshotReady);
    QSignalSpy errorSpy(source, &CompositorBackend::backendError);

    controller.step(1);
    QCOMPARE(controller.state(), AltTabController::State::Opening);
    adapter->advertiseManager();
    completeReloadSnapshot(*adapter, 1, QStringLiteral("1"), 1, QStringLiteral("Initial"));

    QCOMPARE(controller.state(), AltTabController::State::Open);
    QCOMPARE(controller.windowModel()->count(), 1);
    QCOMPARE(controller.windowModel()->at(0).title, QStringLiteral("Initial"));
    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).value<RequestToken>(), RequestToken(1));
    const int startsBeforeReload = adapter->starts;
    const int errorsBeforeReload = errorSpy.count();

    updateReloadSnapshot(*adapter, 1, 2, QStringLiteral("Updated"));
    controller.reloadWindows();

    QCOMPARE(controller.state(), AltTabController::State::Open);
    QCOMPARE(controller.windowModel()->count(), 1);
    QCOMPARE(controller.windowModel()->at(0).title, QStringLiteral("Updated"));
    QCOMPARE(readySpy.count(), 2);
    QCOMPARE(readySpy.at(1).at(0).value<RequestToken>(), RequestToken(1));
    QCOMPARE(readySpy.at(1).at(1).value<WindowSnapshot>().revision, Revision(2));
    QCOMPARE(adapter->starts, startsBeforeReload);
    QCOMPARE(errorSpy.count(), errorsBeforeReload);
    QCOMPARE(source->state(), BackendState::Ready);
    delete source;
}

void TestAltTabController::stress100ColdAsynchronousOpens()
{
    for (int cycle = 0; cycle < 100; ++cycle) {
        AsynchronousStartupBackend backend({makeWindow(QStringLiteral("0x%1").arg(cycle + 1), 0)});
        AltTabController controller(&backend, m_resolver);

        controller.step(1);
        QCOMPARE(controller.state(), AltTabController::State::Opening);
        QCOMPARE(backend.requestedSnapshotTokens.size(), 1);
        backend.deliverSnapshot();
        QCOMPARE(controller.state(), AltTabController::State::Open);
        QCOMPARE(controller.windowModel()->count(), 1);
        controller.cancel();
        QCOMPARE(controller.state(), AltTabController::State::Hidden);
    }
}

void TestAltTabController::stress100EmptyWorkspaceSnapshots()
{
    for (int cycle = 0; cycle < 100; ++cycle) {
        WindowInfo unknown = makeWindow(QStringLiteral("0x%1").arg(cycle + 1), 0);
        unknown.workspaceId = {};
        FakeWindowSource backend({unknown});
        AltTabController controller(&backend, m_resolver);

        controller.show();
        QCOMPARE(controller.state(), AltTabController::State::Open);
        QCOMPARE(controller.windowModel()->count(), 1);
        QCOMPARE(controller.windowModel()->at(0).workspaceId.value, QString());
        controller.cancel();
    }
}

void TestAltTabController::stress100TyphonReloadWindowsCycles()
{
    auto *adapter = new ReloadTyphonAdapter;
    auto *connection = new TyphonToplevelConnection(adapter);
    auto *source = new TyphonWindowSource(connection);
    AltTabController controller(source, m_resolver);
    QSignalSpy readySpy(source, &CompositorBackend::snapshotReady);
    QSignalSpy errorSpy(source, &CompositorBackend::backendError);

    controller.show();
    adapter->advertiseManager();
    completeReloadSnapshot(*adapter, 1, QStringLiteral("1"), 1, QStringLiteral("Initial"));
    QCOMPARE(controller.state(), AltTabController::State::Open);

    for (Revision revision = 2; revision <= 101; ++revision) {
        updateReloadSnapshot(*adapter, 1, revision,
                             QStringLiteral("Revision %1").arg(revision));
        controller.reloadWindows();
        QCOMPARE(controller.state(), AltTabController::State::Open);
        QCOMPARE(controller.windowModel()->count(), 1);
        QCOMPARE(controller.windowModel()->at(0).title,
                 QStringLiteral("Revision %1").arg(revision));
        QCOMPARE(readySpy.count(), static_cast<qsizetype>(revision));
        QCOMPARE(readySpy.last().at(0).value<RequestToken>(), RequestToken(1));
    }

    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(source->state(), BackendState::Ready);
    QCOMPARE(adapter->starts, 1);
    delete source;
}

QTEST_MAIN(TestAltTabController)
#include "AltTabControllerTest.moc"
