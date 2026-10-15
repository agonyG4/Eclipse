#include <QTest>
#include <QSet>
#include <QSignalSpy>

#include "core/DockController.hpp"
#include "platform/typhon/TyphonDockRuntimeBackend.hpp"
#include "platform/typhon/TyphonToplevelConnection.hpp"

using namespace Astrea::Typhon;
using Astrea::Dock::TyphonDockRuntimeBackend;

class FakeLauncher final : public ApplicationLauncher {
    Q_OBJECT

public:
    FakeLauncher() : ApplicationLauncher(QStringLiteral("fake-launcher")) {}

    void launchDesktop(const ApplicationLaunchRequest &request) override
    {
        requests.append(request);
        emit launchAccepted(request.desktopId.isEmpty() ? request.desktopFileName : request.desktopId);
    }

    QVector<ApplicationLaunchRequest> requests;
};

class FakeTyphonAdapter final : public TyphonProtocolAdapter {
    Q_OBJECT

public:
    explicit FakeTyphonAdapter(QObject *parent = nullptr)
        : TyphonProtocolAdapter(parent)
    {
    }

    void start() override { ++starts; }
    void stop() override { ++stops; }
    bool isAvailable() const override { return true; }
    TyphonActionCapabilityState actionCapability() const override { return capability; }
    quint32 managerVersion() const override { return protocolVersion; }
    std::optional<ToplevelActionError> requestAction(
        quint64 handleToken, TyphonActionToken token, ToplevelAction action) override
    {
        if (staleHandleTokens.contains(handleToken))
            return ToplevelActionError::ToplevelNotLive;
        actionRequests.append({handleToken, token, action});
        return requestError;
    }
    std::optional<ToplevelActionError> setMinimizeAnchor(
        quint64 handleToken, const QRect &rect) override
    {
        anchorRequests.append({handleToken, rect});
        return anchorError;
    }
    std::optional<ToplevelActionError> clearMinimizeAnchor(quint64 handleToken) override
    {
        clearedAnchorHandles.append(handleToken);
        return anchorError;
    }

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
    void managerDone(Revision revision, quint32 total)
    {
        emit managerCompleted(revision, total, false);
    }
    void close(quint64 token)
    {
        staleHandleTokens.insert(token);
        emit handleClosed(token);
    }
    void disconnectDisplay() { emit displayDisconnected(); }
    void advertiseActionCapability(TyphonActionCapabilityState value)
    {
        capability = value;
        emit actionCapabilityChanged(value);
    }
    void completeAction(ToplevelActionResult result)
    {
        const auto request = actionRequests.takeLast();
        emit actionCompleted(request.token.hi, request.token.lo, request.action, result);
    }

    int starts = 0;
    int stops = 0;
    quint32 protocolVersion = 3;
    TyphonActionCapabilityState capability = TyphonActionCapabilityState::ActionReadyV2;
    std::optional<ToplevelActionError> requestError;
    struct ActionRequest {
        quint64 handleToken = 0;
        TyphonActionToken token;
        ToplevelAction action = ToplevelAction::Activate;
    };
    QVector<ActionRequest> actionRequests;
    QSet<quint64> staleHandleTokens;
    struct AnchorRequest {
        quint64 handleToken = 0;
        QRect rect;
    };
    QVector<AnchorRequest> anchorRequests;
    QVector<quint64> clearedAnchorHandles;
    std::optional<ToplevelActionError> anchorError;
};

static std::shared_ptr<DesktopEntrySnapshot> makeCatalog()
{
    auto snapshot = std::make_shared<DesktopEntrySnapshot>();
    DesktopEntryRecord record;
    record.desktopFileName = QStringLiteral("one.desktop");
    record.id = QStringLiteral("one");
    record.name = QStringLiteral("One");
    const int index = snapshot->entries.size();
    snapshot->entries.append(record);
    snapshot->byDesktopFileName.insert(record.desktopFileName, index);
    snapshot->byDesktopId.insert(record.id, index);

    record = {};
    record.desktopFileName = QStringLiteral("two.desktop");
    record.id = QStringLiteral("two");
    record.name = QStringLiteral("Two");
    const int secondIndex = snapshot->entries.size();
    snapshot->entries.append(record);
    snapshot->byDesktopFileName.insert(record.desktopFileName, secondIndex);
    snapshot->byDesktopId.insert(record.id, secondIndex);
    return snapshot;
}

class DockTyphonRuntimeIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void authoritativeSnapshotDrivesDockRuntimeRoles();
    void initialReadyTransitionPublishesOneNeutralSnapshot();
    void runningApplicationActivatesMostRecentExactWindow();
    void unavailableActivationNeverLaunchesSameClick();
    void staleExactTargetNeverRetargetsOrLaunches();
    void nonPinnedMinimizedApplicationAppearsActivatesAndCloses();
    void runtimeOnlyTaskActivatesAndClosesExactWindow();
    void runtimeOnlyTaskAnchorTargetsAllWindows();
    void actionReadyReplaysCachedAnchorWithoutAnotherSnapshot();
    void clearRequestedDuringAuthenticationRetriesWhenReady();
    void anchorFailureRetriesOnNextProjectionWithoutDuplicateQueuedSends();
    void newConnectionGenerationReplaysCachedAnchor();
    void removedTaskAnchorDoesNotLeakToAnotherApplication();
    void recreatedHandleReceivesCachedAnchorForExactWindow();
    void sameWindowIdTaskMigrationClearsThePreviousApplicationsAnchor();
    void unsupportedProtocolKeepsAnchorLocalAndDegraded();
    void connectionLossRemovesDynamicRowsButKeepsPinsUnknown();
    void cachedAnchorProjectsToExistingAndNewWindows();
    void catalogRebuildKeepsExactActionsAndMinimizeAnchorOnStableTask();
};

void DockTyphonRuntimeIntegrationTest::authoritativeSnapshotDrivesDockRuntimeRoles()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    DockController controller;
    controller.setCatalogSnapshot(makeCatalog());
    DockConfig config = DockConfig::defaults();
    config.pins = {QStringLiteral("one.desktop")};
    controller.applyConfig(config);
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    adapter->create(1);
    adapter->id(1, QStringLiteral("1"));
    adapter->app(1, QStringLiteral("one"));
    adapter->title(1, QStringLiteral("One"));
    adapter->pid(1, 100);
    adapter->kind(1, ToplevelKind::XdgToplevel);
    adapter->state(1, ToplevelStates{ToplevelStateFlag::Active});
    adapter->focus(1, 1);
    adapter->handleDone(1, 1);
    adapter->managerDone(1, 1);

    const QModelIndex item = controller.appModel()->index(0, 0);
    QVERIFY(controller.runtimeKnown());
    QVERIFY(item.data(DockAppModel::RuntimeKnownRole).toBool());
    QVERIFY(item.data(DockAppModel::RunningRole).toBool());
    QVERIFY(item.data(DockAppModel::ActiveRole).toBool());
    QCOMPARE(item.data(DockAppModel::WindowCountRole).toInt(), 1);

    adapter->disconnectDisplay();
    QVERIFY(!controller.runtimeKnown());
    QVERIFY(!item.data(DockAppModel::RuntimeKnownRole).toBool());
    QVERIFY(!item.data(DockAppModel::RunningRole).toBool());
}

void DockTyphonRuntimeIntegrationTest::initialReadyTransitionPublishesOneNeutralSnapshot()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    QSignalSpy readySpy(&runtimeBackend,
                        &Astrea::Dock::DockRuntimeBackend::runtimeReadyChanged);
    QSignalSpy snapshotSpy(&runtimeBackend,
                           &Astrea::Dock::DockRuntimeBackend::snapshotChanged);

    Astrea::Dock::DockRuntimeSnapshot cachedAtReady;
    bool readySnapshotWasCoherent = false;
    connect(&runtimeBackend, &Astrea::Dock::DockRuntimeBackend::runtimeReadyChanged,
            &runtimeBackend, [&runtimeBackend, &cachedAtReady, &readySnapshotWasCoherent](bool ready) {
        if (!ready)
            return;
        cachedAtReady = runtimeBackend.snapshot();
        readySnapshotWasCoherent = runtimeBackend.runtimeReady()
            && cachedAtReady.authoritative && cachedAtReady.windows.size() == 1
            && cachedAtReady.windows.first().id == QStringLiteral("1");
    });

    connection.start();
    adapter->advertiseManager();
    adapter->create(1);
    adapter->id(1, QStringLiteral("1"));
    adapter->app(1, QStringLiteral("one"));
    adapter->title(1, QStringLiteral("First window"));
    adapter->pid(1, 100);
    adapter->kind(1, ToplevelKind::XdgToplevel);
    adapter->state(1, ToplevelStates{ToplevelStateFlag::Active});
    adapter->focus(1, 1);
    adapter->handleDone(1, 1);
    adapter->managerDone(1, 1);

    QVERIFY(connection.hasSnapshot());
    QCOMPARE(connection.state(), TyphonConnectionState::Ready);
    QCOMPARE(readySpy.count(), 1);
    QVERIFY(readySnapshotWasCoherent);
    QCOMPARE(cachedAtReady.windows.first().appId, QStringLiteral("one"));
    QCOMPARE(cachedAtReady.windows.first().title, QStringLiteral("First window"));
    QCOMPARE(cachedAtReady.windows.first().pid, quint32(100));

    QCOMPARE(snapshotSpy.count(), 1);
    const auto publishedSnapshot = snapshotSpy.takeFirst().at(0)
                                       .value<Astrea::Dock::DockRuntimeSnapshot>();
    QVERIFY(publishedSnapshot.authoritative);
    QCOMPARE(publishedSnapshot.windows.size(), 1);
    QCOMPARE(publishedSnapshot.windows.first().id, QStringLiteral("1"));
    QCOMPARE(publishedSnapshot.windows.first().appId, QStringLiteral("one"));
    QCOMPARE(publishedSnapshot.windows.first().title, QStringLiteral("First window"));
    QCOMPARE(publishedSnapshot.windows.first().pid, quint32(100));
}

