#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>
#include <QPersistentModelIndex>

#include <optional>

#include "core/DockController.hpp"
#include "apps/appidentity/AppIdentityResolver.hpp"
#include "services/DockConfigPersistence.hpp"

class FakeDockRuntimeBackend final : public Astrea::Dock::DockRuntimeBackend {
    Q_OBJECT

public:
    struct ActionRequest {
        QString windowId;
        Astrea::Dock::DockRuntimeAction action = Astrea::Dock::DockRuntimeAction::Activate;
        quint64 token = 0;
    };

    explicit FakeDockRuntimeBackend(QObject *parent = nullptr)
        : DockRuntimeBackend(parent)
    {
    }

    bool runtimeReady() const override { return m_runtimeReady; }
    bool actionReady() const override { return m_actionReady; }
    Astrea::Dock::DockRuntimeSnapshot snapshot() const override { return m_snapshot; }
    quint64 generation() const override { return m_snapshot.generation; }
    std::optional<quint64> handleTokenForWindowId(const QString &windowId) const override
    {
        for (const auto &window : m_snapshot.windows) {
            if (window.id == windowId)
                return static_cast<quint64>(qHash(windowId));
        }
        return std::nullopt;
    }

    std::optional<Astrea::Dock::DockRuntimeActionError> requestAction(
        const QString &windowId, Astrea::Dock::DockRuntimeAction action,
        quint64 consumerToken) override
    {
        if (nextActionError) {
            const auto error = nextActionError;
            nextActionError.reset();
            return error;
        }
        if (!m_actionReady)
            return Astrea::Dock::DockRuntimeActionError::NotActionReady;
        actionRequests.append({windowId, action, consumerToken});
        return std::nullopt;
    }

    std::optional<Astrea::Dock::DockRuntimeActionError> setMinimizeAnchor(
        const QString &windowId, const QRect &rect) override
    {
        if (!m_actionReady)
            return Astrea::Dock::DockRuntimeActionError::NotActionReady;
        publishedAnchors.insert(windowId, rect);
        return std::nullopt;
    }

    std::optional<Astrea::Dock::DockRuntimeActionError> clearMinimizeAnchor(
        const QString &windowId) override
    {
        if (!m_actionReady)
            return Astrea::Dock::DockRuntimeActionError::NotActionReady;
        publishedAnchors.remove(windowId);
        return std::nullopt;
    }

    QString diagnosticContext() const override
    {
        return QStringLiteral("connection_state=ready capability=action_ready_v2 generation=%1")
            .arg(generation());
    }

    void publishSnapshot(Astrea::Dock::DockRuntimeSnapshot snapshot)
    {
        const bool wasReady = m_runtimeReady;
        snapshot.authoritative = true;
        m_snapshot = std::move(snapshot);
        m_runtimeReady = true;
        if (!wasReady)
            emit runtimeReadyChanged(true);
        emit snapshotChanged(m_snapshot);
    }

    void clearRuntime()
    {
        const bool wasReady = m_runtimeReady;
        m_runtimeReady = false;
        m_snapshot = {};
        if (wasReady)
            emit runtimeReadyChanged(false);
        emit snapshotChanged(m_snapshot);
    }

    void setActionReady(bool ready)
    {
        if (m_actionReady == ready)
            return;
        m_actionReady = ready;
        emit actionReadyChanged(ready);
    }

    void finishAction(quint64 token, Astrea::Dock::DockRuntimeAction action,
                      Astrea::Dock::DockRuntimeActionResult result)
    {
        emit actionFinished(token, action, result);
    }

    void failAction(quint64 token, Astrea::Dock::DockRuntimeAction action,
                    Astrea::Dock::DockRuntimeActionError error)
    {
        emit actionFailed(token, action, error);
    }

    std::optional<Astrea::Dock::DockRuntimeActionError> nextActionError;
    QVector<ActionRequest> actionRequests;
    QHash<QString, QRect> publishedAnchors;

private:
    Astrea::Dock::DockRuntimeSnapshot m_snapshot;
    bool m_runtimeReady = false;
    bool m_actionReady = false;
};

class FakeLauncher final : public ApplicationLauncher {
    Q_OBJECT

public:
    FakeLauncher() : ApplicationLauncher(QStringLiteral("fake-launcher")) {}

    void launchDesktop(const ApplicationLaunchRequest &request) override
    {
        requests.append(request);
        emit launchAccepted(request.desktopId.isEmpty() ? request.desktopFileName : request.desktopId);
    }

    void succeed(const QString &desktopId) { emit launchSucceeded(desktopId); }
    void fail(const QString &desktopId, const QString &error) { emit launchFailed(desktopId, error); }
    void timeout(const QString &desktopId) { emit launchTimedOut(desktopId); }

    QVector<ApplicationLaunchRequest> requests;
};

class DockControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void configApplicationAndComponentToggle();
    void launchRequestRoutesToSharedLauncher();
    void sameItemIsSuppressedWhilePending();
    void unrelatedItemsLaunchIndependently();
    void successFailureAndTimeoutClearState();
    void unresolvedPinUsesFullDesktopFilename();
    void completionUsesFullFilenameForPendingRows();
    void unresolvedPinResolvesAfterCatalogRebuild();
    void invalidIndexIsIgnored();
    void catalogRebuildKeepsModelRowsStable();
    void runningRuntimeSuppressesDuplicateLaunch();
    void runtimeWindowsKeepExactIdentityAndTitles();
    void runtimeBackendActionsAndReadinessRemainSynchronous();
    void newWindowLaunchBypassesRunningActivation();
    void pinMutationRequiresSuccessfulPersistence();
    void pinCountersExcludeRuntimeOnlyRows();
    void movingFirstPinToEndPersistsAndPreservesState();
    void movingRuntimePinnedItemsPreservesRuntimeState();
    void movingLastPinToBeginningPersists();
    void movingToSameIndexIsNoOp();
    void invalidOrRuntimeOnlySourceIsRejected();
    void persistenceFailureLeavesOrderUnchanged();
    void runtimeOnlyOrderingRemainsUnchangedAfterPinMove();
    void runtimeOnlyTaskUsesTaskKeyAndRejectsLauncherActions();
    void runtimeIdentityEnrichmentKeepsTaskKeyStable();
    void staleIdentityEnrichmentIsRejectedAtControllerBoundary();
    void catalogGainWhileLiveKeepsStableTaskKeyAndAvoidsRowChurn();
    void catalogLossWhileLiveKeepsStableTaskKey();
    void catalogLossWhilePinnedKeepsOneStableLiveRow();
    void runtimeMetadataChangeKeepsStableTaskKey();
    void newSameAppWindowJoinsStableTaskAfterCatalogGain();
    void lateResolvedRuntimeCanBePinnedWithoutDuplicate();
    void newLifecycleCanUseDesktopKeyAfterOldTaskEnds();
    void desktopFilenameLookupFindsStableRuntimeTask();
    void personalizationPropertiesPropagateAndUnchangedConfigIsQuiet();
    void autoHidePolicyKeepsSurfaceMappedAndReservationBounded();
    void runtimeObstructionUpdatesSurfacePlacement();
    void surfacePlacementTracksRuntimePositionTransitions();
};

