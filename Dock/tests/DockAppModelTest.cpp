#include <QSignalSpy>
#include <QTest>

#include "core/DockAppModel.hpp"
#include "platform/typhon/DockApplicationStateProjector.hpp"

class DockAppModelTest final : public QObject {
    Q_OBJECT

private slots:
    void initialPopulationPreservesConfiguredOrder();
    void duplicatePinsCollapseByFirstOccurrence();
    void unresolvedPinRemainsVisible();
    void catalogResolutionEmitsExactRoles();
    void identicalUpdateIsNoOp();
    void launchingAndErrorArePerItem();
    void pinInsertionAndRemovalUsesStructuralSignals();
    void stableKeyRetainsItemStateAfterReorder();
    void runtimeOnlyApplicationAppearsAfterPins();
    void runtimeOnlyApplicationDisappearsAfterLastWindowCloses();
    void multipleWindowsRemainOneDockRow();
    void minimizedRuntimeApplicationRemainsVisible();
    void runtimeOrderSurvivesFocusChanges();
    void newlyObservedRuntimeApplicationAppends();
    void runtimeOnlyApplicationBecomesPinnedWithoutDuplication();
    void runningPinnedApplicationBecomesRuntimeOnly();
    void stoppedPinnedApplicationIsRemoved();
    void authorityLossKeepsPinsAndRemovesRuntimeOnlyRows();
    void runtimeStatePreservesLaunchState();
    void runtimeStateDisappearsWhenWindowCloses();
    void unknownRuntimeStateDoesNotClaimStopped();
};

static std::shared_ptr<DesktopEntrySnapshot> makeSnapshot(const QStringList &names)
{
    auto snapshot = std::make_shared<DesktopEntrySnapshot>();
    for (const QString &fileName : names) {
        DesktopEntryRecord record;
        record.desktopFileName = fileName;
        record.id = fileName.chopped(8);
        record.name = record.id.toUpper();
        record.icon = record.id + QStringLiteral("-icon");
        record.sourceFilePath = QStringLiteral("/tmp/") + fileName;
        snapshot->byDesktopId.insert(record.id, snapshot->entries.size());
        snapshot->byDesktopFileName.insert(record.desktopFileName, snapshot->entries.size());
        snapshot->entries.append(record);
    }
    return snapshot;
}

static Astrea::Typhon::DockApplicationRuntimeState runtimeState(
    const QString &desktopFileName, int windowCount = 1, bool active = false,
    bool running = true)
{
    Astrea::Typhon::DockApplicationRuntimeState state;
    state.desktopFileName = desktopFileName;
    state.running = running;
    state.active = active;
    state.windowCount = windowCount;
    return state;
}

static Astrea::Typhon::DockApplicationRuntimeProjection runtimeProjection(
    std::initializer_list<Astrea::Typhon::DockApplicationRuntimeState> states,
    const QStringList &encounterOrder = {})
{
    Astrea::Typhon::DockApplicationRuntimeProjection projection;
    for (const auto &state : states) {
        projection.states.insert(state.desktopFileName, state);
        if (!encounterOrder.contains(state.desktopFileName))
            projection.encounterOrder.append(state.desktopFileName);
    }
    for (const QString &key : encounterOrder) {
        if (!projection.encounterOrder.contains(key))
            projection.encounterOrder.append(key);
    }
    return projection;
}

void DockAppModelTest::initialPopulationPreservesConfiguredOrder()
{
    DockAppModel model;
    model.setCatalogSnapshot(makeSnapshot({QStringLiteral("one.desktop"), QStringLiteral("two.desktop")}));
    model.setPins({QStringLiteral("two.desktop"), QStringLiteral("one.desktop")});

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0, 0), DockAppModel::DesktopFileNameRole).toString(),
             QStringLiteral("two.desktop"));
    QCOMPARE(model.data(model.index(1, 0), DockAppModel::DesktopFileNameRole).toString(),
             QStringLiteral("one.desktop"));
}

void DockAppModelTest::duplicatePinsCollapseByFirstOccurrence()
{
    DockAppModel model;
    model.setPins({QStringLiteral("one.desktop"), QStringLiteral("one.desktop"),
                   QStringLiteral("two.desktop"), QStringLiteral("one.desktop")});

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0, 0), DockAppModel::DesktopFileNameRole).toString(),
             QStringLiteral("one.desktop"));
}