void DockTyphonRuntimeIntegrationTest::cachedAnchorProjectsToExistingAndNewWindows()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    DockController controller;
    controller.setCatalogSnapshot(makeCatalog());
    DockConfig config = DockConfig::defaults();
    config.pins = {QStringLiteral("one.desktop")};
    controller.applyConfig(config);
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    adapter->create(1);
    adapter->id(1, QStringLiteral("1"));
    adapter->app(1, QStringLiteral("one"));
    adapter->title(1, QStringLiteral("One"));
    adapter->pid(1, 100);
    adapter->kind(1, ToplevelKind::XdgToplevel);
    adapter->state(1, ToplevelStates{});
    adapter->focus(1, 1);
    adapter->handleDone(1, 1);
    adapter->managerDone(1, 1);

    const QRect anchor(123, -45, 48, 48);
    QVERIFY(controller.setMinimizeAnchor(QStringLiteral("desktop:one.desktop"), anchor));
    QCOMPARE(adapter->anchorRequests.size(), 1);
    QCOMPARE(adapter->anchorRequests.first().handleToken, quint64(1));
    QCOMPARE(adapter->anchorRequests.first().rect, anchor);

    adapter->create(4);
    adapter->id(4, QStringLiteral("4"));
    adapter->app(4, QStringLiteral("one"));
    adapter->title(4, QStringLiteral("One second"));
    adapter->pid(4, 101);
    adapter->kind(4, ToplevelKind::XdgToplevel);
    adapter->state(4, ToplevelStates{});
    adapter->focus(4, 2);
    adapter->handleDone(4, 2);
    adapter->managerDone(2, 2);

    QCOMPARE(adapter->anchorRequests.size(), 2);
    QCOMPARE(adapter->anchorRequests.last().handleToken, quint64(4));
    QCOMPARE(adapter->anchorRequests.last().rect, anchor);
}

void DockTyphonRuntimeIntegrationTest::runningApplicationActivatesMostRecentExactWindow()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.setCatalogSnapshot(makeCatalog());
    DockConfig config = DockConfig::defaults();
    config.pins = {QStringLiteral("one.desktop")};
    controller.applyConfig(config);
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    for (const auto &window : QVector<QPair<quint64, quint64>>{{1, 10}, {2, 30}, {3, 20}}) {
        adapter->create(window.first);
        adapter->id(window.first, QString::number(window.first));
        adapter->app(window.first, QStringLiteral("one"));
        adapter->title(window.first, QStringLiteral("One"));
        adapter->pid(window.first, 100 + window.first);
        adapter->kind(window.first, ToplevelKind::XdgToplevel);
        adapter->state(window.first,
                       window.first == 2
                           ? ToplevelStates{ToplevelStateFlag::Minimized}
                           : ToplevelStates{});
        adapter->focus(window.first, window.second);
        adapter->handleDone(window.first, 1);
    }
    adapter->managerDone(1, 3);
    QVERIFY(controller.runtimeKnown());

    controller.launch(0);
    QCOMPARE(launcher.requests.size(), 0);
    QCOMPARE(adapter->actionRequests.size(), 1);
    QCOMPARE(adapter->actionRequests.first().handleToken, quint64(2));
    QCOMPARE(adapter->actionRequests.first().action, ToplevelAction::Activate);
    adapter->completeAction(ToplevelActionResult::Accepted);

    controller.launch(0);
    QCOMPARE(launcher.requests.size(), 0);
    QCOMPARE(adapter->actionRequests.size(), 1);
    QCOMPARE(adapter->actionRequests.first().handleToken, quint64(2));
    adapter->completeAction(ToplevelActionResult::NoChange);
    QCOMPARE(launcher.requests.size(), 0);
}

