#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>

#include "core/DockController.hpp"

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

QTEST_MAIN(DockControllerTest)
#include "DockControllerTest.moc"
