#include <QTest>

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
    return snapshot;
}

class DockTyphonRuntimeIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void authoritativeSnapshotDrivesDockRuntimeRoles();
    void runningApplicationActivatesMostRecentExactWindow();
    void unavailableActivationNeverLaunchesSameClick();
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

QTEST_GUILESS_MAIN(DockTyphonRuntimeIntegrationTest)
#include "DockTyphonRuntimeIntegrationTest.moc"