void DockTyphonRuntimeIntegrationTest::unavailableActivationNeverLaunchesSameClick()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.setCatalogSnapshot(makeCatalog());
    DockConfig config = DockConfig::defaults();
    config.pins = {QStringLiteral("one.desktop")};
    controller.applyConfig(config);
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    adapter->create(1);
    adapter->id(1, QStringLiteral("1"));
    adapter->app(1, QStringLiteral("one"));
    adapter->title(1, QStringLiteral("One"));
    adapter->pid(1, 101);
    adapter->kind(1, ToplevelKind::XdgToplevel);
    adapter->state(1, {});
    adapter->focus(1, 1);
    adapter->handleDone(1, 1);
    adapter->managerDone(1, 1);

    controller.launch(0);
    QCOMPARE(launcher.requests.size(), 0);
    adapter->completeAction(ToplevelActionResult::Unavailable);
    QCOMPARE(launcher.requests.size(), 0);
}

void DockTyphonRuntimeIntegrationTest::staleExactTargetNeverRetargetsOrLaunches()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.setCatalogSnapshot(makeCatalog());
    DockConfig config = DockConfig::defaults();
    config.pins = {QStringLiteral("one.desktop")};
    controller.applyConfig(config);
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    adapter->create(1);
    adapter->id(1, QStringLiteral("1"));
    adapter->app(1, QStringLiteral("one"));
    adapter->title(1, QStringLiteral("One"));
    adapter->pid(1, 101);
    adapter->kind(1, ToplevelKind::XdgToplevel);
    adapter->state(1, {});
    adapter->focus(1, 1);
    adapter->handleDone(1, 1);
    adapter->managerDone(1, 1);

    QVERIFY(controller.runtimeKnown());
    QVERIFY(controller.appModel()->index(0, 0).data(DockAppModel::RunningRole).toBool());

    // The Dock has observed the live target, then the exact protocol handle disappears
    // before requestAction. No alternate target or launch fallback is allowed.
    adapter->close(1);
    controller.launch(0);

    QCOMPARE(adapter->actionRequests.size(), 0);
    QCOMPARE(launcher.requests.size(), 0);
    QCOMPARE(controller.launchingCount(), 0);
}

void DockTyphonRuntimeIntegrationTest::nonPinnedMinimizedApplicationAppearsActivatesAndCloses()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.setCatalogSnapshot(makeCatalog());
    DockConfig config = DockConfig::defaults();
    config.pins = {QStringLiteral("one.desktop")};
    controller.applyConfig(config);
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    adapter->create(2);
    adapter->id(2, QStringLiteral("2"));
    adapter->app(2, QStringLiteral("two"));
    adapter->title(2, QStringLiteral("Two"));
    adapter->pid(2, 202);
    adapter->kind(2, ToplevelKind::XdgToplevel);
    adapter->state(2, ToplevelStates{ToplevelStateFlag::Minimized});
    adapter->focus(2, 7);
    adapter->handleDone(2, 1);
    adapter->managerDone(1, 1);

    QCOMPARE(controller.appModel()->rowCount(), 2);
    const QModelIndex dynamic = controller.appModel()->index(1, 0);
    QCOMPARE(dynamic.data(DockAppModel::DesktopFileNameRole).toString(),
             QStringLiteral("two.desktop"));
    QVERIFY(!dynamic.data(DockAppModel::PinnedRole).toBool());
    QVERIFY(dynamic.data(DockAppModel::RunningRole).toBool());
    QCOMPARE(dynamic.data(DockAppModel::WindowCountRole).toInt(), 1);

    controller.launch(1);
    QCOMPARE(launcher.requests.size(), 0);
    QCOMPARE(adapter->actionRequests.size(), 1);
    QCOMPARE(adapter->actionRequests.first().handleToken, quint64(2));
    QCOMPARE(adapter->actionRequests.first().action, ToplevelAction::Activate);
    adapter->completeAction(ToplevelActionResult::Accepted);

    adapter->close(2);
    adapter->managerDone(2, 0);
    QTRY_COMPARE_WITH_TIMEOUT(controller.appModel()->rowCount(), 1, 1000);
    QCOMPARE(controller.appModel()->desktopFileNameAt(0), QStringLiteral("one.desktop"));
}

