#include "statusnotifier/DBusMenuModel.hpp"
#include "statusnotifier/StatusNotifierIconStore.hpp"
#include "statusnotifier/StatusNotifierService.hpp"
#include "statusnotifier/StatusNotifierItemModel.hpp"
#include "statusnotifier/StatusNotifierTypes.hpp"
#include "statusnotifier/StatusNotifierWatcherBridge.hpp"

#include <QSignalSpy>
#include <QTest>

using namespace Astrea::StatusNotifier;

class StatusNotifierTest final : public QObject {
    Q_OBJECT

private slots:
    void registrationFormsNormalize();
    void malformedRegistrationsAreRejected();
    void argb32NetworkPixelsDecodeExactly();
    void iconSelectionPrefersLargerThenSmaller();
    void iconRevisionInvalidatesProviderSource();
    void menuLabelsAndBoundsAreSafe();
    void menuModelExposesNestedChildren();
    void serviceKeepsPassiveItemsAndHealthIsSafe();
    void testWatcherDeduplicatesAndRecoversAddresses();
    void watcherStartAndStopAreNonBlocking();
};

void StatusNotifierTest::registrationFormsNormalize()
{
    const auto serviceOnly = normalizeRegistration(QStringLiteral("org.example.Tray"));
    QCOMPARE(serviceOnly.service, QStringLiteral("org.example.Tray"));
    QCOMPARE(serviceOnly.objectPath, QStringLiteral("/StatusNotifierItem"));

    const auto pathOnly = normalizeRegistration(QStringLiteral("/org/example/Tray"),
                                                 QStringLiteral(":1.42"));
    QCOMPARE(pathOnly.service, QStringLiteral(":1.42"));
    QCOMPARE(pathOnly.objectPath, QStringLiteral("/org/example/Tray"));
    QCOMPARE(pathOnly.uniqueOwner, QStringLiteral(":1.42"));

    const auto combined = normalizeRegistration(
        QStringLiteral("com.canonical.AppIndicator/foo/bar"));
    QCOMPARE(combined.service, QStringLiteral("com.canonical.AppIndicator"));
    QCOMPARE(combined.objectPath, QStringLiteral("/foo/bar"));
}

void StatusNotifierTest::malformedRegistrationsAreRejected()
{
    QString error;
    QVERIFY(!normalizeRegistration({}, {}, {}, &error).isValid());
    QVERIFY(!error.isEmpty());
    error.clear();
    QVERIFY(!normalizeRegistration(QStringLiteral("not a service"), {}, {}, &error).isValid());
    QVERIFY(!error.isEmpty());
    error.clear();
    QVERIFY(!normalizeRegistration(QStringLiteral("org.example.Service/"), {}, {}, &error)
                .isValid());
    QVERIFY(!error.isEmpty());
}

void StatusNotifierTest::argb32NetworkPixelsDecodeExactly()
{
    PixmapData data{2, 1, QByteArray::fromHex("80ff0000ff00ff00")};
    const auto result = decodeArgb32NetworkPixmap(data);
    QVERIFY(result.ok());
    QCOMPARE(result.image.pixel(0, 0), qRgba(255, 0, 0, 128));
    QCOMPARE(result.image.pixel(1, 0), qRgba(0, 255, 0, 255));
}

void StatusNotifierTest::iconSelectionPrefersLargerThenSmaller()
{
    StatusNotifierIconStore store;
    ItemSnapshot snapshot;
    snapshot.address = {QStringLiteral("org.example.Tray"), QStringLiteral("/StatusNotifierItem"),
                        QStringLiteral(":1.4")};
    snapshot.pixmaps = {
        {8, 8, QByteArray(8 * 8 * 4, '\x01')},
        {24, 24, QByteArray(24 * 24 * 4, '\x02')},
        {32, 32, QByteArray(32 * 32 * 4, '\x03')},
    };
    store.updateItem(snapshot);
    QCOMPARE(store.image(snapshot.address.key(), QSize(20, 20)).size(), QSize(24, 24));
    QVERIFY(!store.image(snapshot.address.key(), QSize(40, 40)).isNull());
}