class CountingPersistence final : public DockConfigPersistence {
public:
    CountingPersistence() : DockConfigPersistence(QStringLiteral("/unused/dock.json")) {}

    bool writePins(const QStringList &pins, QString *errorOut = nullptr) override
    {
        ++calls;
        lastPins = pins;
        if (!succeed && errorOut)
            *errorOut = QStringLiteral("persistence failed");
        return succeed;
    }

    int calls = 0;
    bool succeed = true;
    QStringList lastPins;
};

static DockConfig configWithPins(const QStringList &pins)
{
    DockConfig config = DockConfig::defaults();
    config.pins = pins;
    return config;
}

static std::shared_ptr<DesktopEntrySnapshot> makeCatalog()
{
    auto snapshot = std::make_shared<DesktopEntrySnapshot>();
    for (const QString &fileName : {QStringLiteral("one.desktop"), QStringLiteral("two.desktop")}) {
        DesktopEntryRecord record;
        record.desktopFileName = fileName;
        record.id = fileName.chopped(8);
        record.name = record.id.toUpper();
        record.icon = record.id;
        record.exec = record.id;
        record.sourceFilePath = QStringLiteral("/tmp/") + fileName;
        const int index = snapshot->entries.size();
        snapshot->entries.append(record);
        snapshot->byDesktopFileName.insert(fileName, index);
        snapshot->byDesktopId.insert(record.id, index);
    }
    return snapshot;
}

static std::shared_ptr<DesktopEntrySnapshot> makeLateCatalog()
{
    auto snapshot = makeCatalog();
    DesktopEntryRecord record;
    record.desktopFileName = QStringLiteral("late.desktop");
    record.id = QStringLiteral("late-app");
    record.name = QStringLiteral("Late App");
    record.icon = QStringLiteral("late-icon");
    record.exec = QStringLiteral("late-app");
    const int index = snapshot->entries.size();
    snapshot->entries.append(record);
    snapshot->byDesktopFileName.insert(record.desktopFileName, index);
    snapshot->byDesktopId.insert(record.id, index);
    return snapshot;
}

static Astrea::Dock::DockRuntimeSnapshot runtimeSnapshot(
    quint64 generation, std::initializer_list<Astrea::Dock::DockRuntimeWindow> windows)
{
    Astrea::Dock::DockRuntimeSnapshot snapshot;
    snapshot.authoritative = true;
    snapshot.generation = generation;
    snapshot.windows = QVector<Astrea::Dock::DockRuntimeWindow>(windows);
    snapshot.total = snapshot.windows.size();
    return snapshot;
}

static void writeDesktopEntry(const QString &path, const QString &name)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("[Desktop Entry]\nType=Application\nName=" + name.toUtf8()
               + "\nIcon=test-icon\nExec=test-app\n");
}

void DockControllerTest::configApplicationAndComponentToggle()
{
    FakeLauncher launcher;
    DesktopEntryCatalog catalog;
    catalog.initialize(QStringLiteral("/path/that/does/not/exist"));
    DockController controller(&launcher, &catalog);
    controller.applyConfig(configWithPins({QStringLiteral("two.desktop"), QStringLiteral("one.desktop")}));

    QCOMPARE(controller.appModel()->rowCount(), 2);
    QCOMPARE(controller.appModel()->desktopFileNameAt(0), QStringLiteral("two.desktop"));
    QVERIFY(controller.visible());
    controller.setComponentEnabled(false);
    QVERIFY(!controller.enabled());
    QVERIFY(!controller.visible());
    controller.setComponentEnabled(true);
    QVERIFY(controller.enabled());
    QVERIFY(controller.visible());
}

void DockControllerTest::launchRequestRoutesToSharedLauncher()
{
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.appModel()->setCatalogSnapshot(makeCatalog());
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop")}));

    controller.launch(0);

    QCOMPARE(launcher.requests.size(), 1);
    QCOMPARE(launcher.requests.first().desktopId, QStringLiteral("one"));
    QCOMPARE(launcher.requests.first().desktopFileName, QStringLiteral("one.desktop"));
    QVERIFY(controller.appModel()->data(controller.appModel()->index(0, 0),
                                        DockAppModel::LaunchingRole).toBool());
}

void DockControllerTest::sameItemIsSuppressedWhilePending()
{
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.appModel()->setCatalogSnapshot(makeCatalog());
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop")}));
    controller.launch(0);
    controller.launch(0);
    QCOMPARE(launcher.requests.size(), 1);
}

void DockControllerTest::unrelatedItemsLaunchIndependently()
{
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.appModel()->setCatalogSnapshot(makeCatalog());
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop"), QStringLiteral("two.desktop")}));
    controller.launch(0);
    controller.launch(1);
    QCOMPARE(launcher.requests.size(), 2);
    QVERIFY(controller.appModel()->data(controller.appModel()->index(0, 0), DockAppModel::LaunchingRole).toBool());
    QVERIFY(controller.appModel()->data(controller.appModel()->index(1, 0), DockAppModel::LaunchingRole).toBool());
}

void DockControllerTest::successFailureAndTimeoutClearState()
{
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.appModel()->setCatalogSnapshot(makeCatalog());
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop"), QStringLiteral("two.desktop")}));

    controller.launch(0);
    launcher.succeed(QStringLiteral("one.desktop"));
    QVERIFY(!controller.appModel()->data(controller.appModel()->index(0, 0), DockAppModel::LaunchingRole).toBool());
    QVERIFY(controller.appModel()->data(controller.appModel()->index(0, 0), DockAppModel::LaunchErrorRole).toString().isEmpty());

    controller.launch(1);
    launcher.fail(QStringLiteral("two.desktop"), QStringLiteral("bad launch"));
    QCOMPARE(controller.appModel()->data(controller.appModel()->index(1, 0), DockAppModel::LaunchErrorRole).toString(),
             QStringLiteral("bad launch"));
    QVERIFY(!controller.appModel()->data(controller.appModel()->index(1, 0), DockAppModel::LaunchingRole).toBool());

    controller.launch(1);
    launcher.timeout(QStringLiteral("two.desktop"));
    QCOMPARE(controller.appModel()->data(controller.appModel()->index(1, 0), DockAppModel::LaunchErrorRole).toString(),
             QStringLiteral("Launch timed out"));
}

