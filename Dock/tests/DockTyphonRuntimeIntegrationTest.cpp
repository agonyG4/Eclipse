#include <QTest>

#include "core/DockController.hpp"
#include "platform/typhon/TyphonToplevelConnection.hpp"

using namespace Astrea::Typhon;

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

    int starts = 0;
    int stops = 0;
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

QTEST_GUILESS_MAIN(DockTyphonRuntimeIntegrationTest)
#include "DockTyphonRuntimeIntegrationTest.moc"
