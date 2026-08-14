#include <QTest>
#include <QSet>

#include "core/DockController.hpp"
#include "platform/typhon/TyphonToplevelConnection.hpp"

using namespace Astrea::Typhon;

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
    std::optional<ToplevelActionError> requestAction(
        quint64 handleToken, TyphonActionToken token, ToplevelAction action) override
    {
        if (staleHandleTokens.contains(handleToken))
            return ToplevelActionError::ToplevelNotLive;
        actionRequests.append({handleToken, token, action});
        return requestError;
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
    void completeAction(ToplevelActionResult result)
    {
        const auto request = actionRequests.takeLast();
        emit actionCompleted(request.token.hi, request.token.lo, request.action, result);
    }

    int starts = 0;
    int stops = 0;
    TyphonActionCapabilityState capability = TyphonActionCapabilityState::ActionReadyV2;
    std::optional<ToplevelActionError> requestError;
    struct ActionRequest {
        quint64 handleToken = 0;
        TyphonActionToken token;
        ToplevelAction action = ToplevelAction::Activate;
    };
    QVector<ActionRequest> actionRequests;
    QSet<quint64> staleHandleTokens;
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
    void runningApplicationActivatesMostRecentExactWindow();
    void unavailableActivationNeverLaunchesSameClick();
    void staleExactTargetNeverRetargetsOrLaunches();
    void nonPinnedMinimizedApplicationAppearsActivatesAndCloses();
    void connectionLossRemovesDynamicRowsButKeepsPinsUnknown();
};

void DockTyphonRuntimeIntegrationTest::authoritativeSnapshotDrivesDockRuntimeRoles()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    DockController controller;
    controller.setCatalogSnapshot(makeCatalog());
    DockConfig config = DockConfig::defaults();
    config.pins = {QStringLiteral("one.desktop")};
    controller.applyConfig(config);
    controller.attachTyphonConnection(&connection);

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

void DockTyphonRuntimeIntegrationTest::runningApplicationActivatesMostRecentExactWindow()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.setCatalogSnapshot(makeCatalog());
    DockConfig config = DockConfig::defaults();
    config.pins = {QStringLiteral("one.desktop")};
    controller.applyConfig(config);
    controller.attachTyphonConnection(&connection);

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
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.setCatalogSnapshot(makeCatalog());
    DockConfig config = DockConfig::defaults();
    config.pins = {QStringLiteral("one.desktop")};
    controller.applyConfig(config);
    controller.attachTyphonConnection(&connection);

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
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.setCatalogSnapshot(makeCatalog());
    DockConfig config = DockConfig::defaults();
    config.pins = {QStringLiteral("one.desktop")};
    controller.applyConfig(config);
    controller.attachTyphonConnection(&connection);

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
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.setCatalogSnapshot(makeCatalog());
    DockConfig config = DockConfig::defaults();
    config.pins = {QStringLiteral("one.desktop")};
    controller.applyConfig(config);
    controller.attachTyphonConnection(&connection);

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

void DockTyphonRuntimeIntegrationTest::connectionLossRemovesDynamicRowsButKeepsPinsUnknown()
{
    auto *adapter = new FakeTyphonAdapter;
    TyphonToplevelConnection connection(adapter);
    DockController controller;
    controller.setCatalogSnapshot(makeCatalog());
    DockConfig config = DockConfig::defaults();
    config.pins = {QStringLiteral("one.desktop")};
    controller.applyConfig(config);
    controller.attachTyphonConnection(&connection);

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

QTEST_GUILESS_MAIN(DockTyphonRuntimeIntegrationTest)
#include "DockTyphonRuntimeIntegrationTest.moc"
