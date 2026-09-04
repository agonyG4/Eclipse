#include "statusnotifier/DBusMenuModel.hpp"
#include "statusnotifier/StatusNotifierIconStore.hpp"
#include "statusnotifier/StatusNotifierService.hpp"
#include "statusnotifier/StatusNotifierItemModel.hpp"
#include "statusnotifier/StatusNotifierItemProxy.hpp"
#include "statusnotifier/StatusNotifierTypes.hpp"
#include "statusnotifier/StatusNotifierWatcherBridge.hpp"

#include <QSignalSpy>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace Astrea::StatusNotifier;

class StatusNotifierTest final : public QObject {
    Q_OBJECT

private slots:
    void registrationFormsNormalize();
    void dbusServiceNamesUseProtocolSyntax();
    void malformedRegistrationsAreRejected();
    void argb32NetworkPixelsDecodeExactly();
    void iconSelectionPrefersLargerThenSmaller();
    void iconSelectionPrefersThemeNamesAndFallsBackPerEntry();
    void iconSelectionFindsItemLocalFiles();
    void iconRevisionInvalidatesProviderSource();
    void menuLabelsAndBoundsAreSafe();
    void menuLiveUpdatesKeepLimitsAndRemovedPropertiesResetState();
    void lazySubmenusAdvertiseChildrenBeforeTheyLoad();
    void emptyMenuRootIsValid();
    void menuWireShapeAndSubtreesArePreserved();
    void menuLayoutValueAcceptsRootAndFullReplyShapes();
    void menuModelExposesNestedChildren();
    void serviceKeepsPassiveItemsAndHealthIsSafe();
    void testWatcherDeduplicatesAndRecoversAddresses();
    void localHostOwnershipExpires();
    void watcherAuthorityPrefersFreedesktopOnAliasConflict();
    void watcherStartAndStopAreNonBlocking();
    void stoppedProxyIgnoresQueuedStatus();
    void cumulativeNodeLimitRejectsSubtree();
    void cumulativeDepthLimitRejectsSubtree();
    void acceptedSubtreeReplacementUpdatesLiveTree();
    void displayTitleUsesProductionFallbackOrder();
    void presentationRevisionTracksVisibleStateChanges();
    void serviceProjectsSnapshotsAtomically();
    void metadataSnapshotsDoNotChurnIconProjection();
    void removalPublishesOneEmptyProjection();
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

void StatusNotifierTest::dbusServiceNamesUseProtocolSyntax()
{
    QVERIFY(isValidDBusServiceName(QStringLiteral("org.example.Application")));
    QVERIFY(isValidDBusServiceName(QStringLiteral("org.kde.SomeService")));
    QVERIFY(isValidDBusServiceName(QStringLiteral("org.example-legacy.Application")));
    QVERIFY(isValidDBusServiceName(QStringLiteral("org._7_zip.Archiver")));
    QVERIFY(isValidDBusServiceName(QStringLiteral(":1.42")));
    QVERIFY(isValidDBusServiceName(QStringLiteral(":1.105")));
    QVERIFY(isValidDBusServiceName(QStringLiteral(":abc.1")));
    QVERIFY(isValidDBusServiceName(QStringLiteral(":1.application")));
    QVERIFY(isValidDBusServiceName(QStringLiteral(":foo-bar._7")));
    QVERIFY(!isValidDBusServiceName(QStringLiteral(":1")));
    QVERIFY(!isValidDBusServiceName(QStringLiteral("org..example")));
    QVERIFY(!isValidDBusServiceName(QStringLiteral(".example")));
    QVERIFY(!isValidDBusServiceName(QStringLiteral("org.example.")));
    QVERIFY(!isValidDBusServiceName(QStringLiteral("7.example.Application")));
    QVERIFY(!isValidDBusServiceName(QStringLiteral("org.$example.Application")));
    QVERIFY(!isValidDBusServiceName(QStringLiteral("org.example")
                                    + QString(250, QLatin1Char('x'))));

    QVERIFY(isValidDBusObjectPath(QStringLiteral("/")));
    QVERIFY(isValidDBusObjectPath(QStringLiteral("/org/example")
                                  + QString(180, QLatin1Char('x'))));
    QVERIFY(!isValidDBusObjectPath(QStringLiteral("/org//example")));
    QVERIFY(!isValidDBusObjectPath(QStringLiteral("/org/example/")));
    QVERIFY(!isValidDBusObjectPath(QStringLiteral("relative/path")));

    const auto pathOnly = normalizeRegistration(QStringLiteral("/org/example/Tray"),
                                                 QStringLiteral(":1.42"));
    QVERIFY(pathOnly.isValid());
    QCOMPARE(pathOnly.service, QStringLiteral(":1.42"));
    QCOMPARE(pathOnly.objectPath, QStringLiteral("/org/example/Tray"));
}

void StatusNotifierTest::malformedRegistrationsAreRejected()
{
    QString error;
    QVERIFY(!normalizeRegistration({}, {}, &error).isValid());
    QVERIFY(!error.isEmpty());
    error.clear();
    QVERIFY(!normalizeRegistration(QStringLiteral("not a service"), {}, &error).isValid());
    QVERIFY(!error.isEmpty());
    error.clear();
    QVERIFY(!normalizeRegistration(QStringLiteral("org.example.Service/"), {}, &error)
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

void StatusNotifierTest::iconSelectionPrefersThemeNamesAndFallsBackPerEntry()
{
    QTemporaryDir themeDir;
    QVERIFY(themeDir.isValid());
    QVERIFY(QDir(themeDir.path()).mkpath(QStringLiteral("16x16")));

    QImage named(16, 16, QImage::Format_ARGB32);
    named.fill(qRgba(255, 0, 0, 255));
    QVERIFY(named.save(QDir(themeDir.path()).filePath(QStringLiteral("16x16/normal.png"))));

    QImage attention(16, 16, QImage::Format_ARGB32);
    attention.fill(qRgba(0, 255, 0, 255));
    QVERIFY(attention.save(QDir(themeDir.path()).filePath(QStringLiteral("16x16/attention.png"))));

    QImage overlay(16, 16, QImage::Format_ARGB32);
    overlay.fill(qRgba(255, 255, 0, 255));
    QVERIFY(overlay.save(QDir(themeDir.path()).filePath(QStringLiteral("16x16/overlay.png"))));

    const QByteArray pixmapBytes(16 * 16 * 4, '\x02');
    ItemSnapshot snapshot;
    snapshot.address = {QStringLiteral("org.example.IconPriority"),
                        QStringLiteral("/StatusNotifierItem"), QStringLiteral(":1.6")};
    snapshot.iconName = QStringLiteral("normal");
    snapshot.attentionIconName = QStringLiteral("attention");
    snapshot.overlayIconName = QStringLiteral("overlay");
    snapshot.iconThemePath = themeDir.path();
    snapshot.pixmaps = {{16, 16, pixmapBytes}};
    snapshot.attentionPixmaps = {{16, 16, pixmapBytes}};
    snapshot.overlayPixmaps = {{16, 16, pixmapBytes}};
    snapshot.status = ItemStatus::Active;

    StatusNotifierIconStore store;
    store.updateItem(snapshot);
    const QString key = snapshot.address.key();
    ItemSnapshot other = snapshot;
    other.address = {QStringLiteral("org.example.IconPriorityOther"),
                     QStringLiteral("/StatusNotifierItem"), QStringLiteral(":1.7")};
    other.iconName.clear();
    other.attentionIconName.clear();
    other.overlayIconName.clear();
    other.overlayPixmaps.clear();
    store.updateItem(other);
    const quint64 otherRevision = store.revision(other.address.key());
    snapshot.overlayIconName.clear();
    snapshot.overlayPixmaps.clear();
    store.updateItem(snapshot);
    QCOMPARE(store.image(key).pixel(0, 0), qRgba(255, 0, 0, 255));
    QCOMPARE(store.revision(other.address.key()), otherRevision);

    snapshot.status = ItemStatus::NeedsAttention;
    snapshot.iconName.clear();
    store.updateItem(snapshot);
    QCOMPARE(store.image(key).pixel(0, 0), qRgba(0, 255, 0, 255));

    snapshot.attentionIconName.clear();
    store.updateItem(snapshot);
    QCOMPARE(store.image(key).pixel(0, 0), qRgba(2, 2, 2, 2));
}

void StatusNotifierTest::iconSelectionFindsItemLocalFiles()
{
    QTemporaryDir themeDir;
    QVERIFY(themeDir.isValid());

    QImage direct(16, 16, QImage::Format_ARGB32);
    direct.fill(qRgba(0x12, 0x34, 0x56, 0xff));
    QVERIFY(direct.save(QDir(themeDir.path()).filePath(QStringLiteral("direct.png"))));

    const QString svgPath = QDir(themeDir.path()).filePath(QStringLiteral("symbolic.svg"));
    QFile svg(svgPath);
    QVERIFY(svg.open(QIODevice::WriteOnly));
    QVERIFY(svg.write(QByteArrayLiteral(
                "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" height=\"16\">"
                "<rect width=\"16\" height=\"16\" fill=\"#abcdef\"/>"
                "</svg>")) > 0);
    svg.close();

    StatusNotifierIconStore store;
    ItemSnapshot snapshot;
    snapshot.address = {QStringLiteral("org.example.ItemLocalIcons"),
                        QStringLiteral("/StatusNotifierItem"), QStringLiteral(":1.8")};
    snapshot.iconThemePath = themeDir.path();
    snapshot.iconName = QStringLiteral("direct");
    store.updateItem(snapshot);
    QCOMPARE(store.image(snapshot.address.key()).pixel(0, 0), qRgba(0x12, 0x34, 0x56, 0xff));

    snapshot.iconName = QStringLiteral("symbolic");
    store.updateItem(snapshot);
    const QImage symbolic = store.image(snapshot.address.key());
    QVERIFY(!symbolic.isNull());
    QCOMPARE(symbolic.pixel(0, 0), qRgba(0xab, 0xcd, 0xef, 0xff));

    QImage transparent(16, 16, QImage::Format_ARGB32);
    transparent.fill(Qt::transparent);
    QVERIFY(transparent.save(QDir(themeDir.path()).filePath(QStringLiteral("transparent.png"))));
    snapshot.iconName = QStringLiteral("transparent");
    store.updateItem(snapshot);
    QVERIFY(store.hasIcon(snapshot.address.key()));
    QVERIFY(!store.image(snapshot.address.key()).isNull());
    QCOMPARE(qAlpha(store.image(snapshot.address.key()).pixel(0, 0)), 0);
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
    const QString encodedKey = source.mid(QStringLiteral("image://astrea-tray/").size())
        .section(QLatin1Char('?'), 0, 0);
    QCOMPARE(QUrl::fromPercentEncoding(encodedKey.toUtf8()), snapshot.address.key());
    store.updateItem(snapshot);
    QVERIFY(store.revision(snapshot.address.key()) > first);
    QVERIFY(source != store.imageSource(snapshot.address.key()));
}

void StatusNotifierTest::menuLiveUpdatesKeepLimitsAndRemovedPropertiesResetState()
{
    DBusMenuModel model;
    DBusMenuNode root;
    DBusMenuNode node;
    node.id = 7;
    node.label = QStringLiteral("Checked");
    node.iconName = QStringLiteral("old-icon");
    node.iconData = QByteArrayLiteral("old-data");
    node.toggleType = QStringLiteral("checkmark");
    node.childrenDisplay = QStringLiteral("submenu");
    node.state = 1;
    node.enabled = false;
    node.visible = false;
    root.children = {node};
    model.setRoot(root);

    const QByteArray oversized(1024 * 1024 + 1, '\x01');
    DBusMenuPropertyUpdate update;
    update.id = 7;
    update.properties = {{QStringLiteral("label"), QStringLiteral("Updated")},
                         {QStringLiteral("icon-data"), oversized}};
    QVERIFY(model.updateNodeProperties({update}, {}));
    const DBusMenuNode afterUpdate = model.nodeById(7);
    QCOMPARE(afterUpdate.label, QStringLiteral("Updated"));
    QVERIFY(afterUpdate.iconData.isEmpty());

    DBusMenuRemovedProperties removed;
    removed.id = 7;
    removed.properties = {QStringLiteral("label"), QStringLiteral("icon-name"),
                          QStringLiteral("icon-data"), QStringLiteral("toggle-type"),
                          QStringLiteral("toggle-state"), QStringLiteral("children-display"),
                          QStringLiteral("enabled"), QStringLiteral("visible")};
    QVERIFY(model.updateNodeProperties({}, {removed}));
    const DBusMenuNode afterRemoval = model.nodeById(7);
    QCOMPARE(afterRemoval.label, QString());
    QCOMPARE(afterRemoval.iconName, QString());
    QVERIFY(afterRemoval.iconData.isEmpty());
    QCOMPARE(afterRemoval.toggleType, QString());
    QCOMPARE(afterRemoval.state, 0);
    QCOMPARE(afterRemoval.childrenDisplay, QString());
    QVERIFY(afterRemoval.enabled);
    QVERIFY(afterRemoval.visible);
}

void StatusNotifierTest::lazySubmenusAdvertiseChildrenBeforeTheyLoad()
{
    DBusMenuModel model;
    DBusMenuNode root;
    root.children = {{10, QStringLiteral("Lazy"), {}, {}, {},
                      QStringLiteral("submenu"), 0, true, true, false, {}, {}, {}}};
    model.setRoot(root);
    QVERIFY(model.data(model.index(0, 0), DBusMenuModel::HasChildrenRole).toBool());
    QVERIFY(model.childModel(10) == nullptr);
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

    DBusMenuLayoutNodeWire malformedIcon;
    malformedIcon.id = 3;
    malformedIcon.properties = {{QStringLiteral("label"), QStringLiteral("Keep me")},
                                {QStringLiteral("icon-data"), QByteArrayLiteral("not-an-image")}};
    DBusMenuLayoutReply iconReply;
    iconReply.root.children = {malformedIcon};
    const auto malformed = parseMenuLayout(QVariant::fromValue(iconReply));
    QVERIFY(malformed.ok());
    QCOMPARE(malformed.root.children.constFirst().label, QStringLiteral("Keep me"));
    QVERIFY(malformed.root.children.constFirst().iconData.isEmpty());
}

void StatusNotifierTest::emptyMenuRootIsValid()
{
    DBusMenuLayoutReply reply;
    reply.revision = 3;
    reply.root.id = 0;
    const auto parsed = parseMenuLayout(QVariant::fromValue(reply));
    QVERIFY(parsed.ok());
    QCOMPARE(parsed.revision, quint32(3));
    QCOMPARE(parsed.root.id, 0);
    QVERIFY(parsed.root.children.isEmpty());
}

void StatusNotifierTest::menuWireShapeAndSubtreesArePreserved()
{
    registerDBusMenuMetaTypes();
    DBusMenuLayoutReply reply;
    reply.revision = 12;
    reply.root.id = 0;
    DBusMenuLayoutNodeWire tools;
    tools.id = 10;
    tools.properties = {{QStringLiteral("label"), QStringLiteral("_Tools")}};
    DBusMenuLayoutNodeWire preferences;
    preferences.id = 11;
    preferences.properties = {{QStringLiteral("label"), QStringLiteral("Preferences")}};
    tools.children.append(preferences);
    DBusMenuLayoutNodeWire separator;
    separator.id = 12;
    separator.properties = {{QStringLiteral("type"), QStringLiteral("separator")}};
    reply.root.children = {tools, separator};
    const auto parsed = parseMenuLayout(QVariant::fromValue(reply));
    QVERIFY(parsed.ok());
    QCOMPARE(parsed.revision, quint32(12));
    QCOMPARE(parsed.root.children.constFirst().label, QStringLiteral("Tools"));
    QCOMPARE(parsed.root.children.constFirst().children.constFirst().id, 11);
    QVERIFY(parsed.root.children.at(1).separator);

    DBusMenuModel model;
    model.setRoot(parsed.root);
    DBusMenuNode child;
    child.id = 10;
    child.children = {{13, QStringLiteral("About")}};
    QVERIFY(model.replaceSubtree(10, child));
    QCOMPARE(model.childModel(10)->property("objectName").toString(), QString());
    auto *nested = qobject_cast<DBusMenuModel *>(model.childModel(10));
    QVERIFY(nested);
    QCOMPARE(nested->data(nested->index(0, 0), DBusMenuModel::NodeIdRole).toInt(), 13);
}

void StatusNotifierTest::menuLayoutValueAcceptsRootAndFullReplyShapes()
{
    registerDBusMenuMetaTypes();

    DBusMenuLayoutNodeWire root;
    root.id = 0;
    root.children = {{42, {{QStringLiteral("label"), QStringLiteral("Live root")}}, {}}};
    const auto rootParsed = parseMenuLayout(QVariant::fromValue(root));
    QVERIFY(rootParsed.ok());
    QCOMPARE(rootParsed.revision, quint32(0));
    QCOMPARE(rootParsed.root.children.constFirst().id, 42);

    DBusMenuLayoutReply reply;
    reply.revision = 19;
    reply.root = root;
    const auto fullParsed = parseMenuLayout(QVariant::fromValue(reply));
    QVERIFY(fullParsed.ok());
    QCOMPARE(fullParsed.revision, quint32(19));
    QCOMPARE(fullParsed.root.children.constFirst().label, QStringLiteral("Live root"));
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

void StatusNotifierTest::localHostOwnershipExpires()
{
    StatusNotifierLocalWatcherObject watcher;
    QSignalSpy hostChanges(&watcher, &StatusNotifierLocalWatcherObject::hostRegisteredChanged);

    watcher.registerVerifiedHost(QStringLiteral("org.example.Host"), QStringLiteral(":1.42"));
    QVERIFY(watcher.hostRegistered());
    QCOMPARE(hostChanges.count(), 1);

    watcher.removeOwner(QStringLiteral(":1.42"));
    QVERIFY(!watcher.hostRegistered());
    QCOMPARE(hostChanges.count(), 2);
    QCOMPARE(hostChanges.at(1).at(0).toString(), QStringLiteral("org.example.Host"));
    QVERIFY(!hostChanges.at(1).at(1).toBool());

    watcher.registerVerifiedHost(QStringLiteral(":1.43"), QStringLiteral(":1.43"));
    QVERIFY(watcher.hostRegistered());
    watcher.removeOwner(QStringLiteral(":1.43"));
    QVERIFY(!watcher.hostRegistered());
}

void StatusNotifierTest::watcherAuthorityPrefersFreedesktopOnAliasConflict()
{
    const auto authority = StatusNotifierWatcherBridge::selectAuthority(
        QStringLiteral(":1.20"), QStringLiteral(":1.21"));
    QCOMPARE(authority.name, QStringLiteral("org.freedesktop.StatusNotifierWatcher"));
    QCOMPARE(authority.owner, QStringLiteral(":1.20"));
    QVERIFY(authority.conflict);
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

void StatusNotifierTest::stoppedProxyIgnoresQueuedStatus()
{
    const ItemAddress address{QStringLiteral("org.example.Stopped"),
                              QStringLiteral("/StatusNotifierItem"), QStringLiteral(":1.200")};
    StatusNotifierItemProxy proxy(address, 1);
    QSignalSpy snapshots(&proxy, &StatusNotifierItemProxy::snapshotChanged);
    QVERIFY(QMetaObject::invokeMethod(&proxy, "onNewStatus", Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("Active"))));
    QCOMPARE(snapshots.count(), 0);
}

DBusMenuNode menuNode(int id, const QString &label, QList<DBusMenuNode> children = {})
{
    DBusMenuNode node;
    node.id = id;
    node.label = label;
    node.children = std::move(children);
    node.childrenDisplay = node.children.isEmpty() ? QString() : QStringLiteral("submenu");
    return node;
}

void StatusNotifierTest::cumulativeNodeLimitRejectsSubtree()
{
    DBusMenuLimits limits;
    limits.maxNodes = 5;
    DBusMenuModel model(limits);
    DBusMenuNode root;
    root.children = {menuNode(1, QStringLiteral("Branch"), {menuNode(2, QStringLiteral("Old"))}),
                     menuNode(3, QStringLiteral("Other"))};
    model.setRoot(root);

    const DBusMenuNode candidate = menuNode(
        1, QStringLiteral("Branch"),
        {menuNode(10, QStringLiteral("One")), menuNode(11, QStringLiteral("Two")),
         menuNode(12, QStringLiteral("Three"))});
    QCOMPARE(model.replaceSubtreeResult(1, candidate), DBusMenuMutationResult::RejectedByLimits);
    QCOMPARE(model.nodeById(1).children.constFirst().id, 2);
}

void StatusNotifierTest::cumulativeDepthLimitRejectsSubtree()
{
    DBusMenuLimits limits;
    limits.maxDepth = 3;
    DBusMenuModel model(limits);
    DBusMenuNode root;
    root.children = {menuNode(1, QStringLiteral("Branch"), {menuNode(2, QStringLiteral("Old"))})};
    model.setRoot(root);

    const DBusMenuNode candidate = menuNode(
        1, QStringLiteral("Branch"),
        {menuNode(10, QStringLiteral("One"),
                   {menuNode(11, QStringLiteral("Two"),
                              {menuNode(12, QStringLiteral("Three"))})})});
    QCOMPARE(model.replaceSubtreeResult(1, candidate), DBusMenuMutationResult::RejectedByLimits);
    QCOMPARE(model.nodeById(1).children.constFirst().id, 2);
}

void StatusNotifierTest::acceptedSubtreeReplacementUpdatesLiveTree()
{
    DBusMenuLimits limits;
    limits.maxNodes = 8;
    limits.maxDepth = 4;
    DBusMenuModel model(limits);
    DBusMenuNode root;
    root.children = {menuNode(1, QStringLiteral("Branch"), {menuNode(2, QStringLiteral("Old"))})};
    model.setRoot(root);

    const DBusMenuNode candidate = menuNode(
        1, QStringLiteral("Branch"), {menuNode(10, QStringLiteral("New"))});
    QCOMPARE(model.replaceSubtreeResult(1, candidate), DBusMenuMutationResult::Applied);
    QCOMPARE(model.nodeById(1).children.constFirst().id, 10);
    QVERIFY(model.childModel(1) != nullptr);
}

void StatusNotifierTest::displayTitleUsesProductionFallbackOrder()
{
    StatusNotifierService service;
    ItemSnapshot snapshot;
    snapshot.address = {QStringLiteral("org.example.Title"),
                        QStringLiteral("/StatusNotifierItem"), QStringLiteral(":1.210")};
    snapshot.id = QStringLiteral("Fallback id");
    snapshot.title = QStringLiteral("Fallback title");
    snapshot.tooltipTitle = QStringLiteral("Fallback tooltip");
    snapshot.generation = 7;

    service.upsertTestItem(snapshot);
    QCOMPARE(service.displayTitleForItem(snapshot.address.key()),
             QStringLiteral("Fallback tooltip"));

    snapshot.tooltipTitle.clear();
    service.upsertTestItem(snapshot);
    QCOMPARE(service.displayTitleForItem(snapshot.address.key()),
             QStringLiteral("Fallback title"));

    snapshot.title.clear();
    service.upsertTestItem(snapshot);
    QCOMPARE(service.displayTitleForItem(snapshot.address.key()), QStringLiteral("Fallback id"));

    snapshot.id.clear();
    service.upsertTestItem(snapshot);
    QCOMPARE(service.displayTitleForItem(snapshot.address.key()), QStringLiteral("Tray item"));
}

void StatusNotifierTest::presentationRevisionTracksVisibleStateChanges()
{
    StatusNotifierService service;
    ItemSnapshot snapshot;
    snapshot.address = {QStringLiteral("org.example.Revision"),
                        QStringLiteral("/StatusNotifierItem"), QStringLiteral(":1.211")};
    snapshot.id = QStringLiteral("revision");
    snapshot.title = QStringLiteral("Initial title");
    snapshot.generation = 11;

    const quint64 initialRevision = service.presentationRevision();
    QCOMPARE(service.presentationRevision(), initialRevision);
    QCOMPARE(service.itemCount(), 0);
    QCOMPARE(service.presentationRevision(), initialRevision);

    service.upsertTestItem(snapshot);
    const quint64 afterSnapshot = service.presentationRevision();
    QVERIFY(afterSnapshot > initialRevision);

    service.hasMenuForItem(snapshot.address.key());
    service.menuModelForItem(snapshot.address.key());
    service.menuStateForItem(snapshot.address.key());
    service.displayTitleForItem(snapshot.address.key());
    service.iconSourceForItem(snapshot.address.key());
    service.tooltipTitleForItem(snapshot.address.key());
    service.tooltipDescriptionForItem(snapshot.address.key());
    service.healthJson();
    QCOMPARE(service.presentationRevision(), afterSnapshot);

    snapshot.title = QStringLiteral("Updated title");
    service.upsertTestItem(snapshot);
    const quint64 afterTitle = service.presentationRevision();
    QVERIFY(afterTitle > afterSnapshot);

    snapshot.menuPath = QStringLiteral("/MenuA");
    service.upsertTestItem(snapshot);
    const quint64 afterMenuA = service.presentationRevision();
    QVERIFY(afterMenuA > afterTitle);
    QObject *menuA = service.menuModelForItem(snapshot.address.key());
    QVERIFY(menuA != nullptr);

    snapshot.menuPath = QStringLiteral("/MenuB");
    service.upsertTestItem(snapshot);
    const quint64 afterMenuB = service.presentationRevision();
    QVERIFY(afterMenuB > afterMenuA);
    QVERIFY(service.menuModelForItem(snapshot.address.key()) != nullptr);
    QVERIFY(service.menuModelForItem(snapshot.address.key()) != menuA);

    snapshot.menuPath.clear();
    service.upsertTestItem(snapshot);
    const quint64 afterMenuRemoval = service.presentationRevision();
    QVERIFY(afterMenuRemoval > afterMenuB);
    QVERIFY(!service.hasMenuForItem(snapshot.address.key()));
    QCOMPARE(service.menuModelForItem(snapshot.address.key()), nullptr);

    service.iconStore()->updateAuxiliaryImage(snapshot.address.key(), QImage(4, 4,
                                                                              QImage::Format_ARGB32));
    const quint64 afterIcon = service.presentationRevision();
    QVERIFY(afterIcon > afterMenuRemoval);
    QVERIFY(!service.iconSourceForItem(snapshot.address.key()).isEmpty());

    service.removeTestItem(snapshot.address.key());
    QVERIFY(service.presentationRevision() > afterIcon);
    QCOMPARE(service.itemCount(), 0);

    const quint64 beforeStop = service.presentationRevision();
    service.start();
    service.stop();
    QVERIFY(service.presentationRevision() > beforeStop);
    const quint64 afterStop = service.presentationRevision();
    service.start();
    QVERIFY(service.presentationRevision() >= afterStop);
    service.stop();
}

void StatusNotifierTest::serviceProjectsSnapshotsAtomically()
{
    StatusNotifierService service;
    ItemSnapshot snapshot;
    snapshot.address = {QStringLiteral("org.example.Atomic"),
                        QStringLiteral("/StatusNotifierItem"), QStringLiteral(":1.220")};
    snapshot.id = QStringLiteral("atomic");
    snapshot.title = QStringLiteral("Old title");
    snapshot.status = ItemStatus::Active;
    snapshot.pixmaps = {{1, 1, QByteArray::fromHex("ff3366cc")}};
    snapshot.menuPath = QStringLiteral("/MenuA");
    snapshot.generation = 22;
    service.upsertTestItem(snapshot);

    const QString key = snapshot.address.key();
    auto *oldModel = qobject_cast<DBusMenuModel *>(service.menuModelForItem(key));
    QVERIFY(oldModel);
    auto *oldClient = qobject_cast<DBusMenuClient *>(oldModel->parent());
    QVERIFY(oldClient);
    QCOMPARE(oldClient->menuPath(), QStringLiteral("/MenuA"));
    const quint64 oldIconRevision = service.iconStore()->revision(key);
    const QString oldIconSource = service.iconSourceForItem(key);

    struct Observation {
        ItemSnapshot snapshot;
        QString iconSource;
        QImage image;
        DBusMenuModel *menuModel = nullptr;
        DBusMenuClient *menuClient = nullptr;
    };
    QVector<Observation> observations;
    const auto revisionConnection = connect(
        &service, &StatusNotifierService::presentationRevisionChanged, &service,
        [&service, &key, &observations] {
            Observation observation;
            observation.snapshot = service.typedItemModel()->item(key);
            observation.iconSource = service.iconSourceForItem(key);
            observation.image = service.iconStore()->image(key);
            observation.menuModel = qobject_cast<DBusMenuModel *>(service.menuModelForItem(key));
            observation.menuClient = observation.menuModel
                ? qobject_cast<DBusMenuClient *>(observation.menuModel->parent()) : nullptr;
            observations.append(std::move(observation));
        });

    snapshot.title = QStringLiteral("New title");
    snapshot.pixmaps = {{1, 1, QByteArray::fromHex("ffcc6633")}};
    snapshot.menuPath = QStringLiteral("/MenuB");
    service.upsertTestItem(snapshot);

    QCOMPARE(observations.size(), 1);
    const Observation &observation = observations.constFirst();
    QCOMPARE(observation.snapshot.title, QStringLiteral("New title"));
    QCOMPARE(observation.snapshot.menuPath, QStringLiteral("/MenuB"));
    QCOMPARE(observation.iconSource, service.iconSourceForItem(key));
    QVERIFY(observation.iconSource != oldIconSource);
    QCOMPARE(observation.image.pixel(0, 0), qRgba(0xcc, 0x66, 0x33, 0xff));
    QVERIFY(observation.menuModel);
    QVERIFY(observation.menuClient);
    QCOMPARE(observation.menuClient->menuPath(), QStringLiteral("/MenuB"));
    QCOMPARE(observation.menuClient->itemGeneration(), snapshot.generation);
    QVERIFY(observation.menuModel != oldModel);
    QVERIFY(observation.menuClient != oldClient);
    QCOMPARE(service.iconStore()->revision(key), oldIconRevision + 1);

    observations.clear();
    snapshot.status = ItemStatus::NeedsAttention;
    snapshot.attentionPixmaps = {{1, 1, QByteArray::fromHex("ff00ff00")}};
    service.upsertTestItem(snapshot);
    QCOMPARE(observations.size(), 1);
    QCOMPARE(observations.constFirst().image.pixel(0, 0), qRgba(0, 0xff, 0, 0xff));
    QCOMPARE(service.iconStore()->revision(key), oldIconRevision + 2);

    disconnect(revisionConnection);
}

void StatusNotifierTest::metadataSnapshotsDoNotChurnIconProjection()
{
    StatusNotifierService service;
    ItemSnapshot snapshot;
    snapshot.address = {QStringLiteral("org.example.StableIcon"),
                        QStringLiteral("/StatusNotifierItem"), QStringLiteral(":1.221")};
    snapshot.title = QStringLiteral("Title A");
    snapshot.tooltipTitle = QStringLiteral("Tooltip A");
    snapshot.tooltipDescription = QStringLiteral("Description A");
    snapshot.status = ItemStatus::Active;
    snapshot.pixmaps = {{1, 1, QByteArray::fromHex("ff112233")}};
    snapshot.menuPath = QStringLiteral("/MenuA");
    snapshot.itemIsMenu = false;
    snapshot.generation = 23;
    service.upsertTestItem(snapshot);

    const QString key = snapshot.address.key();
    const quint64 iconRevision = service.iconStore()->revision(key);
    const QString iconSource = service.iconSourceForItem(key);
    auto *stableModel = qobject_cast<DBusMenuModel *>(service.menuModelForItem(key));
    QVERIFY(stableModel);
    auto *stableClient = qobject_cast<DBusMenuClient *>(stableModel->parent());
    QVERIFY(stableClient);

    auto assertStableIcon = [&] {
        QCOMPARE(service.iconStore()->revision(key), iconRevision);
        QCOMPARE(service.iconSourceForItem(key), iconSource);
        QCOMPARE(service.menuModelForItem(key), stableModel);
        QCOMPARE(qobject_cast<DBusMenuClient *>(stableModel->parent()), stableClient);
    };

    QSignalSpy revisions(&service, &StatusNotifierService::presentationRevisionChanged);
    snapshot.title = QStringLiteral("Title B");
    service.upsertTestItem(snapshot);
    QCOMPARE(revisions.count(), 1);
    assertStableIcon();

    revisions.clear();
    snapshot.tooltipTitle = QStringLiteral("Tooltip B");
    snapshot.tooltipDescription = QStringLiteral("Description B");
    service.upsertTestItem(snapshot);
    QCOMPARE(revisions.count(), 1);
    assertStableIcon();

    revisions.clear();
    snapshot.itemIsMenu = true;
    service.upsertTestItem(snapshot);
    QCOMPARE(revisions.count(), 1);
    assertStableIcon();

    revisions.clear();
    snapshot.menuPath = QStringLiteral("/MenuB");
    service.upsertTestItem(snapshot);
    QCOMPARE(revisions.count(), 1);
    QCOMPARE(service.iconStore()->revision(key), iconRevision);
    QCOMPARE(service.iconSourceForItem(key), iconSource);
    QVERIFY(service.menuModelForItem(key) != stableModel);

    revisions.clear();
    snapshot.pixmaps = {{1, 1, QByteArray::fromHex("ff332211")}};
    service.upsertTestItem(snapshot);
    QCOMPARE(revisions.count(), 1);
    QVERIFY(service.iconStore()->revision(key) > iconRevision);
    QVERIFY(service.iconSourceForItem(key) != iconSource);
    QCOMPARE(service.iconStore()->image(key).pixel(0, 0), qRgba(0x33, 0x22, 0x11, 0xff));

    revisions.clear();
    service.iconStore()->updateAuxiliaryImage(key, QImage(1, 1, QImage::Format_ARGB32));
    QCOMPARE(revisions.count(), 1);
    QVERIFY(service.iconStore()->revision(key) > iconRevision);
    QVERIFY(!service.iconSourceForItem(key).isEmpty());
    QCOMPARE(service.typedItemModel()->item(key).title, QStringLiteral("Title B"));
}

void StatusNotifierTest::removalPublishesOneEmptyProjection()
{
    StatusNotifierService service;
    ItemSnapshot snapshot;
    snapshot.address = {QStringLiteral("org.example.Remove"),
                        QStringLiteral("/StatusNotifierItem"), QStringLiteral(":1.222")};
    snapshot.title = QStringLiteral("To remove");
    snapshot.pixmaps = {{1, 1, QByteArray::fromHex("ffabcdef")}};
    snapshot.menuPath = QStringLiteral("/MenuA");
    snapshot.generation = 24;
    service.upsertTestItem(snapshot);
    const QString key = snapshot.address.key();
    QVERIFY(service.menuModelForItem(key));

    struct RemovalObservation {
        bool modelContains = true;
        bool iconPresent = true;
        bool menuPresent = true;
    };
    QVector<RemovalObservation> observations;
    const auto revisionConnection = connect(
        &service, &StatusNotifierService::presentationRevisionChanged, &service,
        [&service, &key, &observations] {
            observations.append({service.typedItemModel()->contains(key),
                                 service.iconStore()->hasIcon(key),
                                 service.menuModelForItem(key) != nullptr});
        });

    service.removeTestItem(key);

    QCOMPARE(observations.size(), 1);
    QCOMPARE(observations.constFirst().modelContains, false);
    QCOMPARE(observations.constFirst().iconPresent, false);
    QCOMPARE(observations.constFirst().menuPresent, false);
    QCOMPARE(service.itemCount(), 0);
    QCOMPARE(service.menuClientCount(), 0);
    disconnect(revisionConnection);
}

QTEST_APPLESS_MAIN(StatusNotifierTest)

#include "StatusNotifierTest.moc"