void DockAppModelTest::unresolvedPinRemainsVisible()
{
    DockAppModel model;
    model.setPins({QStringLiteral("missing.desktop")});

    const QModelIndex index = model.index(0, 0);
    QCOMPARE(model.rowCount(), 1);
    QVERIFY(!model.data(index, DockAppModel::ResolvedRole).toBool());
    QCOMPARE(model.data(index, DockAppModel::DisplayNameRole).toString(), QStringLiteral("missing"));
    QVERIFY(model.data(index, DockAppModel::PinnedRole).toBool());
}

void DockAppModelTest::catalogResolutionEmitsExactRoles()
{
    DockAppModel model;
    model.setPins({QStringLiteral("one.desktop")});
    QSignalSpy dataSpy(&model, &QAbstractItemModel::dataChanged);

    model.setCatalogSnapshot(makeSnapshot({QStringLiteral("one.desktop")}));

    QCOMPARE(dataSpy.count(), 1);
    const QList<int> roles = dataSpy.at(0).at(2).value<QList<int>>();
    QVERIFY(roles.contains(DockAppModel::DisplayNameRole));
    QVERIFY(roles.contains(DockAppModel::IconNameRole));
    QVERIFY(!roles.contains(DockAppModel::IconPathRole));
    QVERIFY(roles.contains(DockAppModel::ResolvedRole));
    QVERIFY(!roles.contains(DockAppModel::PinnedRole));
}

void DockAppModelTest::identicalUpdateIsNoOp()
{
    DockAppModel model;
    const auto snapshot = makeSnapshot({QStringLiteral("one.desktop")});
    model.setCatalogSnapshot(snapshot);
    model.setPins({QStringLiteral("one.desktop")});
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy dataSpy(&model, &QAbstractItemModel::dataChanged);

    model.setPins({QStringLiteral("one.desktop")});
    model.setCatalogSnapshot(snapshot);

    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(dataSpy.count(), 0);
}

void DockAppModelTest::launchingAndErrorArePerItem()
{
    DockAppModel model;
    model.setPins({QStringLiteral("one.desktop"), QStringLiteral("two.desktop")});
    QVERIFY(model.setLaunching(QStringLiteral("one.desktop"), true));
    QVERIFY(model.setLaunchError(QStringLiteral("two.desktop"), QStringLiteral("failed")));

    QVERIFY(model.data(model.index(0, 0), DockAppModel::LaunchingRole).toBool());
    QVERIFY(!model.data(model.index(1, 0), DockAppModel::LaunchingRole).toBool());
    QCOMPARE(model.data(model.index(1, 0), DockAppModel::LaunchErrorRole).toString(),
             QStringLiteral("failed"));
    QVERIFY(model.data(model.index(0, 0), DockAppModel::LaunchErrorRole).toString().isEmpty());
}

void DockAppModelTest::pinInsertionAndRemovalUsesStructuralSignals()
{
    DockAppModel model;
    model.setPins({QStringLiteral("one.desktop"), QStringLiteral("two.desktop")});
    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);

    model.setPins({QStringLiteral("two.desktop"), QStringLiteral("three.desktop")});

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(insertedSpy.count(), 1);
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(model.desktopFileNameAt(0), QStringLiteral("two.desktop"));
    QCOMPARE(model.desktopFileNameAt(1), QStringLiteral("three.desktop"));
}

void DockAppModelTest::stableKeyRetainsItemStateAfterReorder()
{
    DockAppModel model;
    model.setPins({QStringLiteral("one.desktop"), QStringLiteral("two.desktop")});
    QVERIFY(model.setLaunching(QStringLiteral("one.desktop"), true));

    model.setPins({QStringLiteral("two.desktop"), QStringLiteral("one.desktop")});

    const QModelIndex oneIndex = model.index(1, 0);
    QCOMPARE(oneIndex.data(DockAppModel::DesktopFileNameRole).toString(), QStringLiteral("one.desktop"));
    QVERIFY(oneIndex.data(DockAppModel::LaunchingRole).toBool());
}

void DockAppModelTest::runtimeOnlyApplicationAppearsAfterPins()
{
    DockAppModel model;
    model.setCatalogSnapshot(makeSnapshot({QStringLiteral("one.desktop"),
                                           QStringLiteral("two.desktop")}));
    model.setPins({QStringLiteral("one.desktop")});

    model.applyRuntimeProjection(runtimeProjection({runtimeState(QStringLiteral("two.desktop"))},
                                                    {QStringLiteral("two.desktop")}));

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.desktopFileNameAt(0), QStringLiteral("one.desktop"));
    QCOMPARE(model.desktopFileNameAt(1), QStringLiteral("two.desktop"));
    QVERIFY(model.index(0, 0).data(DockAppModel::PinnedRole).toBool());
    QVERIFY(!model.index(1, 0).data(DockAppModel::PinnedRole).toBool());
    QVERIFY(model.index(1, 0).data(DockAppModel::RuntimeKnownRole).toBool());
    QVERIFY(model.index(1, 0).data(DockAppModel::RunningRole).toBool());
}