void DockTyphonRuntimeIntegrationTest::runtimeOnlyTaskActivatesAndClosesExactWindow()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    adapter->create(7);
    adapter->id(7, QStringLiteral("7"));
    adapter->app(7, QStringLiteral("steam_app_1091500"));
    adapter->title(7, QStringLiteral("Cyberpunk 2077"));
    adapter->pid(7, 700);
    adapter->kind(7, ToplevelKind::XdgToplevel);
    adapter->state(7, ToplevelStates{ToplevelStateFlag::Minimized});
    adapter->focus(7, 42);
    adapter->handleDone(7, 1);
    adapter->managerDone(1, 1);

    const QString taskKey = QStringLiteral("app:steam_app_1091500");
    QCOMPARE(controller.appModel()->rowForTaskKey(taskKey), 0);
    QVERIFY(controller.activateWindow(taskKey, QStringLiteral("7")));
    QCOMPARE(adapter->actionRequests.size(), 1);
    QCOMPARE(adapter->actionRequests.last().handleToken, quint64(7));
    QCOMPARE(adapter->actionRequests.last().action, ToplevelAction::Activate);
    adapter->completeAction(ToplevelActionResult::Accepted);

    QVERIFY(controller.closeWindow(taskKey, QStringLiteral("7")));
    QCOMPARE(adapter->actionRequests.size(), 1);
    QCOMPARE(adapter->actionRequests.last().handleToken, quint64(7));
    QCOMPARE(adapter->actionRequests.last().action, ToplevelAction::Close);
    QCOMPARE(launcher.requests.size(), 0);
}

void DockTyphonRuntimeIntegrationTest::runtimeOnlyTaskAnchorTargetsAllWindows()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    for (const auto &window : QVector<QPair<quint64, quint64>>{{8, 10}, {9, 20}}) {
        adapter->create(window.first);
        adapter->id(window.first, QString::number(window.first));
        adapter->app(window.first, QStringLiteral("steam_app_1091500"));
        adapter->title(window.first, QStringLiteral("Cyberpunk 2077"));
        adapter->pid(window.first, static_cast<quint32>(window.first * 10));
        adapter->kind(window.first, ToplevelKind::XdgToplevel);
        adapter->state(window.first, ToplevelStates{});
        adapter->focus(window.first, window.second);
        adapter->handleDone(window.first, 1);
    }
    adapter->managerDone(1, 2);

    const QString taskKey = QStringLiteral("app:steam_app_1091500");
    const QRect anchor(11, 12, 48, 48);
    QVERIFY(controller.setMinimizeAnchor(taskKey, anchor));
    QCOMPARE(adapter->anchorRequests.size(), 2);
    QCOMPARE(adapter->anchorRequests.at(0).rect, anchor);
    QCOMPARE(adapter->anchorRequests.at(1).rect, anchor);

    controller.clearMinimizeAnchor(taskKey);
    QCOMPARE(adapter->clearedAnchorHandles.size(), 2);
}

void DockTyphonRuntimeIntegrationTest::actionReadyReplaysCachedAnchorWithoutAnotherSnapshot()
{
    auto *adapter = new FakeTyphonAdapter;
    adapter->capability = TyphonActionCapabilityState::AuthenticatingV2;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    adapter->create(17);
    adapter->id(17, QStringLiteral("17"));
    adapter->app(17, QStringLiteral("steam_app_1091500"));
    adapter->title(17, QStringLiteral("Cyberpunk 2077"));
    adapter->pid(17, 1700);
    adapter->kind(17, ToplevelKind::XdgToplevel);
    adapter->state(17, ToplevelStates{});
    adapter->focus(17, 42);
    adapter->handleDone(17, 1);
    adapter->managerDone(1, 1);

    const QString taskKey = QStringLiteral("app:steam_app_1091500");
    const QRect anchor(31, 47, 48, 48);
    QVERIFY(controller.setMinimizeAnchor(taskKey, anchor));
    QCOMPARE(adapter->anchorRequests.size(), 0);

    QSignalSpy snapshots(&connection, &TyphonToplevelConnection::snapshotChanged);
    adapter->advertiseActionCapability(TyphonActionCapabilityState::ActionReadyV2);

    QCOMPARE(snapshots.count(), 0);
    QCOMPARE(adapter->anchorRequests.size(), 1);
    QCOMPARE(adapter->anchorRequests.first().handleToken, quint64(17));
    QCOMPARE(adapter->anchorRequests.first().rect, anchor);
}