void DockControllerTest::unresolvedPinUsesFullDesktopFilename()
{
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.applyConfig(configWithPins({QStringLiteral("missing.desktop")}));

    controller.launch(0);

    QCOMPARE(launcher.requests.size(), 1);
    QCOMPARE(launcher.requests.first().desktopFileName, QStringLiteral("missing.desktop"));
    QCOMPARE(launcher.requests.first().desktopId, QStringLiteral("missing"));
    QVERIFY(controller.appModel()->data(controller.appModel()->index(0, 0),
                                        DockAppModel::LaunchingRole).toBool());
    launcher.succeed(QStringLiteral("missing.desktop"));
    QVERIFY(!controller.appModel()->data(controller.appModel()->index(0, 0),
                                         DockAppModel::LaunchingRole).toBool());
}

void DockControllerTest::completionUsesFullFilenameForPendingRows()
{
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.appModel()->setCatalogSnapshot(makeCatalog());
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop"), QStringLiteral("two.desktop")}));

    controller.launch(0);
    controller.launch(1);
    launcher.succeed(QStringLiteral("two.desktop"));

    QVERIFY(controller.appModel()->data(controller.appModel()->index(0, 0),
                                        DockAppModel::LaunchingRole).toBool());
    QVERIFY(!controller.appModel()->data(controller.appModel()->index(1, 0),
                                         DockAppModel::LaunchingRole).toBool());
    launcher.fail(QStringLiteral("one.desktop"), QStringLiteral("one failed"));
    QVERIFY(!controller.appModel()->data(controller.appModel()->index(0, 0),
                                         DockAppModel::LaunchingRole).toBool());
    QCOMPARE(controller.appModel()->data(controller.appModel()->index(0, 0),
                                         DockAppModel::LaunchErrorRole).toString(),
             QStringLiteral("one failed"));
}

void DockControllerTest::unresolvedPinResolvesAfterCatalogRebuild()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());

    DockController controller(nullptr, &catalog);
    controller.applyConfig(configWithPins({QStringLiteral("late.desktop")}));
    QVERIFY(!controller.appModel()->data(controller.appModel()->index(0, 0),
                                         DockAppModel::ResolvedRole).toBool());

    const QString applications = home.path() + QStringLiteral("/.local/share/applications");
    QVERIFY(QDir().mkpath(applications));
    writeDesktopEntry(applications + QStringLiteral("/late.desktop"), QStringLiteral("Late"));

    QTRY_VERIFY_WITH_TIMEOUT(controller.appModel()->data(controller.appModel()->index(0, 0),
                                                        DockAppModel::ResolvedRole).toBool(),
                             2000);
    QCOMPARE(controller.appModel()->data(controller.appModel()->index(0, 0),
                                         DockAppModel::DisplayNameRole).toString(),
             QStringLiteral("Late"));
}

void DockControllerTest::invalidIndexIsIgnored()
{
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop")}));
    controller.launch(-1);
    controller.launch(1);
    QCOMPARE(launcher.requests.size(), 0);
}

void DockControllerTest::catalogRebuildKeepsModelRowsStable()
{
    FakeLauncher launcher;
    DockController controller(&launcher);
    auto first = makeCatalog();
    controller.appModel()->setCatalogSnapshot(first);
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop"), QStringLiteral("two.desktop")}));
    const QModelIndex firstIndex = controller.appModel()->index(0, 0);

    auto second = makeCatalog();
    second->entries[0].name = QStringLiteral("Changed");
    controller.appModel()->setCatalogSnapshot(second);

    QCOMPARE(controller.appModel()->index(0, 0).data(DockAppModel::DesktopFileNameRole).toString(),
             QStringLiteral("one.desktop"));
    QVERIFY(firstIndex.isValid());
    QCOMPARE(controller.appModel()->data(controller.appModel()->index(0, 0), DockAppModel::DisplayNameRole).toString(),
             QStringLiteral("Changed"));
}

void DockControllerTest::runningRuntimeSuppressesDuplicateLaunch()
{
    FakeLauncher launcher;
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller(&launcher);
    controller.attachRuntimeBackend(&runtimeBackend);
    controller.setCatalogSnapshot(makeCatalog());
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop")}));

    Astrea::Dock::DockRuntimeSnapshot snapshot;
    snapshot.generation = 1;
    snapshot.revision = 1;
    snapshot.total = 1;
    Astrea::Dock::DockRuntimeWindow window;
    window.id = QStringLiteral("window-1");
    window.appId = QStringLiteral("one");
    window.active = true;
    snapshot.windows.append(window);
    runtimeBackend.publishSnapshot(snapshot);

    const QModelIndex index = controller.appModel()->index(0, 0);
    QVERIFY(index.data(DockAppModel::RuntimeKnownRole).toBool());
    QVERIFY(index.data(DockAppModel::RunningRole).toBool());
    QVERIFY(index.data(DockAppModel::ActiveRole).toBool());
    QCOMPARE(index.data(DockAppModel::WindowCountRole).toInt(), 1);

    controller.launch(0);
    QCOMPARE(launcher.requests.size(), 0);

    runtimeBackend.clearRuntime();
    QVERIFY(!controller.appModel()->data(index, DockAppModel::RuntimeKnownRole).toBool());
    controller.launch(0);
    QCOMPARE(launcher.requests.size(), 1);
}

void DockControllerTest::runtimeWindowsKeepExactIdentityAndTitles()
{
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);
    controller.setCatalogSnapshot(makeCatalog());
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop")}));

    Astrea::Dock::DockRuntimeSnapshot snapshot;
    snapshot.generation = 2;
    snapshot.revision = 4;
    Astrea::Dock::DockRuntimeWindow first;
    first.id = QStringLiteral("window-a");
    first.appId = QStringLiteral("one");
    first.title = QStringLiteral("First title");
    Astrea::Dock::DockRuntimeWindow second;
    second.id = QStringLiteral("window-b");
    second.appId = QStringLiteral("one");
    second.title = QStringLiteral("Second title");
    snapshot.windows = {first, second};
    runtimeBackend.publishSnapshot(snapshot);

    const auto windows = controller.windowsForDesktopFileName(QStringLiteral("one.desktop"));
    QCOMPARE(windows.size(), 2);
    QCOMPARE(windows.at(0).id, QStringLiteral("window-a"));
    QCOMPARE(windows.at(0).title, QStringLiteral("First title"));
    QCOMPARE(windows.at(1).id, QStringLiteral("window-b"));
    QVERIFY(controller.windowsForDesktopFileName(QStringLiteral("two.desktop")).isEmpty());
}