void DockAppModelTest::runtimeOnlyApplicationDisappearsAfterLastWindowCloses()
{
    DockAppModel model;
    model.setPins({QStringLiteral("one.desktop")});
    model.applyRuntimeProjection(runtimeProjection({runtimeState(QStringLiteral("two.desktop"))}));
    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);

    model.applyRuntimeProjection({});

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.desktopFileNameAt(0), QStringLiteral("one.desktop"));
    QCOMPARE(removedSpy.count(), 1);
}

void DockAppModelTest::multipleWindowsRemainOneDockRow()
{
    DockAppModel model;
    model.setPins({QStringLiteral("one.desktop")});
    model.applyRuntimeProjection(runtimeProjection({runtimeState(QStringLiteral("two.desktop"), 3)}));

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.index(1, 0).data(DockAppModel::WindowCountRole).toInt(), 3);
}

void DockAppModelTest::minimizedRuntimeApplicationRemainsVisible()
{
    DockAppModel model;
    model.setPins({QStringLiteral("one.desktop")});
    model.applyRuntimeProjection(runtimeProjection({runtimeState(QStringLiteral("two.desktop"), 1,
                                                                  false, true)}));

    QCOMPARE(model.rowCount(), 2);
    QVERIFY(model.index(1, 0).data(DockAppModel::RunningRole).toBool());
}

void DockAppModelTest::runtimeOrderSurvivesFocusChanges()
{
    DockAppModel model;
    model.setPins({QStringLiteral("one.desktop")});
    model.applyRuntimeProjection(runtimeProjection({runtimeState(QStringLiteral("two.desktop")),
                                                    runtimeState(QStringLiteral("three.desktop"))},
                                                   {QStringLiteral("two.desktop"),
                                                    QStringLiteral("three.desktop")}));
    model.applyRuntimeProjection(runtimeProjection({runtimeState(QStringLiteral("two.desktop"), 1,
                                                                  true),
                                                    runtimeState(QStringLiteral("three.desktop"), 1,
                                                                  false)},
                                                   {QStringLiteral("three.desktop"),
                                                    QStringLiteral("two.desktop")}));

    QCOMPARE(model.desktopFileNameAt(1), QStringLiteral("two.desktop"));
    QCOMPARE(model.desktopFileNameAt(2), QStringLiteral("three.desktop"));
}

void DockAppModelTest::newlyObservedRuntimeApplicationAppends()
{
    DockAppModel model;
    model.setPins({QStringLiteral("one.desktop")});
    model.applyRuntimeProjection(runtimeProjection({runtimeState(QStringLiteral("two.desktop")),
                                                    runtimeState(QStringLiteral("three.desktop"))},
                                                   {QStringLiteral("two.desktop"),
                                                    QStringLiteral("three.desktop")}));
    model.applyRuntimeProjection(runtimeProjection({runtimeState(QStringLiteral("two.desktop")),
                                                    runtimeState(QStringLiteral("three.desktop")),
                                                    runtimeState(QStringLiteral("four.desktop"))},
                                                   {QStringLiteral("three.desktop"),
                                                    QStringLiteral("two.desktop"),
                                                    QStringLiteral("four.desktop")}));

    QCOMPARE(model.desktopFileNameAt(1), QStringLiteral("two.desktop"));
    QCOMPARE(model.desktopFileNameAt(2), QStringLiteral("three.desktop"));
    QCOMPARE(model.desktopFileNameAt(3), QStringLiteral("four.desktop"));
}

void DockAppModelTest::runtimeOnlyApplicationBecomesPinnedWithoutDuplication()
{
    DockAppModel model;
    model.applyRuntimeProjection(runtimeProjection({runtimeState(QStringLiteral("two.desktop"))}));
    model.setPins({QStringLiteral("two.desktop"), QStringLiteral("one.desktop")});

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.desktopFileNameAt(0), QStringLiteral("two.desktop"));
    QVERIFY(model.index(0, 0).data(DockAppModel::PinnedRole).toBool());
    QVERIFY(model.index(0, 0).data(DockAppModel::RunningRole).toBool());
}