void DockTyphonRuntimeIntegrationTest::clearRequestedDuringAuthenticationRetriesWhenReady()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    adapter->create(19);
    adapter->id(19, QStringLiteral("19"));
    adapter->app(19, QStringLiteral("one"));
    adapter->title(19, QStringLiteral("One"));
    adapter->pid(19, 1900);
    adapter->kind(19, ToplevelKind::XdgToplevel);
    adapter->state(19, ToplevelStates{});
    adapter->focus(19, 43);
    adapter->handleDone(19, 1);
    adapter->managerDone(1, 1);

    const QString taskKey = QStringLiteral("app:one");
    QVERIFY(controller.setMinimizeAnchor(taskKey, QRect(3, 5, 48, 48)));
    QCOMPARE(adapter->anchorRequests.size(), 1);

    adapter->advertiseActionCapability(TyphonActionCapabilityState::AuthenticatingV2);
    controller.clearMinimizeAnchor(taskKey);
    QCOMPARE(adapter->clearedAnchorHandles.size(), 0);

    adapter->advertiseActionCapability(TyphonActionCapabilityState::ActionReadyV2);
    QCOMPARE(adapter->clearedAnchorHandles.size(), 1);
    QCOMPARE(adapter->clearedAnchorHandles.first(), quint64(19));
}

void DockTyphonRuntimeIntegrationTest::anchorFailureRetriesOnNextProjectionWithoutDuplicateQueuedSends()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    adapter->create(23);
    adapter->id(23, QStringLiteral("23"));
    adapter->app(23, QStringLiteral("one"));
    adapter->title(23, QStringLiteral("One"));
    adapter->pid(23, 2300);
    adapter->kind(23, ToplevelKind::XdgToplevel);
    adapter->state(23, ToplevelStates{});
    adapter->focus(23, 50);
    adapter->handleDone(23, 1);
    adapter->managerDone(1, 1);

    const QString taskKey = QStringLiteral("app:one");
    const QRect anchor(10, 20, 48, 48);
    adapter->anchorError = ToplevelActionError::LocalCapacityExceeded;
    QVERIFY(controller.setMinimizeAnchor(taskKey, anchor));
    QCOMPARE(adapter->anchorRequests.size(), 1);

    adapter->anchorError.reset();
    adapter->managerDone(2, 1);
    QCOMPARE(adapter->anchorRequests.size(), 2);
    QCOMPARE(adapter->anchorRequests.last().rect, anchor);

    QVERIFY(controller.setMinimizeAnchor(taskKey, anchor));
    QCOMPARE(adapter->anchorRequests.size(), 2);
}

void DockTyphonRuntimeIntegrationTest::newConnectionGenerationReplaysCachedAnchor()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    adapter->create(29);
    adapter->id(29, QStringLiteral("29"));
    adapter->app(29, QStringLiteral("steam_app_1091500"));
    adapter->title(29, QStringLiteral("Cyberpunk 2077"));
    adapter->pid(29, 2900);
    adapter->kind(29, ToplevelKind::XdgToplevel);
    adapter->state(29, ToplevelStates{});
    adapter->focus(29, 60);
    adapter->handleDone(29, 1);
    adapter->managerDone(1, 1);

    const QString taskKey = QStringLiteral("app:steam_app_1091500");
    const QRect anchor(80, 90, 48, 48);
    QVERIFY(controller.setMinimizeAnchor(taskKey, anchor));
    QCOMPARE(adapter->anchorRequests.size(), 1);

    const quint64 firstGeneration = connection.connectionGeneration();
    connection.stop();
    connection.start();
    adapter->advertiseManager();
    adapter->create(29);
    adapter->id(29, QStringLiteral("29"));
    adapter->app(29, QStringLiteral("steam_app_1091500"));
    adapter->title(29, QStringLiteral("Cyberpunk 2077"));
    adapter->pid(29, 2900);
    adapter->kind(29, ToplevelKind::XdgToplevel);
    adapter->state(29, ToplevelStates{});
    adapter->focus(29, 60);
    adapter->handleDone(29, 1);
    adapter->managerDone(1, 1);

    QCOMPARE(adapter->anchorRequests.size(), 2);
    QVERIFY(connection.connectionGeneration() != firstGeneration);
    QCOMPARE(adapter->anchorRequests.last().handleToken, quint64(29));
    QCOMPARE(adapter->anchorRequests.last().rect, anchor);
}

