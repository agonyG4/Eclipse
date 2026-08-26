#include "platform/ipc/WallpaperControlServer.hpp"

#include "core/WallpaperCatalog.hpp"
#include "core/WallpaperPersistence.hpp"
#include "core/WallpaperService.hpp"

#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <memory>
#include <vector>

using namespace Paper;

class WallpaperControlServerTest final : public QObject
{
    Q_OBJECT

private slots:
    void createsUserRuntimeEndpoint();
    void handlesGetSetDefaultAndReset();
    void listsCatalogAndSelectsStableId();
    void importsThroughPaperEndpoint();
    void addsToCatalogWithoutChangingCurrentWallpaper();
    void failedSecondServerDoesNotRemoveLiveEndpoint();
    void boundsIdleClientsAndReclaimsCapacity();
    void rejectsMalformedUnknownAndOversizedRequests();
    void preservesUnicodeSpacesAndInjectionShapedSourceAsData();

private:
    static QString writeImage(const QString &path);
    static QJsonObject request(const QString &endpoint, const QString &line);
};

QString WallpaperControlServerTest::writeImage(const QString &path)
{
    QImage image(20, 20, QImage::Format_ARGB32);
    image.fill(Qt::yellow);
    if (!image.save(path)) {
        qFatal("Could not create test image at %s", qPrintable(path));
    }
    return path;
}

QJsonObject WallpaperControlServerTest::request(const QString &endpoint, const QString &line)
{
    const auto reply = WallpaperControlServer::requestReply(endpoint, line, 1000);
    return QJsonDocument::fromJson(reply).object();
}

void WallpaperControlServerTest::createsUserRuntimeEndpoint()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             std::make_unique<XdgWallpaperPersistence>(
                                 temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();
    const auto endpoint = temp.filePath(QStringLiteral("r/w.sock"));
    WallpaperControlServer server(&service, endpoint);
    QString error;
    QVERIFY2(server.listen(&error), qPrintable(error));
    const QFileInfo parentInfo(QFileInfo(endpoint).absolutePath());
    QVERIFY(parentInfo.isDir());
    QCOMPARE(parentInfo.permissions() & QFileDevice::WriteOther, QFileDevice::Permissions());
    QCOMPARE(parentInfo.permissions() & QFileDevice::ReadOther, QFileDevice::Permissions());
    QCOMPARE(parentInfo.permissions() & QFileDevice::ExeOther, QFileDevice::Permissions());
    QVERIFY(QFileInfo::exists(endpoint));
}

