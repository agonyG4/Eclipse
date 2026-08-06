#include <QTest>
#include <QSignalSpy>

#include "core/AltTabWindowModel.hpp"
#include "core/CompositorTypes.hpp"

static WindowInfo makeWindow(const QString &id, int wsId = 1) {
    WindowInfo w;
    w.windowId = WindowId{id};
    w.pid = 1000 + id.toInt(nullptr, 16);
    w.className = QStringLiteral("App_") + id;
    w.title = QStringLiteral("Title ") + id;
    w.displayName = w.className;
    w.workspaceId = WorkspaceId{QString::number(wsId)};
    w.focusHistoryId = 0;
    return w;
}

class TestAltTabWindowModel : public QObject {
    Q_OBJECT

private slots:
    void testRoles();
    void testSetWindows();
    void testUpdateWindow();
    void testRemoveWindow();
    void testClear();
    void testSelectedIndex();
    void testStableKeys();
    void testStableSelectionOnReplace();
    void testInsertionBeforeSelectionKeepsWindowId();
    void testRemovalBeforeSelectionKeepsWindowId();
    void testSelectedRowRemovedFallsBackNearOldIndex();
    void testIdenticalSnapshotNoBroadcast();
    void testCountChangedOnlyOnCountChange();
    void testInsertThenUpdate();
    void testReordering();
    void testSelectedRowRemoval();
};

void TestAltTabWindowModel::testRoles() {
    AltTabWindowModel model;

    QHash<int, QByteArray> roles = model.roleNames();
    QVERIFY(roles.contains(AltTabWindowModel::WindowIdRole));
    QVERIFY(roles.contains(AltTabWindowModel::DisplayNameRole));
    QVERIFY(roles.contains(AltTabWindowModel::IconNameRole));
    QVERIFY(roles.contains(AltTabWindowModel::IconUrlRole));
    QVERIFY(roles.contains(AltTabWindowModel::SelectedRole));
    QVERIFY(roles.contains(AltTabWindowModel::WorkspaceIdRole));
    QVERIFY(roles.contains(AltTabWindowModel::OutputRole));
    QVERIFY(roles.contains(AltTabWindowModel::HiddenRole));
    QVERIFY(roles.contains(AltTabWindowModel::MinimizedRole));
    QVERIFY(roles.contains(AltTabWindowModel::ActiveRole));
    QVERIFY(roles.contains(AltTabWindowModel::FocusOrderRole));

    QCOMPARE(roles.value(AltTabWindowModel::WindowIdRole), QByteArray("address"));
    QCOMPARE(roles.value(AltTabWindowModel::DisplayNameRole), QByteArray("displayName"));
    QCOMPARE(roles.value(AltTabWindowModel::IconNameRole), QByteArray("iconName"));
    QCOMPARE(roles.value(AltTabWindowModel::SelectedRole), QByteArray("selected"));
    QCOMPARE(roles.value(AltTabWindowModel::OutputRole), QByteArray("output"));
    QCOMPARE(roles.value(AltTabWindowModel::HiddenRole), QByteArray("hidden"));
    QCOMPARE(roles.value(AltTabWindowModel::MinimizedRole), QByteArray("minimized"));
    QCOMPARE(roles.value(AltTabWindowModel::ActiveRole), QByteArray("active"));
}

void TestAltTabWindowModel::testSetWindows() {
    AltTabWindowModel model;
    QSignalSpy countSpy(&model, &AltTabWindowModel::countChanged);

    QVector<WindowInfo> windows;
    windows.append(makeWindow(QStringLiteral("0x0001")));
    windows.append(makeWindow(QStringLiteral("0x0002")));
    windows.append(makeWindow(QStringLiteral("0x0003")));

    model.setWindows(windows);
    QCOMPARE(model.count(), 3);
    QCOMPARE(countSpy.count(), 1);

    QCOMPARE(model.data(model.index(0), AltTabWindowModel::WindowIdRole).toString(),
             QStringLiteral("0x0001"));
    QCOMPARE(model.data(model.index(1), AltTabWindowModel::WindowIdRole).toString(),
             QStringLiteral("0x0002"));

    QCOMPARE(model.indexOf(QStringLiteral("0x0001")), 0);
    QCOMPARE(model.indexOf(QStringLiteral("0x9999")), -1);
}

void TestAltTabWindowModel::testUpdateWindow() {
    AltTabWindowModel model;

    WindowInfo w = makeWindow(QStringLiteral("0x1234"));
    model.setWindows({w});
    QCOMPARE(model.count(), 1);

    WindowInfo updated = w;
    updated.iconName = QStringLiteral("firefox");
    model.updateWindow(updated);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.data(model.index(0), AltTabWindowModel::IconNameRole).toString(),
             QStringLiteral("firefox"));

    WindowInfo w2 = makeWindow(QStringLiteral("0x5678"));
    model.updateWindow(w2);
    QCOMPARE(model.count(), 2);
}

