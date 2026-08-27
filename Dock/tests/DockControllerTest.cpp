#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>

#include "core/DockController.hpp"
#include "services/DockConfigPersistence.hpp"

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
    void newWindowLaunchBypassesRunningActivation();
    void pinMutationRequiresSuccessfulPersistence();
    void pinCountersExcludeRuntimeOnlyRows();
    void movingFirstPinToEndPersistsAndPreservesState();
    void movingLastPinToBeginningPersists();
    void movingToSameIndexIsNoOp();
    void invalidOrRuntimeOnlySourceIsRejected();
    void persistenceFailureLeavesOrderUnchanged();
    void runtimeOnlyOrderingRemainsUnchangedAfterPinMove();
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
    DockController controller(&launcher);
    controller.setCatalogSnapshot(makeCatalog());
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop")}));

    Astrea::Typhon::Snapshot snapshot;
    snapshot.connectionGeneration = 1;
    snapshot.revision = 1;
    snapshot.total = 1;
    Astrea::Typhon::Toplevel window;
    window.id = QStringLiteral("window-1");
    window.appId = QStringLiteral("one");
    window.states = Astrea::Typhon::ToplevelStates{Astrea::Typhon::ToplevelStateFlag::Active};
    snapshot.windows.append(window);
    controller.applyTyphonSnapshot(snapshot);

    const QModelIndex index = controller.appModel()->index(0, 0);
    QVERIFY(index.data(DockAppModel::RuntimeKnownRole).toBool());
    QVERIFY(index.data(DockAppModel::RunningRole).toBool());
    QVERIFY(index.data(DockAppModel::ActiveRole).toBool());
    QCOMPARE(index.data(DockAppModel::WindowCountRole).toInt(), 1);

    controller.launch(0);
    QCOMPARE(launcher.requests.size(), 0);

    controller.clearTyphonRuntime();
    QVERIFY(!controller.appModel()->data(index, DockAppModel::RuntimeKnownRole).toBool());
    controller.launch(0);
    QCOMPARE(launcher.requests.size(), 1);
}

void DockControllerTest::runtimeWindowsKeepExactIdentityAndTitles()
{
    DockController controller;
    controller.setCatalogSnapshot(makeCatalog());
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop")}));

    Astrea::Typhon::Snapshot snapshot;
    snapshot.connectionGeneration = 2;
    snapshot.revision = 4;
    Astrea::Typhon::Toplevel first;
    first.id = QStringLiteral("window-a");
    first.appId = QStringLiteral("one");
    first.title = QStringLiteral("First title");
    Astrea::Typhon::Toplevel second;
    second.id = QStringLiteral("window-b");
    second.appId = QStringLiteral("one");
    second.title = QStringLiteral("Second title");
    snapshot.windows = {first, second};
    controller.applyTyphonSnapshot(snapshot);

    const auto windows = controller.windowsForDesktopFileName(QStringLiteral("one.desktop"));
    QCOMPARE(windows.size(), 2);
    QCOMPARE(windows.at(0).id, QStringLiteral("window-a"));
    QCOMPARE(windows.at(0).title, QStringLiteral("First title"));
    QCOMPARE(windows.at(1).id, QStringLiteral("window-b"));
    QVERIFY(controller.windowsForDesktopFileName(QStringLiteral("two.desktop")).isEmpty());
}

void DockControllerTest::newWindowLaunchBypassesRunningActivation()
{
    FakeLauncher launcher;
    DockController controller(&launcher);
    controller.setCatalogSnapshot(makeCatalog());
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop")}));

    Astrea::Typhon::Snapshot snapshot;
    snapshot.connectionGeneration = 1;
    snapshot.revision = 1;
    Astrea::Typhon::Toplevel window;
    window.id = QStringLiteral("window-1");
    window.appId = QStringLiteral("one");
    snapshot.windows.append(window);
    controller.applyTyphonSnapshot(snapshot);

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
    DockController controller(&launcher);
    controller.setCatalogSnapshot(makeCatalog());
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop"),
                                           QStringLiteral("missing.desktop")}));

    Astrea::Typhon::Snapshot snapshot;
    snapshot.connectionGeneration = 1;
    snapshot.revision = 1;
    Astrea::Typhon::Toplevel window;
    window.id = QStringLiteral("window-2");
    window.appId = QStringLiteral("two");
    snapshot.windows.append(window);
    controller.applyTyphonSnapshot(snapshot);

    QCOMPARE(controller.pinCount(), 2);
    QCOMPARE(controller.appModel()->rowCount(), 3);
    QCOMPARE(controller.resolvedPinCount(), 1);
    QVERIFY(!controller.appModel()->index(2, 0).data(DockAppModel::PinnedRole).toBool());
}

void DockControllerTest::movingFirstPinToEndPersistsAndPreservesState()
{
    CountingPersistence persistence;
    DockController controller(nullptr, nullptr, &persistence);
    controller.applyConfig(configWithPins({QStringLiteral("one.desktop"),
                                           QStringLiteral("two.desktop"),
                                           QStringLiteral("three.desktop")}));
    QVERIFY(controller.appModel()->setLaunching(QStringLiteral("one.desktop"), true));

    QVERIFY(controller.movePinned(QStringLiteral("one.desktop"), 2));

    QCOMPARE(persistence.calls, 1);
    QCOMPARE(persistence.lastPins, QStringList({QStringLiteral("two.desktop"),
                                                 QStringLiteral("three.desktop"),
                                                 QStringLiteral("one.desktop")}));
    QCOMPARE(controller.appModel()->desktopFileNameAt(0), QStringLiteral("two.desktop"));
    QCOMPARE(controller.appModel()->desktopFileNameAt(2), QStringLiteral("one.desktop"));
    QVERIFY(controller.appModel()->index(2, 0).data(DockAppModel::LaunchingRole).toBool());
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
    Astrea::Typhon::DockApplicationRuntimeProjection projection;
    Astrea::Typhon::DockApplicationRuntimeState runtime;
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
    Astrea::Typhon::DockApplicationRuntimeProjection projection;
    for (const QString &key : {QStringLiteral("runtime-a.desktop"),
                               QStringLiteral("runtime-b.desktop")}) {
        Astrea::Typhon::DockApplicationRuntimeState state;
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

QTEST_MAIN(DockControllerTest)
#include "DockControllerTest.moc"