void WallpaperControlServerTest::handlesGetSetDefaultAndReset()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto selected = writeImage(temp.filePath(QStringLiteral("selected.png")));
    auto catalog = std::make_shared<WallpaperCatalog>(
        WallpaperResolver(factory, emergency),
        temp.filePath(QStringLiteral("library")),
        temp.filePath(QStringLiteral("system")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             std::make_unique<XdgWallpaperPersistence>(
                                 temp.filePath(QStringLiteral("paper.ini"))),
                             catalog);
    service.initialize();
    const auto endpoint = temp.filePath(QStringLiteral("r/w.sock"));
    WallpaperControlServer server(&service, endpoint);
    QVERIFY(server.listen());

    const auto initial = request(endpoint, QStringLiteral("wallpaper get {}"));
    QCOMPARE(initial.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(initial.value(QStringLiteral("snapshot")).toObject()
                 .value(QStringLiteral("effective")).toObject()
                 .value(QStringLiteral("logicalId"))
                 .toString(),
             QStringLiteral("astrea://wallpaper/default"));

    const auto set = request(endpoint,
                             QStringLiteral("wallpaper set {\"source\":\"%1\",\"fit\":\"contain\"}")
                                 .arg(selected));
    QCOMPARE(set.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(set.value(QStringLiteral("completed")).toBool(), true);
    QCOMPARE(set.value(QStringLiteral("snapshot")).toObject().value(QStringLiteral("state")),
             QStringLiteral("ready"));
    const auto imported = set.value(QStringLiteral("snapshot")).toObject()
                              .value(QStringLiteral("effective")).toObject();
    QVERIFY(imported.value(QStringLiteral("logicalId")).toString()
            .startsWith(QStringLiteral("astrea://wallpaper/user/")));
    QVERIFY(imported.value(QStringLiteral("source")).toString() != selected);
    QVERIFY(QFileInfo::exists(imported.value(QStringLiteral("source")).toString()));

    const auto invalid = request(
        endpoint,
        QStringLiteral("wallpaper set {\"source\":\"%1\",\"fit\":\"cover\"}")
            .arg(temp.filePath(QStringLiteral("missing.png"))));
    QCOMPARE(invalid.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(invalid.value(QStringLiteral("completed")).toBool(), true);
    QCOMPARE(invalid.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("unsupported-image"));
    QCOMPARE(service.snapshot().effective.source(),
             imported.value(QStringLiteral("source")).toString());

    const auto reset = request(endpoint, QStringLiteral("wallpaper reset {}"));
    QCOMPARE(reset.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(service.snapshot().configured.has_value(), false);

    const auto defaultReply = request(endpoint, QStringLiteral("wallpaper default {}"));
    QCOMPARE(defaultReply.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(defaultReply.value(QStringLiteral("snapshot")).toObject()
                 .value(QStringLiteral("factoryDefault")).toObject()
                 .value(QStringLiteral("logicalId"))
                 .toString(),
             QStringLiteral("astrea://wallpaper/default"));
}

void WallpaperControlServerTest::listsCatalogAndSelectsStableId()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    auto catalog = std::make_shared<WallpaperCatalog>(
        WallpaperResolver(factory, emergency), temp.filePath(QStringLiteral("library")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             std::make_unique<XdgWallpaperPersistence>(
                                 temp.filePath(QStringLiteral("paper.ini"))),
                             catalog);
    service.initialize();
    const auto endpoint = temp.filePath(QStringLiteral("r/w.sock"));
    WallpaperControlServer server(&service, endpoint);
    QVERIFY(server.listen());

    const auto listed = request(endpoint, QStringLiteral("wallpaper list {}"));
    QCOMPARE(listed.value(QStringLiteral("ok")).toBool(), true);
    const auto wallpapers = listed.value(QStringLiteral("wallpapers")).toArray();
    QVERIFY(!wallpapers.isEmpty());
    bool foundDefault = false;
    for (const auto &entry : wallpapers) {
        if (entry.toObject().value(QStringLiteral("logicalId")).toString()
            == QStringLiteral("astrea://wallpaper/default")) {
            foundDefault = true;
            break;
        }
    }
    QVERIFY(foundDefault);

    const auto selected = request(
        endpoint,
        QStringLiteral("wallpaper set {\"id\":\"astrea://wallpaper/default\",\"fit\":\"center\"}"));
    QCOMPARE(selected.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(selected.value(QStringLiteral("completed")).toBool(), true);
    QCOMPARE(selected.value(QStringLiteral("snapshot")).toObject()
                 .value(QStringLiteral("effective")).toObject()
                 .value(QStringLiteral("fit"))
                 .toString(),
             QStringLiteral("center"));
}

void WallpaperControlServerTest::importsThroughPaperEndpoint()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto source = writeImage(temp.filePath(QStringLiteral("source.png")));
    auto catalog = std::make_shared<WallpaperCatalog>(
        WallpaperResolver(factory, emergency), temp.filePath(QStringLiteral("library")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             std::make_unique<XdgWallpaperPersistence>(
                                 temp.filePath(QStringLiteral("paper.ini"))),
                             catalog);
    service.initialize();
    const auto endpoint = temp.filePath(QStringLiteral("r/w.sock"));
    WallpaperControlServer server(&service, endpoint);
    QVERIFY(server.listen());

    const auto imported = request(
        endpoint,
        QStringLiteral("wallpaper import {\"path\":\"%1\",\"fit\":\"contain\"}")
            .arg(source));
    QCOMPARE(imported.value(QStringLiteral("ok")).toBool(), true);
    QVERIFY(imported.value(QStringLiteral("completed")).toBool());
    const auto configured = imported.value(QStringLiteral("snapshot"))
                                .toObject()
                                .value(QStringLiteral("configured"))
                                .toObject();
    const auto id = configured.value(QStringLiteral("logicalId")).toString();
    QVERIFY(id.startsWith(QStringLiteral("astrea://wallpaper/user/")));
    QCOMPARE(configured.value(QStringLiteral("fit")).toString(), QStringLiteral("contain"));

    const auto selected = request(
        endpoint,
        QStringLiteral("wallpaper set {\"id\":\"%1\",\"fit\":\"center\"}").arg(id));
    QCOMPARE(selected.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(selected.value(QStringLiteral("snapshot")).toObject()
                 .value(QStringLiteral("configured")).toObject()
                 .value(QStringLiteral("logicalId"))
                 .toString(),
             id);
}

void WallpaperControlServerTest::addsToCatalogWithoutChangingCurrentWallpaper()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto active = writeImage(temp.filePath(QStringLiteral("active.png")));
    const auto libraryEntry = writeImage(temp.filePath(QStringLiteral("library-entry.png")));
    auto catalog = std::make_shared<WallpaperCatalog>(
        WallpaperResolver(factory, emergency), temp.filePath(QStringLiteral("library")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             std::make_unique<XdgWallpaperPersistence>(
                                 temp.filePath(QStringLiteral("paper.ini"))),
                             catalog);
    service.initialize();
    const auto endpoint = temp.filePath(QStringLiteral("r/w.sock"));
    WallpaperControlServer server(&service, endpoint);
    QVERIFY(server.listen());

    const auto activeReply = request(
        endpoint,
        QStringLiteral("wallpaper import {\"path\":\"%1\",\"fit\":\"contain\",\"displayName\":\"Active\"}")
            .arg(active));
    QVERIFY(activeReply.value(QStringLiteral("ok")).toBool());
    const auto before = activeReply.value(QStringLiteral("snapshot")).toObject();
    const auto beforeConfigured = before.value(QStringLiteral("configured")).toObject();
    const auto beforeEffective = before.value(QStringLiteral("effective")).toObject();
    const auto beforeGeneration = before.value(QStringLiteral("generation"));

    const auto added = request(
        endpoint,
        QStringLiteral("wallpaper add {\"path\":\"%1\",\"displayName\":\"Library B\"}")
            .arg(libraryEntry));
    QCOMPARE(added.value(QStringLiteral("ok")).toBool(), true);
    const auto after = added.value(QStringLiteral("snapshot")).toObject();
    QCOMPARE(after.value(QStringLiteral("configured")).toObject()
                 .value(QStringLiteral("logicalId")),
             beforeConfigured.value(QStringLiteral("logicalId")));
    QCOMPARE(after.value(QStringLiteral("effective")).toObject()
                 .value(QStringLiteral("logicalId")),
             beforeEffective.value(QStringLiteral("logicalId")));
    QCOMPARE(after.value(QStringLiteral("generation")), beforeGeneration);

    QString addedId;
    for (const auto &entry : added.value(QStringLiteral("wallpapers")).toArray()) {
        const auto object = entry.toObject();
        if (object.value(QStringLiteral("displayName")).toString() == QStringLiteral("Library B")) {
            addedId = object.value(QStringLiteral("logicalId")).toString();
            break;
        }
    }
    QVERIFY(!addedId.isEmpty());
    const auto selected = request(
        endpoint,
        QStringLiteral("wallpaper set {\"id\":\"%1\",\"fit\":\"center\"}").arg(addedId));
    QCOMPARE(selected.value(QStringLiteral("ok")).toBool(), true);
    QCOMPARE(selected.value(QStringLiteral("snapshot")).toObject()
                 .value(QStringLiteral("effective")).toObject()
                 .value(QStringLiteral("logicalId")),
             addedId);
}

void WallpaperControlServerTest::failedSecondServerDoesNotRemoveLiveEndpoint()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             std::make_unique<XdgWallpaperPersistence>(
                                 temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();
    const auto endpoint = temp.filePath(QStringLiteral("r/w.sock"));
    WallpaperControlServer first(&service, endpoint);
    QVERIFY(first.listen());

    {
        WallpaperControlServer second(&service, endpoint);
        QString error;
        QVERIFY(!second.listen(&error));
    }

    QVERIFY(QFileInfo::exists(endpoint));
    const auto reply = request(endpoint, QStringLiteral("wallpaper get {}"));
    QCOMPARE(reply.value(QStringLiteral("ok")).toBool(), true);
    first.stopListening();
    QVERIFY(!QFileInfo::exists(endpoint));
}

void WallpaperControlServerTest::boundsIdleClientsAndReclaimsCapacity()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             std::make_unique<XdgWallpaperPersistence>(
                                 temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();
    const auto endpoint = temp.filePath(QStringLiteral("r/w.sock"));
    WallpaperControlServer server(&service, endpoint);
    QVERIFY(server.listen());

    std::vector<std::unique_ptr<QLocalSocket>> clients;
    for (int index = 0; index < 16; ++index) {
        auto client = std::make_unique<QLocalSocket>();
        client->connectToServer(endpoint);
        QVERIFY(client->waitForConnected(1000));
        clients.push_back(std::move(client));
    }

    QLocalSocket excess;
    excess.connectToServer(endpoint);
    QVERIFY(excess.waitForConnected(1000));
    QTRY_VERIFY_WITH_TIMEOUT(excess.state() == QLocalSocket::UnconnectedState, 1000);

    QTest::qWait(700);
    for (auto &client : clients) {
        QVERIFY(client->state() == QLocalSocket::UnconnectedState);
    }

    QLocalSocket replacement;
    replacement.connectToServer(endpoint);
    QVERIFY(replacement.waitForConnected(1000));
}

void WallpaperControlServerTest::rejectsMalformedUnknownAndOversizedRequests()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             std::make_unique<XdgWallpaperPersistence>(
                                 temp.filePath(QStringLiteral("paper.ini"))));
    service.initialize();
    const auto endpoint = temp.filePath(QStringLiteral("r/w.sock"));
    WallpaperControlServer server(&service, endpoint);
    QVERIFY(server.listen());

    const auto malformed = request(endpoint, QStringLiteral("wallpaper set {not-json}"));
    QCOMPARE(malformed.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(malformed.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("control-protocol-error"));

    const auto unknown = request(endpoint, QStringLiteral("wallpaper nope {}"));
    QCOMPARE(unknown.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(unknown.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("control-protocol-error"));

    const auto invalidDisplayName = request(
        endpoint,
        QStringLiteral("wallpaper add {\"path\":\"%1\",\"displayName\":42}").arg(factory));
    QCOMPARE(invalidDisplayName.value(QStringLiteral("ok")).toBool(), false);
    QCOMPARE(invalidDisplayName.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("invalid-descriptor"));

    const auto oversized = WallpaperControlServer::requestReply(
        endpoint, QStringLiteral("wallpaper get {") + QString(5000, QLatin1Char('a')) + '}');
    QVERIFY(oversized.isEmpty());
}

void WallpaperControlServerTest::preservesUnicodeSpacesAndInjectionShapedSourceAsData()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto factory = writeImage(temp.filePath(QStringLiteral("factory.png")));
    const auto emergency = writeImage(temp.filePath(QStringLiteral("emergency.png")));
    const auto selected = writeImage(temp.filePath(QStringLiteral("snow & café; $(touch no).png")));
    auto catalog = std::make_shared<WallpaperCatalog>(
        WallpaperResolver(factory, emergency),
        temp.filePath(QStringLiteral("library")),
        temp.filePath(QStringLiteral("system")));
    WallpaperService service(WallpaperResolver(factory, emergency),
                             std::make_unique<XdgWallpaperPersistence>(
                                 temp.filePath(QStringLiteral("paper.ini"))),
                             catalog);
    service.initialize();
    const auto endpoint = temp.filePath(QStringLiteral("r/w.sock"));
    WallpaperControlServer server(&service, endpoint);
    QVERIFY(server.listen());

    QJsonObject body{{QStringLiteral("source"), selected},
                     {QStringLiteral("fit"), QStringLiteral("center")},
                     {QStringLiteral("kind"), QStringLiteral("image")},
                     {QStringLiteral("scope"), QStringLiteral("global")}};
    const auto line = QStringLiteral("wallpaper set ")
        + QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact));
    const auto reply = request(endpoint, line);
    QCOMPARE(reply.value(QStringLiteral("ok")).toBool(), true);
    const auto managedSource = service.snapshot().effective.source();
    QVERIFY(managedSource != selected);
    QVERIFY(QFileInfo::exists(managedSource));
    QVERIFY(service.snapshot().effective.logicalId().startsWith(
        QStringLiteral("astrea://wallpaper/user/")));
    QVERIFY(!QFileInfo::exists(temp.filePath(QStringLiteral("no"))));
}

QTEST_GUILESS_MAIN(WallpaperControlServerTest)
#include "WallpaperControlServerTest.moc"