void TestAltTabWindowModel::testRemoveWindow() {
    AltTabWindowModel model;

    QVector<WindowInfo> windows;
    windows.append(makeWindow(QStringLiteral("0x0001")));
    windows.append(makeWindow(QStringLiteral("0x0002")));
    windows.append(makeWindow(QStringLiteral("0x0003")));
    model.setWindows(windows);

    model.removeWindow(QStringLiteral("0x0002"));
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.at(0).windowId.value, QStringLiteral("0x0001"));
    QCOMPARE(model.at(1).windowId.value, QStringLiteral("0x0003"));

    model.removeWindow(QStringLiteral("nonexistent"));
    QCOMPARE(model.count(), 2);
}

void TestAltTabWindowModel::testClear() {
    AltTabWindowModel model;
    QSignalSpy countSpy(&model, &AltTabWindowModel::countChanged);

    model.setWindows({makeWindow(QStringLiteral("0x1234"))});
    QCOMPARE(countSpy.count(), 1);

    model.clear();
    QCOMPARE(model.count(), 0);
    QCOMPARE(countSpy.count(), 2);

    model.clear();
    QCOMPARE(model.count(), 0);
    QCOMPARE(countSpy.count(), 2);
}

void TestAltTabWindowModel::testSelectedIndex() {
    AltTabWindowModel model;

    QVector<WindowInfo> windows;
    windows.append(makeWindow(QStringLiteral("0x0001")));
    windows.append(makeWindow(QStringLiteral("0x0002")));
    windows.append(makeWindow(QStringLiteral("0x0003")));
    model.setWindows(windows);

    model.setSelectedIndex(1);
    QCOMPARE(model.selectedIndex(), 1);

    QCOMPARE(model.data(model.index(0), AltTabWindowModel::SelectedRole).toBool(), false);
    QCOMPARE(model.data(model.index(1), AltTabWindowModel::SelectedRole).toBool(), true);
    QCOMPARE(model.data(model.index(2), AltTabWindowModel::SelectedRole).toBool(), false);

    model.setSelectedIndex(100);
    QCOMPARE(model.selectedIndex(), 2);

    model.setSelectedIndex(-1);
    QCOMPARE(model.selectedIndex(), -1);
}

void TestAltTabWindowModel::testStableKeys() {
    WindowInfo w;
    w.windowId = WindowId{QStringLiteral("0x1234")};
    w.pid = 1234;
    w.className = QStringLiteral("TestApp");
    w.initialClass = QStringLiteral("test-app");
    w.title = QStringLiteral("Test Window");
    w.initialTitle = QStringLiteral("test-window");

    QCOMPARE(w.stableKey(), QStringLiteral("0x1234"));
    QVERIFY(!w.metaKey().isEmpty());
}

void TestAltTabWindowModel::testStableSelectionOnReplace() {
    AltTabWindowModel model;

    QVector<WindowInfo> windows;
    windows.append(makeWindow(QStringLiteral("0x0001")));
    windows.append(makeWindow(QStringLiteral("0x0002")));
    windows.append(makeWindow(QStringLiteral("0x0003")));
    model.setWindows(windows);
    model.setSelectedIndex(2);
    QCOMPARE(model.at(model.selectedIndex()).windowId.value, QStringLiteral("0x0003"));

    QVector<WindowInfo> reordered;
    reordered.append(makeWindow(QStringLiteral("0x0003")));
    reordered.append(makeWindow(QStringLiteral("0x0001")));
    reordered.append(makeWindow(QStringLiteral("0x0002")));
    model.setWindows(reordered);

    QCOMPARE(model.at(model.selectedIndex()).windowId.value, QStringLiteral("0x0003"));
}

void TestAltTabWindowModel::testInsertionBeforeSelectionKeepsWindowId() {
    AltTabWindowModel model;
    model.setWindows({makeWindow(QStringLiteral("0x0001")), makeWindow(QStringLiteral("0x0003"))});
    model.setSelectedIndex(1);

    model.setWindows({makeWindow(QStringLiteral("0x0001")), makeWindow(QStringLiteral("0x0002")), makeWindow(QStringLiteral("0x0003"))});
    QCOMPARE(model.at(model.selectedIndex()).windowId.value, QStringLiteral("0x0003"));
}