void DockControllerTest::runtimeBackendActionsAndReadinessRemainSynchronous()
{
    FakeDockRuntimeBackend runtimeBackend;
    Astrea::Dock::DockRuntimeWindow window;
    window.id = QStringLiteral("exact-window");
    window.appId = QStringLiteral("one");
    window.title = QStringLiteral("Exact target");
    runtimeBackend.publishSnapshot(runtimeSnapshot(8, {window}));

    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);
    controller.setCatalogSnapshot(makeCatalog());
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop")}));
    QVERIFY(controller.runtimeKnown());
    QCOMPARE(controller.windowsForDesktopFileName(QStringLiteral("one.desktop")).size(), 1);

    const auto runtimeKey = controller.appModel()->runtimeTaskKeyForDesktopFileName(
        QStringLiteral("one.desktop"));
    QVERIFY(!runtimeKey.isEmpty());
    const QRect anchor(10, 20, 30, 40);
    QVERIFY(controller.setMinimizeAnchor(runtimeKey, anchor));
    QVERIFY(runtimeBackend.publishedAnchors.isEmpty());
    runtimeBackend.setActionReady(true);
    QCOMPARE(runtimeBackend.publishedAnchors.value(QStringLiteral("exact-window")), anchor);

    QVERIFY(controller.activateWindow(runtimeKey, QStringLiteral("exact-window")));
    QCOMPARE(runtimeBackend.actionRequests.size(), 1);
    QCOMPARE(runtimeBackend.actionRequests.last().windowId, QStringLiteral("exact-window"));
    QCOMPARE(runtimeBackend.actionRequests.last().action,
             Astrea::Dock::DockRuntimeAction::Activate);
    runtimeBackend.finishAction(runtimeBackend.actionRequests.last().token,
                                Astrea::Dock::DockRuntimeAction::Activate,
                                Astrea::Dock::DockRuntimeActionResult::Accepted);
    QVERIFY(controller.lastError().isEmpty());

    QVERIFY(controller.activateWindow(runtimeKey, QStringLiteral("exact-window")));
    const quint64 failedToken = runtimeBackend.actionRequests.last().token;
    runtimeBackend.failAction(failedToken, Astrea::Dock::DockRuntimeAction::Activate,
                              Astrea::Dock::DockRuntimeActionError::Disconnected);
    QCOMPARE(controller.lastError(), QStringLiteral("The selected window action failed"));

    runtimeBackend.nextActionError = Astrea::Dock::DockRuntimeActionError::NotActionReady;
    QVERIFY(!controller.closeWindow(runtimeKey, QStringLiteral("exact-window")));
    QCOMPARE(runtimeBackend.actionRequests.size(), 2);
    QVERIFY(!controller.lastError().isEmpty());
}

void DockControllerTest::newWindowLaunchBypassesRunningActivation()
{
    FakeLauncher launcher;
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller(&launcher);
    controller.attachRuntimeBackend(&runtimeBackend);
    controller.setCatalogSnapshot(makeCatalog());
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop")}));

    Astrea::Dock::DockRuntimeSnapshot snapshot;
    snapshot.generation = 1;
    snapshot.revision = 1;
    Astrea::Dock::DockRuntimeWindow window;
    window.id = QStringLiteral("window-1");
    window.appId = QStringLiteral("one");
    snapshot.windows.append(window);
    runtimeBackend.publishSnapshot(snapshot);

    QVERIFY(controller.launchNewWindow(QStringLiteral("one.desktop")));
    QCOMPARE(launcher.requests.size(), 1);
    QCOMPARE(launcher.requests.first().desktopFileName, QStringLiteral("one.desktop"));
}

void DockControllerTest::pinMutationRequiresSuccessfulPersistence()
{
    CountingPersistence persistence;
    DockController controller(nullptr, nullptr, &persistence);
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop")}));

    QVERIFY(controller.setPinned(QStringLiteral("two.desktop"), true));
    QCOMPARE(persistence.lastPins, QStringList({QStringLiteral("one.desktop"),
                                                 QStringLiteral("two.desktop")}));
    QVERIFY(controller.setPinned(QStringLiteral("one.desktop"), false));
    QCOMPARE(controller.pinCount(), 1);

    persistence.succeed = false;
    QVERIFY(!controller.setPinned(QStringLiteral("three.desktop"), true));
    QCOMPARE(controller.pinCount(), 1);
    QVERIFY(!controller.lastError().isEmpty());
}

void DockControllerTest::pinCountersExcludeRuntimeOnlyRows()
{
    FakeLauncher launcher;
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller(&launcher);
    controller.attachRuntimeBackend(&runtimeBackend);
    controller.setCatalogSnapshot(makeCatalog());
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop"),
                                           QStringLiteral("missing.desktop")}));

    Astrea::Dock::DockRuntimeSnapshot snapshot;
    snapshot.generation = 1;
    snapshot.revision = 1;
    Astrea::Dock::DockRuntimeWindow window;
    window.id = QStringLiteral("window-2");
    window.appId = QStringLiteral("two");
    snapshot.windows.append(window);
    runtimeBackend.publishSnapshot(snapshot);

    QCOMPARE(controller.pinCount(), 2);
    QCOMPARE(controller.appModel()->rowCount(), 3);
    QCOMPARE(controller.resolvedPinCount(), 1);
    QVERIFY(!controller.appModel()->index(2, 0).data(DockAppModel::PinnedRole).toBool());
}

void DockControllerTest::movingFirstPinToEndPersistsAndPreservesState()
{
    FakeLauncher launcher;
    CountingPersistence persistence;
    DockController controller(&launcher, nullptr, &persistence);
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop"),
                                           QStringLiteral("two.desktop"),
                                           QStringLiteral("three.desktop")}));
    QVERIFY(controller.appModel()->setLaunching(QStringLiteral("one.desktop"), true));
    const QPersistentModelIndex movedIndex = controller.appModel()->index(0, 0);

    QVERIFY(controller.movePinned(QStringLiteral("one.desktop"), 2));

    QCOMPARE(persistence.calls, 1);
    QCOMPARE(persistence.lastPins, QStringList({QStringLiteral("two.desktop"),
                                                 QStringLiteral("three.desktop"),
                                                 QStringLiteral("one.desktop")}));
    QCOMPARE(controller.appModel()->desktopFileNameAt(0), QStringLiteral("two.desktop"));
    QCOMPARE(controller.appModel()->desktopFileNameAt(2), QStringLiteral("one.desktop"));
    QVERIFY(controller.appModel()->index(2, 0).data(DockAppModel::LaunchingRole).toBool());
    QVERIFY(movedIndex.isValid());
    QCOMPARE(movedIndex.row(), 2);
    QCOMPARE(movedIndex.data(DockAppModel::DesktopFileNameRole).toString(),
             QStringLiteral("one.desktop"));

    controller.launchByDesktopFileName(QStringLiteral("one.desktop"));
    QCOMPARE(launcher.requests.size(), 1);
    QCOMPARE(launcher.requests.first().desktopFileName, QStringLiteral("one.desktop"));
}

