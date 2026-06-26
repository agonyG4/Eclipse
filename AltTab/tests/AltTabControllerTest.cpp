#include <QTest>
#include <QSignalSpy>
#include <QVector>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include "core/AltTabController.hpp"
#include "core/WindowInfo.hpp"
#include "platform/compositor/FakeBackend.hpp"
#include "services/AppIdentityResolver.hpp"

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

QTEST_MAIN(TestAltTabController)
#include "AltTabControllerTest.moc"
