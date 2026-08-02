#include <QSignalSpy>
#include <QTest>

#include "core/DockAppModel.hpp"

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

QTEST_MAIN(DockAppModelTest)
#include "DockAppModelTest.moc"