void DockControllerTest::movingRuntimePinnedItemsPreservesRuntimeState()
{
    CountingPersistence persistence;
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller(nullptr, nullptr, &persistence);
    controller.attachRuntimeBackend(&runtimeBackend);
    controller.setCatalogSnapshot(makeCatalog());
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop"),
                                           QStringLiteral("two.desktop")}));

    Astrea::Dock::DockRuntimeSnapshot snapshot;
    snapshot.generation = 3;
    snapshot.revision = 2;
    Astrea::Dock::DockRuntimeWindow running;
    running.id = QStringLiteral("running-window");
    running.appId = QStringLiteral("one");
    running.active = true;
    Astrea::Dock::DockRuntimeWindow minimized;
    minimized.id = QStringLiteral("minimized-window");
    minimized.appId = QStringLiteral("two");
    minimized.minimized = true;
    snapshot.windows = {running, minimized};
    runtimeBackend.publishSnapshot(snapshot);

    QVERIFY(controller.movePinned(QStringLiteral("one.desktop"), 1));
    QCOMPARE(controller.appModel()->desktopFileNameAt(0), QStringLiteral("two.desktop"));
    QCOMPARE(controller.appModel()->desktopFileNameAt(1), QStringLiteral("one.desktop"));
    const QModelIndex minimizedIndex = controller.appModel()->index(0, 0);
    const QModelIndex runningIndex = controller.appModel()->index(1, 0);
    QVERIFY(minimizedIndex.data(DockAppModel::RunningRole).toBool());
    QVERIFY(!minimizedIndex.data(DockAppModel::ActiveRole).toBool());
    QCOMPARE(minimizedIndex.data(DockAppModel::WindowCountRole).toInt(), 1);
    QVERIFY(runningIndex.data(DockAppModel::RunningRole).toBool());
    QVERIFY(runningIndex.data(DockAppModel::ActiveRole).toBool());
    QCOMPARE(runningIndex.data(DockAppModel::WindowCountRole).toInt(), 1);
}

void DockControllerTest::movingLastPinToBeginningPersists()
{
    CountingPersistence persistence;
    DockController controller(nullptr, nullptr, &persistence);
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop"),
                                           QStringLiteral("two.desktop"),
                                           QStringLiteral("three.desktop")}));

    QVERIFY(controller.movePinned(QStringLiteral("three.desktop"), 0));

    QCOMPARE(persistence.calls, 1);
    QCOMPARE(controller.appModel()->desktopFileNameAt(0), QStringLiteral("three.desktop"));
    QCOMPARE(controller.appModel()->desktopFileNameAt(2), QStringLiteral("two.desktop"));
}

void DockControllerTest::movingToSameIndexIsNoOp()
{
    CountingPersistence persistence;
    DockController controller(nullptr, nullptr, &persistence);
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop"),
                                           QStringLiteral("two.desktop")}));

    QVERIFY(!controller.movePinned(QStringLiteral("two.desktop"), 1));
    QCOMPARE(persistence.calls, 0);
    QCOMPARE(controller.appModel()->desktopFileNameAt(0), QStringLiteral("one.desktop"));
}

void DockControllerTest::invalidOrRuntimeOnlySourceIsRejected()
{
    CountingPersistence persistence;
    DockController controller(nullptr, nullptr, &persistence);
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop")}));
    Astrea::Dock::DockApplicationRuntimeProjection projection;
    Astrea::Dock::DockApplicationRuntimeState runtime;
    runtime.desktopFileName = QStringLiteral("two.desktop");
    runtime.running = true;
    runtime.windowCount = 1;
    projection.states.insert(QStringLiteral("two.desktop"), runtime);
    projection.encounterOrder.append(QStringLiteral("two.desktop"));
    controller.appModel()->applyRuntimeProjection(projection);

    QVERIFY(!controller.movePinned(QStringLiteral("missing.desktop"), 0));
    QVERIFY(!controller.movePinned(QStringLiteral("two.desktop"), 0));
    QCOMPARE(persistence.calls, 0);
}

void DockControllerTest::persistenceFailureLeavesOrderUnchanged()
{
    CountingPersistence persistence;
    persistence.succeed = false;
    DockController controller(nullptr, nullptr, &persistence);
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop"),
                                           QStringLiteral("two.desktop")}));

    QVERIFY(!controller.movePinned(QStringLiteral("one.desktop"), 1));

    QCOMPARE(persistence.calls, 1);
    QCOMPARE(controller.appModel()->desktopFileNameAt(0), QStringLiteral("one.desktop"));
    QCOMPARE(controller.appModel()->desktopFileNameAt(1), QStringLiteral("two.desktop"));
    QVERIFY(controller.lastError().size() <= 512);
    QVERIFY(!controller.lastError().isEmpty());
}

void DockControllerTest::runtimeOnlyOrderingRemainsUnchangedAfterPinMove()
{
    CountingPersistence persistence;
    DockController controller(nullptr, nullptr, &persistence);
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop"),
                                           QStringLiteral("two.desktop")}));
    Astrea::Dock::DockApplicationRuntimeProjection projection;
    for (const QString &key : {QStringLiteral("runtime-a.desktop"),
                               QStringLiteral("runtime-b.desktop")}) {
        Astrea::Dock::DockApplicationRuntimeState state;
        state.desktopFileName = key;
        state.running = true;
        state.windowCount = 1;
        projection.states.insert(key, state);
        projection.encounterOrder.append(key);
    }
    controller.appModel()->applyRuntimeProjection(projection);

    QVERIFY(controller.movePinned(QStringLiteral("one.desktop"), 1));

    QCOMPARE(controller.appModel()->desktopFileNameAt(2), QStringLiteral("runtime-a.desktop"));
    QCOMPARE(controller.appModel()->desktopFileNameAt(3), QStringLiteral("runtime-b.desktop"));
}

void DockControllerTest::runtimeOnlyTaskUsesTaskKeyAndRejectsLauncherActions()
{
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);
    controller.setCatalogSnapshot(makeCatalog());

    Astrea::Dock::DockRuntimeSnapshot snapshot;
    snapshot.generation = 1;
    snapshot.revision = 1;
    Astrea::Dock::DockRuntimeWindow window;
    window.id = QStringLiteral("runtime-window");
    window.appId = QStringLiteral("steam_app_1091500");
    window.title = QStringLiteral("Cyberpunk 2077");
    window.focusSerial = 9;
    snapshot.windows.append(window);
    runtimeBackend.publishSnapshot(snapshot);

    const QString taskKey = QStringLiteral("app:steam_app_1091500");
    QCOMPARE(controller.appModel()->rowForTaskKey(taskKey), 0);
    QCOMPARE(controller.windowsForTaskKey(taskKey).size(), 1);
    QVERIFY(!controller.launchNewWindow(taskKey));
    QVERIFY(!controller.setPinned(taskKey, true));
}

void DockControllerTest::runtimeIdentityEnrichmentKeepsTaskKeyStable()
{
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);
    Astrea::Dock::DockRuntimeSnapshot snapshot;
    snapshot.generation = 4;
    snapshot.revision = 1;
    Astrea::Dock::DockRuntimeWindow window;
    window.id = QStringLiteral("9");
    window.appId = QStringLiteral("steam_app_1091500");
    window.title = QStringLiteral("Cyberpunk 2077");
    snapshot.windows.append(window);

    runtimeBackend.publishSnapshot(snapshot);

    QCOMPARE(controller.appModel()->rowCount(), 1);
    QCOMPARE(controller.appModel()->taskKeyAt(0),
             QStringLiteral("app:steam_app_1091500"));
    QCOMPARE(controller.appModel()->data(controller.appModel()->index(0, 0),
                                         DockAppModel::IconNameRole).toString(),
             QStringLiteral("steam_icon_1091500"));
}