void DockTyphonRuntimeIntegrationTest::removedTaskAnchorDoesNotLeakToAnotherApplication()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    adapter->create(37);
    adapter->id(37, QStringLiteral("37"));
    adapter->app(37, QStringLiteral("steam_app_1091500"));
    adapter->title(37, QStringLiteral("Cyberpunk 2077"));
    adapter->pid(37, 3700);
    adapter->kind(37, ToplevelKind::XdgToplevel);
    adapter->state(37, ToplevelStates{});
    adapter->focus(37, 70);
    adapter->handleDone(37, 1);
    adapter->managerDone(1, 1);

    QVERIFY(controller.setMinimizeAnchor(QStringLiteral("app:steam_app_1091500"),
                                        QRect(100, 110, 48, 48)));
    QCOMPARE(adapter->anchorRequests.size(), 1);

    connection.stop();
    connection.start();
    adapter->advertiseManager();
    adapter->create(38);
    adapter->id(38, QStringLiteral("38"));
    adapter->app(38, QStringLiteral("new-application"));
    adapter->title(38, QStringLiteral("New application"));
    adapter->pid(38, 3800);
    adapter->kind(38, ToplevelKind::XdgToplevel);
    adapter->state(38, ToplevelStates{});
    adapter->focus(38, 71);
    adapter->handleDone(38, 1);
    adapter->managerDone(1, 1);

    QCOMPARE(adapter->anchorRequests.size(), 1);
    QCOMPARE(adapter->clearedAnchorHandles.size(), 0);
}

void DockTyphonRuntimeIntegrationTest::recreatedHandleReceivesCachedAnchorForExactWindow()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    adapter->create(47);
    adapter->id(47, QStringLiteral("47"));
    adapter->app(47, QStringLiteral("recreated-app"));
    adapter->title(47, QStringLiteral("Window"));
    adapter->pid(47, 4700);
    adapter->kind(47, ToplevelKind::XdgToplevel);
    adapter->state(47, ToplevelStates{});
    adapter->focus(47, 80);
    adapter->handleDone(47, 1);
    adapter->managerDone(1, 1);

    const QString taskKey = QStringLiteral("app:recreated-app");
    const QRect anchor(121, 131, 48, 48);
    QVERIFY(controller.setMinimizeAnchor(taskKey, anchor));
    QCOMPARE(adapter->anchorRequests.size(), 1);
    QCOMPARE(adapter->anchorRequests.last().handleToken, quint64(47));

    adapter->close(47);
    adapter->managerDone(2, 0);
    adapter->create(48);
    adapter->id(48, QStringLiteral("47"));
    adapter->app(48, QStringLiteral("recreated-app"));
    adapter->title(48, QStringLiteral("Window"));
    adapter->pid(48, 4700);
    adapter->kind(48, ToplevelKind::XdgToplevel);
    adapter->state(48, ToplevelStates{});
    adapter->focus(48, 81);
    adapter->handleDone(48, 3);
    adapter->managerDone(3, 1);

    QCOMPARE(adapter->anchorRequests.size(), 2);
    QCOMPARE(adapter->anchorRequests.last().handleToken, quint64(48));
    QCOMPARE(adapter->anchorRequests.last().rect, anchor);
}

void DockTyphonRuntimeIntegrationTest::sameWindowIdTaskMigrationClearsThePreviousApplicationsAnchor()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    adapter->create(59);
    adapter->id(59, QStringLiteral("59"));
    adapter->app(59, QStringLiteral("old-application"));
    adapter->title(59, QStringLiteral("Old application"));
    adapter->pid(59, 5900);
    adapter->kind(59, ToplevelKind::XdgToplevel);
    adapter->state(59, ToplevelStates{});
    adapter->focus(59, 100);
    adapter->handleDone(59, 1);
    adapter->managerDone(1, 1);

    QVERIFY(controller.setMinimizeAnchor(QStringLiteral("app:old-application"),
                                        QRect(41, 51, 48, 48)));
    QCOMPARE(adapter->anchorRequests.size(), 1);

    connection.stop();
    connection.start();
    adapter->advertiseManager();
    adapter->create(59);
    adapter->id(59, QStringLiteral("59"));
    adapter->app(59, QStringLiteral("new-application"));
    adapter->title(59, QStringLiteral("New application"));
    adapter->pid(59, 5900);
    adapter->kind(59, ToplevelKind::XdgToplevel);
    adapter->state(59, ToplevelStates{});
    adapter->focus(59, 101);
    adapter->handleDone(59, 1);
    adapter->managerDone(1, 1);

    QCOMPARE(adapter->anchorRequests.size(), 1);
    QCOMPARE(adapter->clearedAnchorHandles.size(), 1);
    QCOMPARE(adapter->clearedAnchorHandles.first(), quint64(59));
}