void StatusNotifierTest::iconRevisionInvalidatesProviderSource()
{
    StatusNotifierIconStore store;
    ItemSnapshot snapshot;
    snapshot.address = {QStringLiteral("org.example.Tray"), QStringLiteral("/StatusNotifierItem"),
                        QStringLiteral(":1.5")};
    snapshot.iconName = QStringLiteral("does-not-exist");
    store.updateItem(snapshot);
    const quint64 first = store.revision(snapshot.address.key());
    const QString source = store.imageSource(snapshot.address.key());
    store.updateItem(snapshot);
    QVERIFY(store.revision(snapshot.address.key()) > first);
    QVERIFY(source != store.imageSource(snapshot.address.key()));
}

void StatusNotifierTest::menuLabelsAndBoundsAreSafe()
{
    const QVariantMap node{
        {QStringLiteral("id"), 0},
        {QStringLiteral("properties"), QVariantMap{{QStringLiteral("label"), QStringLiteral("_File")}}},
        {QStringLiteral("children"), QVariantList{QVariantMap{
            {QStringLiteral("id"), 1},
            {QStringLiteral("properties"), QVariantMap{{QStringLiteral("label"), QStringLiteral("Sa__ve")}}}
        }}}
    };
    const auto parsed = parseMenuLayout(QVariantList{quint32(7), node});
    QVERIFY(parsed.ok());
    QCOMPARE(parsed.revision, quint32(7));
    QCOMPARE(parsed.root.label, QStringLiteral("File"));
    QCOMPARE(parsed.root.children.constFirst().label, QStringLiteral("Sa_ve"));

    DBusMenuLimits limits;
    limits.maxChildren = 0;
    const auto rejected = parseMenuLayout(QVariantList{quint32(7), node}, limits);
    QVERIFY(!rejected.ok());
}

void StatusNotifierTest::menuModelExposesNestedChildren()
{
    DBusMenuModel model;
    DBusMenuNode root;
    root.children = {{1, QStringLiteral("Actions"), {}, {}, {}, QStringLiteral("submenu"), 0,
                      true, true, false, {{2, QStringLiteral("Open")}}}};
    model.setRoot(root);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), DBusMenuModel::LabelRole).toString(),
             QStringLiteral("Actions"));
    QVERIFY(model.data(model.index(0, 0), DBusMenuModel::HasChildrenRole).toBool());
    QVERIFY(model.childModel(1));
    QCOMPARE(model.childModel(1)->property("objectName").toString(), QString());
}

void StatusNotifierTest::serviceKeepsPassiveItemsAndHealthIsSafe()
{
    StatusNotifierService service;
    ItemSnapshot snapshot;
    snapshot.address = {QStringLiteral("org.example.Passive"), QStringLiteral("/StatusNotifierItem"),
                        QStringLiteral(":1.7")};
    snapshot.id = QStringLiteral("passive");
    snapshot.status = ItemStatus::Passive;
    snapshot.ready = true;
    service.upsertTestItem(snapshot);
    QCOMPARE(service.itemCount(), 1);
    QCOMPARE(service.typedItemModel()->data(service.typedItemModel()->index(0, 0),
                                            StatusNotifierItemModel::StatusRole).toString(),
             QStringLiteral("Passive"));
    QVERIFY(service.healthJson().contains(QStringLiteral("mode")));
}

void StatusNotifierTest::testWatcherDeduplicatesAndRecoversAddresses()
{
    StatusNotifierWatcherBridge bridge;
    QSignalSpy registered(&bridge, &StatusNotifierWatcherBridge::itemRegistered);
    QSignalSpy unregistered(&bridge, &StatusNotifierWatcherBridge::itemUnregistered);
    const ItemAddress address{QStringLiteral("org.example.Restart"),
                              QStringLiteral("/StatusNotifierItem"), QStringLiteral(":1.99")};
    bridge.registerTestAddress(address);
    bridge.registerTestAddress(address);
    QCOMPARE(registered.count(), 1);
    bridge.unregisterTestKey(address.key());
    QCOMPARE(unregistered.count(), 1);
    bridge.registerTestAddress(address);
    QCOMPARE(registered.count(), 2);
}

void StatusNotifierTest::watcherStartAndStopAreNonBlocking()
{
    StatusNotifierWatcherBridge bridge;
    bridge.start();
    QTest::qWait(30);
    QVERIFY(bridge.mode() == WatcherMode::Unavailable
            || bridge.mode() == WatcherMode::External
            || bridge.mode() == WatcherMode::Owned);
    bridge.stop();
    QCOMPARE(bridge.mode(), WatcherMode::Unavailable);
}

QTEST_APPLESS_MAIN(StatusNotifierTest)

#include "StatusNotifierTest.moc"