void DockControllerTest::staleIdentityEnrichmentIsRejectedAtControllerBoundary()
{
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);
    Astrea::Dock::DockRuntimeSnapshot first;
    first.generation = 4;
    first.revision = 1;
    Astrea::Dock::DockRuntimeWindow firstWindow;
    firstWindow.id = QStringLiteral("9");
    firstWindow.appId = QStringLiteral("example-app");
    firstWindow.title = QStringLiteral("Old title");
    firstWindow.pid = 100;
    first.windows.append(firstWindow);
    runtimeBackend.publishSnapshot(first);

    Astrea::Dock::DockRuntimeSnapshot current = first;
    current.revision = 2;
    current.windows.first().title = QStringLiteral("New title");
    runtimeBackend.publishSnapshot(current);

    AppIdentity stale;
    stale.windowId = QStringLiteral("9");
    stale.pid = 100;
    stale.openGeneration = 3;
    stale.metadataFingerprint = QStringLiteral("example-app|Old title");
    stale.iconName = QStringLiteral("stale-icon");
    emit controller.identityResolver()->identityResolved(QStringLiteral("9"), stale);

    QCOMPARE(controller.appModel()->rowCount(), 1);
    QCOMPARE(controller.appModel()->taskKeyAt(0), QStringLiteral("app:example-app"));
    QVERIFY(controller.appModel()->data(controller.appModel()->index(0, 0),
                                        DockAppModel::IconNameRole).toString()
             != QStringLiteral("stale-icon"));
}

void DockControllerTest::catalogGainWhileLiveKeepsStableTaskKeyAndAvoidsRowChurn()
{
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);
    const auto empty = std::make_shared<DesktopEntrySnapshot>();
    controller.setCatalogSnapshot(empty);
    Astrea::Dock::DockRuntimeWindow window;
    window.id = QStringLiteral("late-window");
    window.appId = QStringLiteral("late-app");
    window.title = QStringLiteral("Late");
    runtimeBackend.publishSnapshot(runtimeSnapshot(1, {window}));

    QCOMPARE(controller.appModel()->rowCount(), 1);
    QCOMPARE(controller.appModel()->taskKeyAt(0), QStringLiteral("app:late-app"));
    QVERIFY(controller.appModel()->desktopFileNameAt(0).isEmpty());

    QSignalSpy insertedSpy(controller.appModel(), &QAbstractItemModel::rowsInserted);
    QSignalSpy removedSpy(controller.appModel(), &QAbstractItemModel::rowsRemoved);
    controller.setCatalogSnapshot(makeLateCatalog());

    QCOMPARE(controller.appModel()->rowCount(), 1);
    QCOMPARE(controller.appModel()->taskKeyAt(0), QStringLiteral("app:late-app"));
    QCOMPARE(controller.appModel()->desktopFileNameAt(0), QStringLiteral("late.desktop"));
    QVERIFY(controller.appModel()->index(0, 0).data(DockAppModel::ResolvedRole).toBool());
    QCOMPARE(insertedSpy.count(), 0);
    QCOMPARE(removedSpy.count(), 0);
}

void DockControllerTest::catalogLossWhileLiveKeepsStableTaskKey()
{
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);
    controller.setCatalogSnapshot(makeLateCatalog());
    Astrea::Dock::DockRuntimeWindow window;
    window.id = QStringLiteral("late-window");
    window.appId = QStringLiteral("late-app");
    runtimeBackend.publishSnapshot(runtimeSnapshot(1, {window}));
    const QString stableKey = controller.appModel()->taskKeyAt(0);

    QSignalSpy insertedSpy(controller.appModel(), &QAbstractItemModel::rowsInserted);
    QSignalSpy removedSpy(controller.appModel(), &QAbstractItemModel::rowsRemoved);
    controller.setCatalogSnapshot(std::make_shared<DesktopEntrySnapshot>());

    QCOMPARE(stableKey, QStringLiteral("desktop:late.desktop"));
    QCOMPARE(controller.appModel()->rowCount(), 1);
    QCOMPARE(controller.appModel()->taskKeyAt(0), stableKey);
    QCOMPARE(controller.appModel()->desktopFileNameAt(0), QStringLiteral("late.desktop"));
    QCOMPARE(insertedSpy.count(), 0);
    QCOMPARE(removedSpy.count(), 0);
}

void DockControllerTest::catalogLossWhilePinnedKeepsOneStableLiveRow()
{
    CountingPersistence persistence;
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller(nullptr, nullptr, &persistence);
    controller.attachRuntimeBackend(&runtimeBackend);
    controller.setCatalogSnapshot(std::make_shared<DesktopEntrySnapshot>());
    Astrea::Dock::DockRuntimeWindow window;
    window.id = QStringLiteral("late-window");
    window.appId = QStringLiteral("late-app");
    runtimeBackend.publishSnapshot(runtimeSnapshot(1, {window}));
    controller.setCatalogSnapshot(makeLateCatalog());
    QVERIFY(controller.setPinned(QStringLiteral("late.desktop"), true));

    const auto resolvedWindows =
        controller.windowsForDesktopFileName(QStringLiteral("late.desktop"));
    QCOMPARE(resolvedWindows.size(), 1);
    QCOMPARE(resolvedWindows.first().id, QStringLiteral("late-window"));

    QSignalSpy insertedSpy(controller.appModel(), &QAbstractItemModel::rowsInserted);
    QSignalSpy removedSpy(controller.appModel(), &QAbstractItemModel::rowsRemoved);
    controller.setCatalogSnapshot(std::make_shared<DesktopEntrySnapshot>());

    QCOMPARE(controller.appModel()->rowCount(), 1);
    QCOMPARE(controller.appModel()->taskKeyAt(0), QStringLiteral("app:late-app"));
    QCOMPARE(controller.appModel()->desktopFileNameAt(0), QStringLiteral("late.desktop"));
    QVERIFY(controller.appModel()->index(0, 0).data(DockAppModel::PinnedRole).toBool());
    QVERIFY(controller.appModel()->index(0, 0).data(DockAppModel::RunningRole).toBool());
    QCOMPARE(insertedSpy.count(), 0);
    QCOMPARE(removedSpy.count(), 0);

    const auto cataloglessWindows =
        controller.windowsForDesktopFileName(QStringLiteral("late.desktop"));
    QCOMPARE(cataloglessWindows.size(), 1);
    QCOMPARE(cataloglessWindows.first().id, QStringLiteral("late-window"));

    runtimeBackend.publishSnapshot(runtimeSnapshot(1, {}));
    QVERIFY(controller.windowsForDesktopFileName(QStringLiteral("late.desktop")).isEmpty());
}