void TestAltTabWindowModel::testRemovalBeforeSelectionKeepsWindowId() {
    AltTabWindowModel model;
    model.setWindows({makeWindow(QStringLiteral("0x0001")), makeWindow(QStringLiteral("0x0002")), makeWindow(QStringLiteral("0x0003"))});
    model.setSelectedIndex(2);

    model.setWindows({makeWindow(QStringLiteral("0x0002")), makeWindow(QStringLiteral("0x0003"))});
    QCOMPARE(model.at(model.selectedIndex()).windowId.value, QStringLiteral("0x0003"));
}

void TestAltTabWindowModel::testSelectedRowRemovedFallsBackNearOldIndex() {
    AltTabWindowModel model;
    model.setWindows({makeWindow(QStringLiteral("0x0001")), makeWindow(QStringLiteral("0x0002")), makeWindow(QStringLiteral("0x0003"))});
    model.setSelectedIndex(1);

    model.setWindows({makeWindow(QStringLiteral("0x0001")), makeWindow(QStringLiteral("0x0003"))});
    QCOMPARE(model.selectedIndex(), 1);
    QCOMPARE(model.at(model.selectedIndex()).windowId.value, QStringLiteral("0x0003"));
}

void TestAltTabWindowModel::testIdenticalSnapshotNoBroadcast() {
    AltTabWindowModel model;

    QVector<WindowInfo> windows;
    windows.append(makeWindow(QStringLiteral("0x0001")));
    windows.append(makeWindow(QStringLiteral("0x0002")));
    model.setWindows(windows);

    QSignalSpy dataSpy(&model, &AltTabWindowModel::dataChanged);

    model.setWindows(windows);
    QCOMPARE(dataSpy.count(), 0);
}

void TestAltTabWindowModel::testCountChangedOnlyOnCountChange()
{
    AltTabWindowModel model;
    QSignalSpy countSpy(&model, &AltTabWindowModel::countChanged);

    model.setWindows({makeWindow(QStringLiteral("0x1")), makeWindow(QStringLiteral("0x2"))});
    QCOMPARE(countSpy.count(), 1);

    // Same count, reorder
    QVector<WindowInfo> reordered;
    reordered.append(makeWindow(QStringLiteral("0x2")));
    reordered.append(makeWindow(QStringLiteral("0x1")));
    model.setWindows(reordered);
    // Should NOT emit countChanged - count didn't change
    QCOMPARE(countSpy.count(), 1);

    // Different count
    model.setWindows({makeWindow(QStringLiteral("0x1"))});
    QCOMPARE(countSpy.count(), 2);
}

void TestAltTabWindowModel::testInsertThenUpdate()
{
    AltTabWindowModel model;
    model.setWindows({makeWindow(QStringLiteral("0x1")), makeWindow(QStringLiteral("0x3"))});
    model.setSelectedIndex(1);

    QSignalSpy dataSpy(&model, &AltTabWindowModel::dataChanged);

    WindowInfo updated = model.at(1);
    updated.iconName = QStringLiteral("test-icon");
    model.updateWindow(updated);

    QVERIFY(dataSpy.count() >= 1);
    QCOMPARE(model.data(model.index(1), AltTabWindowModel::IconNameRole).toString(), QStringLiteral("test-icon"));
}

void TestAltTabWindowModel::testReordering()
{
    AltTabWindowModel model;

    QVector<WindowInfo> windows;
    windows.append(makeWindow(QStringLiteral("0x1")));
    windows.append(makeWindow(QStringLiteral("0x2")));
    windows.append(makeWindow(QStringLiteral("0x3")));
    model.setWindows(windows);
    model.setSelectedIndex(2); // select 0x3

    QSignalSpy countSpy(&model, &AltTabWindowModel::countChanged);

    // Reorder: 0x3, 0x1, 0x2
    QVector<WindowInfo> reordered;
    reordered.append(makeWindow(QStringLiteral("0x3")));
    reordered.append(makeWindow(QStringLiteral("0x1")));
    reordered.append(makeWindow(QStringLiteral("0x2")));
    model.setWindows(reordered);

    // Count unchanged
    QCOMPARE(countSpy.count(), 0);
    // Selection preserved by ID
    QCOMPARE(model.at(model.selectedIndex()).windowId.value, QStringLiteral("0x3"));
    QCOMPARE(model.selectedIndex(), 0);
}

void TestAltTabWindowModel::testSelectedRowRemoval()
{
    AltTabWindowModel model;
    model.setWindows({
        makeWindow(QStringLiteral("0x1")),
        makeWindow(QStringLiteral("0x2")),
        makeWindow(QStringLiteral("0x3"))
    });
    model.setSelectedIndex(1); // select 0x2

    model.removeWindow(QStringLiteral("0x2"));
    QVERIFY(model.selectedIndex() >= 0);
    QVERIFY(model.selectedIndex() < model.count());
}

QTEST_MAIN(TestAltTabWindowModel)
#include "AltTabWindowModelTest.moc"