void DockAppModelTest::runningPinnedApplicationBecomesRuntimeOnly()
{
    DockAppModel model;
    model.setPins({QStringLiteral("one.desktop"), QStringLiteral("two.desktop")});
    model.applyRuntimeProjection(runtimeProjection({runtimeState(QStringLiteral("two.desktop"), 2, true)}));
    model.setPins({QStringLiteral("one.desktop")});

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.desktopFileNameAt(1), QStringLiteral("two.desktop"));
    QVERIFY(!model.index(1, 0).data(DockAppModel::PinnedRole).toBool());
    QCOMPARE(model.index(1, 0).data(DockAppModel::WindowCountRole).toInt(), 2);
}

void DockAppModelTest::stoppedPinnedApplicationIsRemoved()
{
    DockAppModel model;
    model.setPins({QStringLiteral("one.desktop"), QStringLiteral("two.desktop")});
    model.applyRuntimeProjection(runtimeProjection({}));
    model.setPins({QStringLiteral("one.desktop")});

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.desktopFileNameAt(0), QStringLiteral("one.desktop"));
}

void DockAppModelTest::authorityLossKeepsPinsAndRemovesRuntimeOnlyRows()
{
    DockAppModel model;
    model.setPins({QStringLiteral("one.desktop")});
    model.applyRuntimeProjection(runtimeProjection({runtimeState(QStringLiteral("one.desktop")),
                                                    runtimeState(QStringLiteral("two.desktop"))}));

    model.clearRuntimeProjection();

    QCOMPARE(model.rowCount(), 1);
    QVERIFY(!model.index(0, 0).data(DockAppModel::RuntimeKnownRole).toBool());
    QVERIFY(!model.index(0, 0).data(DockAppModel::RunningRole).toBool());
}

void DockAppModelTest::runtimeStatePreservesLaunchState()
{
    DockAppModel model;
    model.setCatalogSnapshot(makeSnapshot({QStringLiteral("one.desktop")}));
    model.setPins({QStringLiteral("one.desktop")});
    QVERIFY(model.setLaunching(QStringLiteral("one.desktop"), true));
    QVERIFY(model.setLaunchError(QStringLiteral("one.desktop"), QStringLiteral("pending")));

    Astrea::Typhon::DockApplicationRuntimeState state;
    state.desktopFileName = QStringLiteral("one.desktop");
    state.running = true;
    state.active = true;
    state.windowCount = 2;
    model.applyRuntimeProjection(runtimeProjection({state}));

    const QModelIndex index = model.index(0, 0);
    QVERIFY(index.data(DockAppModel::RunningRole).toBool());
    QVERIFY(index.data(DockAppModel::RuntimeKnownRole).toBool());
    QVERIFY(index.data(DockAppModel::ActiveRole).toBool());
    QCOMPARE(index.data(DockAppModel::WindowCountRole).toInt(), 2);
    QVERIFY(index.data(DockAppModel::LaunchingRole).toBool());
    QCOMPARE(index.data(DockAppModel::LaunchErrorRole).toString(), QStringLiteral("pending"));
}

void DockAppModelTest::unknownRuntimeStateDoesNotClaimStopped()
{
    DockAppModel model;
    model.setCatalogSnapshot(makeSnapshot({QStringLiteral("one.desktop")}));
    model.setPins({QStringLiteral("one.desktop")});

    Astrea::Typhon::DockApplicationRuntimeState state;
    state.desktopFileName = QStringLiteral("one.desktop");
    state.running = true;
    state.windowCount = 1;
    model.applyRuntimeProjection(runtimeProjection({state}));
    model.clearRuntimeProjection();

    const QModelIndex index = model.index(0, 0);
    QVERIFY(!index.data(DockAppModel::RuntimeKnownRole).toBool());
    QVERIFY(!index.data(DockAppModel::RunningRole).toBool());
    QCOMPARE(index.data(DockAppModel::WindowCountRole).toInt(), 0);
}

void DockAppModelTest::runtimeStateDisappearsWhenWindowCloses()
{
    DockAppModel model;
    model.setPins({QStringLiteral("one.desktop"), QStringLiteral("two.desktop")});
    Astrea::Typhon::DockApplicationRuntimeState state;
    state.desktopFileName = QStringLiteral("one.desktop");
    state.running = true;
    state.active = true;
    state.windowCount = 1;
    model.applyRuntimeProjection(runtimeProjection({state}));
    model.applyRuntimeProjection({});

    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        QVERIFY(!index.data(DockAppModel::RunningRole).toBool());
        QVERIFY(!index.data(DockAppModel::ActiveRole).toBool());
        QCOMPARE(index.data(DockAppModel::WindowCountRole).toInt(), 0);
    }
}

QTEST_MAIN(DockAppModelTest)
#include "DockAppModelTest.moc"