void DockControllerTest::runtimeMetadataChangeKeepsStableTaskKey()
{
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);
    const auto empty = std::make_shared<DesktopEntrySnapshot>();
    controller.setCatalogSnapshot(empty);
    Astrea::Dock::DockRuntimeWindow first;
    first.id = QStringLiteral("stable-window");
    first.appId = QStringLiteral("before-app");
    first.title = QStringLiteral("Before");
    runtimeBackend.publishSnapshot(runtimeSnapshot(1, {first}));

    Astrea::Dock::DockRuntimeWindow changed = first;
    changed.appId = QStringLiteral("after-app");
    changed.title = QStringLiteral("After");
    runtimeBackend.publishSnapshot(runtimeSnapshot(1, {changed}));

    QCOMPARE(controller.appModel()->rowCount(), 1);
    QCOMPARE(controller.appModel()->taskKeyAt(0), QStringLiteral("app:before-app"));
    QCOMPARE(controller.windowsForTaskKey(QStringLiteral("app:before-app")).first().title,
             QStringLiteral("After"));
}

void DockControllerTest::newSameAppWindowJoinsStableTaskAfterCatalogGain()
{
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);
    controller.setCatalogSnapshot(std::make_shared<DesktopEntrySnapshot>());
    Astrea::Dock::DockRuntimeWindow first;
    first.id = QStringLiteral("first");
    first.appId = QStringLiteral("late-app");
    runtimeBackend.publishSnapshot(runtimeSnapshot(1, {first}));
    controller.setCatalogSnapshot(makeLateCatalog());

    Astrea::Dock::DockRuntimeWindow second = first;
    second.id = QStringLiteral("second");
    runtimeBackend.publishSnapshot(runtimeSnapshot(1, {first, second}));

    QCOMPARE(controller.appModel()->rowCount(), 1);
    QCOMPARE(controller.appModel()->taskKeyAt(0), QStringLiteral("app:late-app"));
    QCOMPARE(controller.appModel()->index(0, 0).data(DockAppModel::WindowCountRole).toInt(), 2);
    QVERIFY(controller.appModel()->rowForTaskKey(QStringLiteral("desktop:late.desktop")) < 0);
}

void DockControllerTest::lateResolvedRuntimeCanBePinnedWithoutDuplicate()
{
    CountingPersistence persistence;
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller(nullptr, nullptr, &persistence);
    controller.attachRuntimeBackend(&runtimeBackend);
    controller.setCatalogSnapshot(std::make_shared<DesktopEntrySnapshot>());
    Astrea::Dock::DockRuntimeWindow window;
    window.id = QStringLiteral("late-window");
    window.appId = QStringLiteral("late-app");
    runtimeBackend.publishSnapshot(runtimeSnapshot(1, {window}));
    controller.setCatalogSnapshot(makeLateCatalog());

    QVERIFY(controller.setPinned(QStringLiteral("late.desktop"), true));
    QCOMPARE(persistence.lastPins, QStringList{QStringLiteral("late.desktop")});
    QCOMPARE(controller.appModel()->rowCount(), 1);
    QCOMPARE(controller.appModel()->taskKeyAt(0), QStringLiteral("app:late-app"));
    QVERIFY(controller.appModel()->index(0, 0).data(DockAppModel::PinnedRole).toBool());
    QVERIFY(controller.appModel()->rowForTaskKey(QStringLiteral("desktop:late.desktop")) < 0);
}

void DockControllerTest::newLifecycleCanUseDesktopKeyAfterOldTaskEnds()
{
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);
    controller.setCatalogSnapshot(std::make_shared<DesktopEntrySnapshot>());
    Astrea::Dock::DockRuntimeWindow window;
    window.id = QStringLiteral("late-window");
    window.appId = QStringLiteral("late-app");
    runtimeBackend.publishSnapshot(runtimeSnapshot(1, {window}));
    QCOMPARE(controller.appModel()->taskKeyAt(0), QStringLiteral("app:late-app"));
    runtimeBackend.publishSnapshot(runtimeSnapshot(1, {}));
    controller.setCatalogSnapshot(makeLateCatalog());
    runtimeBackend.publishSnapshot(runtimeSnapshot(1, {window}));

    QCOMPARE(controller.appModel()->rowCount(), 1);
    QCOMPARE(controller.appModel()->taskKeyAt(0), QStringLiteral("desktop:late.desktop"));
}

void DockControllerTest::desktopFilenameLookupFindsStableRuntimeTask()
{
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);
    controller.setCatalogSnapshot(std::make_shared<DesktopEntrySnapshot>());
    Astrea::Dock::DockRuntimeWindow window;
    window.id = QStringLiteral("late-window");
    window.appId = QStringLiteral("late-app");
    runtimeBackend.publishSnapshot(runtimeSnapshot(1, {window}));
    controller.setCatalogSnapshot(makeLateCatalog());

    const auto windows = controller.windowsForDesktopFileName(QStringLiteral("late.desktop"));
    QCOMPARE(windows.size(), 1);
    QCOMPARE(windows.first().id, QStringLiteral("late-window"));
}

void DockControllerTest::personalizationPropertiesPropagateAndUnchangedConfigIsQuiet()
{
    DockController controller;
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop")}));
    QSignalSpy configSpy(&controller, &DockController::configChanged);
    QSignalSpy modelSpy(&controller, &DockController::modelChanged);

    DockConfig config = DockConfig::defaults();
    config.pins = {QStringLiteral("one.desktop")};
    config.position = QStringLiteral("right");
    config.edgeMargin = 30;
    config.floating = false;
    config.cornerRadius = 17;
    config.autoHide = QStringLiteral("always");
    config.indicatorStyle = QStringLiteral("dot");
    config.indicatorSize = 6;
    config.animationsEnabled = false;
    config.animationSpeed = 2.5;
    controller.applyConfig(config);

    QCOMPARE(controller.edgeMargin(), 30);
    QCOMPARE(controller.effectiveEdgeMargin(), 0);
    QCOMPARE(controller.position(), QStringLiteral("right"));
    QVERIFY(controller.vertical());
    QCOMPARE(controller.cornerRadius(), 17);
    QCOMPARE(controller.autoHide(), QStringLiteral("always"));
    QCOMPARE(controller.indicatorStyle(), QStringLiteral("dot"));
    QCOMPARE(controller.indicatorSize(), 6);
    QVERIFY(!controller.animationsEnabled());
    QCOMPARE(controller.animationSpeed(), 2.5);
    QCOMPARE(configSpy.count(), 1);
    QCOMPARE(modelSpy.count(), 1);

    controller.applyConfig(config);
    QCOMPARE(configSpy.count(), 1);
    QCOMPARE(modelSpy.count(), 1);
}