void DockTyphonRuntimeIntegrationTest::unsupportedProtocolKeepsAnchorLocalAndDegraded()
{
    auto *adapter = new FakeTyphonAdapter;
    adapter->protocolVersion = 2;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    adapter->create(53);
    adapter->id(53, QStringLiteral("53"));
    adapter->app(53, QStringLiteral("legacy-app"));
    adapter->title(53, QStringLiteral("Legacy"));
    adapter->pid(53, 5300);
    adapter->kind(53, ToplevelKind::XdgToplevel);
    adapter->state(53, ToplevelStates{});
    adapter->focus(53, 90);
    adapter->handleDone(53, 1);
    adapter->managerDone(1, 1);

    QVERIFY(controller.setMinimizeAnchor(QStringLiteral("app:legacy-app"),
                                        QRect(11, 12, 48, 48)));
    QCOMPARE(adapter->anchorRequests.size(), 0);
    QCOMPARE(adapter->clearedAnchorHandles.size(), 0);
}

void DockTyphonRuntimeIntegrationTest::connectionLossRemovesDynamicRowsButKeepsPinsUnknown()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    DockController controller;
    controller.setCatalogSnapshot(makeCatalog());
    DockConfig config = DockConfig::defaults();
    config.pins = {QStringLiteral("one.desktop")};
    controller.applyConfig(config);
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    for (const auto &window : QVector<quint64>{1, 2}) {
        adapter->create(window);
        adapter->id(window, QString::number(window));
        adapter->app(window, window == 1 ? QStringLiteral("one") : QStringLiteral("two"));
        adapter->title(window, window == 1 ? QStringLiteral("One") : QStringLiteral("Two"));
        adapter->pid(window, 100 + window);
        adapter->kind(window, ToplevelKind::XdgToplevel);
        adapter->state(window, {});
        adapter->focus(window, window);
        adapter->handleDone(window, 1);
    }
    adapter->managerDone(1, 2);

    QCOMPARE(controller.appModel()->rowCount(), 2);
    adapter->disconnectDisplay();

    QCOMPARE(controller.appModel()->rowCount(), 1);
    QCOMPARE(controller.appModel()->desktopFileNameAt(0), QStringLiteral("one.desktop"));
    QVERIFY(!controller.appModel()->index(0, 0).data(DockAppModel::RuntimeKnownRole).toBool());
}

void DockTyphonRuntimeIntegrationTest::catalogRebuildKeepsExactActionsAndMinimizeAnchorOnStableTask()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    TyphonDockRuntimeBackend runtimeBackend(&connection);
    DockController controller;
    controller.setCatalogSnapshot(std::make_shared<DesktopEntrySnapshot>());
    controller.attachRuntimeBackend(&runtimeBackend);

    connection.start();
    adapter->advertiseManager();
    adapter->create(7);
    adapter->id(7, QStringLiteral("7"));
    adapter->app(7, QStringLiteral("late-app"));
    adapter->title(7, QStringLiteral("Late"));
    adapter->pid(7, 700);
    adapter->kind(7, ToplevelKind::XdgToplevel);
    adapter->state(7, ToplevelStates{});
    adapter->focus(7, 42);
    adapter->handleDone(7, 1);
    adapter->managerDone(1, 1);

    const QString oldKey = QStringLiteral("app:late-app");
    const QRect anchor(11, 12, 48, 48);
    QVERIFY(controller.setMinimizeAnchor(oldKey, anchor));
    QCOMPARE(adapter->anchorRequests.size(), 1);
    controller.setCatalogSnapshot([&] {
        auto snapshot = std::make_shared<DesktopEntrySnapshot>();
        DesktopEntryRecord record;
        record.desktopFileName = QStringLiteral("late.desktop");
        record.id = QStringLiteral("late-app");
        record.name = QStringLiteral("Late App");
        const int index = snapshot->entries.size();
        snapshot->entries.append(record);
        snapshot->byDesktopFileName.insert(record.desktopFileName, index);
        snapshot->byDesktopId.insert(record.id, index);
        return snapshot;
    }());

    QCOMPARE(controller.appModel()->rowCount(), 1);
    QCOMPARE(controller.appModel()->taskKeyAt(0), oldKey);
    QVERIFY(controller.windowsForTaskKey(oldKey).size() == 1);
    QVERIFY(controller.activateWindow(oldKey, QStringLiteral("7")));
    QVERIFY(controller.closeWindow(oldKey, QStringLiteral("7")));
    QVERIFY(!controller.activateWindow(QStringLiteral("app:other"), QStringLiteral("7")));
    QVERIFY(!controller.closeWindow(QStringLiteral("app:other"), QStringLiteral("7")));
    QCOMPARE(adapter->anchorRequests.last().handleToken, quint64(7));
}

QTEST_GUILESS_MAIN(DockTyphonRuntimeIntegrationTest)
#include "DockTyphonRuntimeIntegrationTest.moc"