void DockControllerTest::autoHidePolicyKeepsSurfaceMappedAndReservationBounded()
{
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop")}));
    const int resting = controller.restingHeight();

    QVERIFY(controller.visible());
    QCOMPARE(controller.exclusiveZone(), resting);
    QVERIFY(controller.revealed());

    DockConfig config = DockConfig::defaults();
    config.pins = {QStringLiteral("one.desktop")};
    config.autoHide = QStringLiteral("always");
    controller.applyConfig(config);
    QVERIFY(controller.visible());
    QCOMPARE(controller.exclusiveZone(), 0);
    QVERIFY(!controller.revealed());

    controller.setPointerInside(true);
    QVERIFY(controller.revealed());
    controller.setPointerInside(false);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.revealed(), 1000);
    QVERIFY(controller.visible());

    config.autoHide = QStringLiteral("intelligent");
    controller.applyConfig(config);
    QCOMPARE(controller.exclusiveZone(), resting);
    QVERIFY(controller.revealed());

    Astrea::Dock::DockRuntimeSnapshot snapshot;
    snapshot.generation = 1;
    snapshot.revision = 1;
    Astrea::Dock::DockRuntimeWindow active;
    active.id = QStringLiteral("maximized");
    active.active = true;
    active.maximized = true;
    snapshot.windows.append(active);
    runtimeBackend.publishSnapshot(snapshot);
    QCOMPARE(controller.exclusiveZone(), 0);
    QVERIFY(!controller.revealed());

    active.active = true;
    active.maximized = false;
    snapshot.windows = {active};
    runtimeBackend.publishSnapshot(snapshot);
    QCOMPARE(controller.exclusiveZone(), resting);
    QVERIFY(controller.revealed());
}

void DockControllerTest::runtimeObstructionUpdatesSurfacePlacement()
{
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);
    DockConfig config = configWithPins({QStringLiteral("one.desktop")});
    config.edgeMargin = 12;
    config.autoHide = QStringLiteral("intelligent");
    controller.applyConfig(config);

    QSignalSpy placementSpy(&controller, &DockController::surfacePlacementChanged);
    QSignalSpy reservationSpy(&controller, &DockController::reservationChanged);
    QCOMPARE(controller.layerShellEdgeMargin(), 12);
    QCOMPARE(controller.chromeEdgeInset(), 0);
    QVERIFY(!controller.physicalEdgeReveal());

    Astrea::Dock::DockRuntimeSnapshot snapshot;
    snapshot.generation = 1;
    snapshot.revision = 1;
    Astrea::Dock::DockRuntimeWindow active;
    active.id = QStringLiteral("maximized");
    active.active = true;
    active.maximized = true;
    snapshot.windows.append(active);
    runtimeBackend.publishSnapshot(snapshot);

    QCOMPARE(placementSpy.count(), 1);
    QCOMPARE(reservationSpy.count(), 1);
    QCOMPARE(controller.layerShellEdgeMargin(), 0);
    QCOMPARE(controller.chromeEdgeInset(), 12);
    QVERIFY(controller.physicalEdgeReveal());

    active.active = true;
    active.maximized = false;
    snapshot.windows = {active};
    runtimeBackend.publishSnapshot(snapshot);

    QCOMPARE(placementSpy.count(), 2);
    QCOMPARE(reservationSpy.count(), 2);
    QCOMPARE(controller.layerShellEdgeMargin(), 12);
    QCOMPARE(controller.chromeEdgeInset(), 0);
    QVERIFY(!controller.physicalEdgeReveal());
}

void DockControllerTest::surfacePlacementTracksRuntimePositionTransitions()
{
    FakeDockRuntimeBackend runtimeBackend;
    DockController controller;
    controller.attachRuntimeBackend(&runtimeBackend);
    DockConfig config = configWithPins({QStringLiteral("one.desktop")});
    config.edgeMargin = 12;
    config.floating = true;
    config.autoHide = QStringLiteral("never");
    controller.applyConfig(config);

    QSignalSpy placementSpy(&controller, &DockController::surfacePlacementChanged);
    QCOMPARE(controller.surfacePlacement().position, QStringLiteral("bottom"));

    config.position = QStringLiteral("left");
    controller.applyConfig(config);
    QCOMPARE(placementSpy.count(), 1);
    QCOMPARE(controller.surfacePlacement().position, QStringLiteral("left"));

    config.position = QStringLiteral("right");
    controller.applyConfig(config);
    QCOMPARE(placementSpy.count(), 2);
    QCOMPARE(controller.surfacePlacement().position, QStringLiteral("right"));

    config.position = QStringLiteral("bottom");
    controller.applyConfig(config);
    QCOMPARE(placementSpy.count(), 3);
    QCOMPARE(controller.surfacePlacement().position, QStringLiteral("bottom"));

    config.autoHide = QStringLiteral("always");
    controller.applyConfig(config);
    placementSpy.clear();
    QCOMPARE(controller.surfacePlacement().layerShellEdgeMargin, 0);
    QCOMPARE(controller.surfacePlacement().chromeEdgeInset, 12);
    QVERIFY(controller.surfacePlacement().physicalEdgeReveal);

    for (const QString &position : {QStringLiteral("left"), QStringLiteral("right"),
                                    QStringLiteral("bottom")}) {
        config.position = position;
        controller.applyConfig(config);
        QCOMPARE(placementSpy.count(), 1);
        QCOMPARE(controller.surfacePlacement().position, position);
        placementSpy.clear();
    }

    config.autoHide = QStringLiteral("intelligent");
    controller.applyConfig(config);

    Astrea::Dock::DockRuntimeSnapshot snapshot;
    snapshot.generation = 1;
    snapshot.revision = 1;
    Astrea::Dock::DockRuntimeWindow active;
    active.id = QStringLiteral("maximized");
    active.active = true;
    active.maximized = true;
    snapshot.windows.append(active);
    runtimeBackend.publishSnapshot(snapshot);
    placementSpy.clear();
    QCOMPARE(controller.surfacePlacement().layerShellEdgeMargin, 0);
    QCOMPARE(controller.surfacePlacement().chromeEdgeInset, 12);
    QVERIFY(controller.surfacePlacement().physicalEdgeReveal);

    for (const QString &position : {QStringLiteral("left"), QStringLiteral("right"),
                                    QStringLiteral("bottom")}) {
        config.position = position;
        controller.applyConfig(config);
        QCOMPARE(placementSpy.count(), 1);
        QCOMPARE(controller.surfacePlacement().position, position);
        QCOMPARE(controller.surfacePlacement().layerShellEdgeMargin, 0);
        QCOMPARE(controller.surfacePlacement().chromeEdgeInset, 12);
        QVERIFY(controller.surfacePlacement().physicalEdgeReveal);
        placementSpy.clear();
    }

    const int cornerRadius = config.cornerRadius;
    config.cornerRadius = cornerRadius + 1;
    controller.applyConfig(config);
    QCOMPARE(placementSpy.count(), 0);
}

QTEST_MAIN(DockControllerTest)
#include "DockControllerTest.moc"
