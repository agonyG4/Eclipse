#include "services/wallpaper/SettingsWallpaperController.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QImageReader>
#include <QDir>
#include <QJsonArray>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class SettingsWallpaperControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void usesNativeBoundedJsonControlRequest();
    void exposesFallbackAndConfiguredStateFromSnapshot();
    void rejectsIncompleteResponseAndSurfacesTypedFailure();
    void rejectsLoadingResponseWithoutCompletion();
    void projectsConfiguredAndEffectiveFitFromSnapshot();
    void fallsBackToEffectiveFitWhenConfiguredFitIsAbsent();
    void roundTripsEverySupportedFit();
    void rejectsUnsupportedFitBeforeTransport();
    void waitsBeyondTransportForFinalCompletion();
    void loadsPhysicalFactoryPreviewSourceAcrossProcessBoundary();
    void listsStableWallpaperIdsForSettings();
    void projectsNativePresentationMetadata();
    void selectsStableIdsAndImportsPaths();
    void serializesImportAndCatalogAddWithDisplayNames();
};

namespace {

QJsonObject snapshot(const QString &source, const QString &state, const QString &fallback)
{
    return {
        {QStringLiteral("configured"), QJsonValue(QJsonValue::Null)},
        {QStringLiteral("factoryDefault"), QJsonObject{{QStringLiteral("source"), source},
                                                         {QStringLiteral("fit"), QStringLiteral("cover")}}},
        {QStringLiteral("effective"), QJsonObject{{QStringLiteral("source"), source},
                                                  {QStringLiteral("resolvedSource"), source},
                                                  {QStringLiteral("fit"), QStringLiteral("cover")}}},
        {QStringLiteral("state"), state},
        {QStringLiteral("fallback"), fallback},
        {QStringLiteral("generation"), 4},
        {QStringLiteral("lastError"), QString()},
        {QStringLiteral("errorCode"), QString()},
    };
}

QByteArray response(const QJsonObject &state)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("ok"), true},
                                     {QStringLiteral("completed"), true},
                                     {QStringLiteral("snapshot"), state}})
        .toJson(QJsonDocument::Compact)
        + '\n';
}

} // namespace

void SettingsWallpaperControllerTest::usesNativeBoundedJsonControlRequest()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto endpoint = temp.filePath(QStringLiteral("w.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    QByteArray received;
    QByteArray history;
    QObject::connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [&, socket] {
            received += socket->readAll();
            if (!received.endsWith('\n'))
                return;
            const QString line = QString::fromUtf8(received).trimmed();
            history += received;
            received.clear();
            if (line.startsWith(QStringLiteral("wallpaper set"))) {
                socket->write(response(snapshot(QStringLiteral("/tmp/selected.png"),
                                              QStringLiteral("ready"),
                                              QStringLiteral("none"))));
            } else {
                socket->write(response(snapshot(QStringLiteral("/tmp/default.png"),
                                              QStringLiteral("ready"),
                                              QStringLiteral("none"))));
            }
            socket->flush();
        });
    });

    SettingsWallpaperController controller(endpoint);
    QSignalSpy busyChanged(&controller, &SettingsWallpaperController::busyChanged);
    controller.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QCOMPARE(controller.stateName(), QStringLiteral("ready"));
    controller.setSource(QStringLiteral("/tmp/snow & café.png"), QStringLiteral("contain"));
    QVERIFY(controller.busy());
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QVERIFY(history.contains("snow & café.png"));
    QVERIFY(history.contains("\"fit\":\"contain\""));
    QCOMPARE(controller.stateName(), QStringLiteral("ready"));
    QVERIFY(busyChanged.count() >= 4);
}

void SettingsWallpaperControllerTest::exposesFallbackAndConfiguredStateFromSnapshot()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto endpoint = temp.filePath(QStringLiteral("w.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    QObject::connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [socket] {
            socket->readAll();
            auto state = snapshot(QStringLiteral("/tmp/default.png"),
                                  QStringLiteral("fallback"),
                                  QStringLiteral("source-missing"));
            state.insert(QStringLiteral("configured"),
                         QJsonObject{{QStringLiteral("source"), QStringLiteral("/tmp/gone.png")},
                                     {QStringLiteral("fit"), QStringLiteral("contain")}});
            socket->write(response(state));
            socket->flush();
        });
    });

    SettingsWallpaperController controller(endpoint);
    controller.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QCOMPARE(controller.stateName(), QStringLiteral("fallback"));
    QCOMPARE(controller.fallbackReason(), QStringLiteral("source-missing"));
    QCOMPARE(controller.configuredSource(), QStringLiteral("/tmp/gone.png"));
    QCOMPARE(controller.effectiveSource(), QStringLiteral("/tmp/default.png"));
    QCOMPARE(controller.currentDisplayName(), QString());
}

void SettingsWallpaperControllerTest::rejectsIncompleteResponseAndSurfacesTypedFailure()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto endpoint = temp.filePath(QStringLiteral("w.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    QObject::connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [socket] {
            socket->readAll();
            const auto state = snapshot(QStringLiteral("/tmp/default.png"),
                                        QStringLiteral("fallback"),
                                        QStringLiteral("source-missing"));
            socket->write(QJsonDocument(QJsonObject{
                                            {QStringLiteral("ok"), false},
                                            {QStringLiteral("completed"), true},
                                            {QStringLiteral("errorCode"), QStringLiteral("source-missing")},
                                            {QStringLiteral("message"), QStringLiteral("source vanished")},
                                            {QStringLiteral("snapshot"), state},
                                        })
                              .toJson(QJsonDocument::Compact)
                          + '\n');
            socket->flush();
        });
    });

    SettingsWallpaperController controller(endpoint);
    controller.setSource(QStringLiteral("/tmp/gone.png"));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QCOMPARE(controller.errorCode(), QStringLiteral("source-missing"));
    QCOMPARE(controller.errorMessage(), QStringLiteral("source vanished"));
    QCOMPARE(controller.stateName(), QStringLiteral("fallback"));
}

void SettingsWallpaperControllerTest::rejectsLoadingResponseWithoutCompletion()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto endpoint = temp.filePath(QStringLiteral("w.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    QObject::connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [socket] {
            socket->readAll();
            socket->write(QJsonDocument(QJsonObject{
                                            {QStringLiteral("ok"), true},
                                            {QStringLiteral("completed"), false},
                                        })
                              .toJson(QJsonDocument::Compact)
                          + '\n');
            socket->flush();
        });
    });

    SettingsWallpaperController controller(endpoint);
    controller.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QCOMPARE(controller.errorCode(), QStringLiteral("paper-response-incomplete"));
}

void SettingsWallpaperControllerTest::projectsConfiguredAndEffectiveFitFromSnapshot()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto endpoint = temp.filePath(QStringLiteral("w.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    QObject::connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [socket] {
            socket->readAll();
            auto state = snapshot(QStringLiteral("/tmp/default.png"),
                                  QStringLiteral("fallback"),
                                  QStringLiteral("source-missing"));
            state.insert(QStringLiteral("configured"),
                         QJsonObject{{QStringLiteral("source"), QStringLiteral("/tmp/contain.png")},
                                     {QStringLiteral("fit"), QStringLiteral("contain")}});
            auto effective = state.value(QStringLiteral("effective")).toObject();
            effective.insert(QStringLiteral("fit"), QStringLiteral("cover"));
            state.insert(QStringLiteral("effective"), effective);
            socket->write(response(state));
            socket->flush();
        });
    });

    SettingsWallpaperController controller(endpoint);
    controller.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QCOMPARE(controller.configuredFit(), QStringLiteral("contain"));
    QCOMPARE(controller.effectiveFit(), QStringLiteral("cover"));
    QCOMPARE(controller.selectionFit(), QStringLiteral("contain"));
}

void SettingsWallpaperControllerTest::fallsBackToEffectiveFitWhenConfiguredFitIsAbsent()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto endpoint = temp.filePath(QStringLiteral("w.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    QObject::connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [socket] {
            socket->readAll();
            auto state = snapshot(QStringLiteral("/tmp/default.png"),
                                  QStringLiteral("ready"),
                                  QStringLiteral("none"));
            auto effective = state.value(QStringLiteral("effective")).toObject();
            effective.insert(QStringLiteral("fit"), QStringLiteral("center"));
            state.insert(QStringLiteral("effective"), effective);
            socket->write(response(state));
            socket->flush();
        });
    });

    SettingsWallpaperController controller(endpoint);
    controller.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QCOMPARE(controller.configuredFit(), QString());
    QCOMPARE(controller.effectiveFit(), QStringLiteral("center"));
    QCOMPARE(controller.selectionFit(), QStringLiteral("center"));
}

void SettingsWallpaperControllerTest::roundTripsEverySupportedFit()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto endpoint = temp.filePath(QStringLiteral("w.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    QObject::connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [socket] {
            const auto line = QString::fromUtf8(socket->readAll()).trimmed();
            const auto body = QJsonDocument::fromJson(
                                  line.mid(QStringLiteral("wallpaper set ").size()).toUtf8())
                                  .object();
            const auto fit = body.value(QStringLiteral("fit")).toString();
            auto state = snapshot(QStringLiteral("/tmp/selected.png"),
                                  QStringLiteral("ready"),
                                  QStringLiteral("none"));
            state.insert(QStringLiteral("configured"),
                         QJsonObject{{QStringLiteral("source"), QStringLiteral("/tmp/selected.png")},
                                     {QStringLiteral("fit"), fit}});
            auto effective = state.value(QStringLiteral("effective")).toObject();
            effective.insert(QStringLiteral("fit"), fit);
            state.insert(QStringLiteral("effective"), effective);
            socket->write(response(state));
            socket->flush();
        });
    });

    SettingsWallpaperController controller(endpoint);
    const QStringList fits{QStringLiteral("cover"),
                           QStringLiteral("contain"),
                           QStringLiteral("stretch"),
                           QStringLiteral("center"),
                           QStringLiteral("tile")};
    for (const auto &fit : fits) {
        controller.setSource(QStringLiteral("/tmp/selected.png"), fit);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
        QCOMPARE(controller.configuredFit(), fit);
        QCOMPARE(controller.effectiveFit(), fit);
    }
}

void SettingsWallpaperControllerTest::rejectsUnsupportedFitBeforeTransport()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto endpoint = temp.filePath(QStringLiteral("w.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    bool received = false;
    QObject::connect(&server, &QLocalServer::newConnection, this, [&] {
        received = true;
    });

    SettingsWallpaperController controller(endpoint);
    controller.setSource(QStringLiteral("/tmp/image.png"), QStringLiteral("unknown"));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QVERIFY(!received);
    QCOMPARE(controller.errorCode(), QStringLiteral("invalid-fit"));
}

void SettingsWallpaperControllerTest::waitsBeyondTransportForFinalCompletion()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto endpoint = temp.filePath(QStringLiteral("w.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    QObject::connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [socket] {
            socket->readAll();
            QTimer::singleShot(1500, socket, [socket] {
                if (!socket->isValid())
                    return;
                socket->write(response(snapshot(QStringLiteral("/tmp/selected.png"),
                                              QStringLiteral("ready"),
                                              QStringLiteral("none"))));
                socket->flush();
            });
        });
    });

    SettingsWallpaperController controller(endpoint);
    controller.setSource(QStringLiteral("/tmp/selected.png"), QStringLiteral("contain"));
    QVERIFY(controller.busy());
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 3500);
    QCOMPARE(controller.errorCode(), QString());
    QCOMPARE(controller.stateName(), QStringLiteral("ready"));
}

void SettingsWallpaperControllerTest::loadsPhysicalFactoryPreviewSourceAcrossProcessBoundary()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto endpoint = temp.filePath(QStringLiteral("w.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    const auto factory = QDir(QStringLiteral(ASTREA_ECLIPSE_SOURCE_DIR))
                             .filePath(QStringLiteral("Paper/assets/default.jpg"));
    QVERIFY(QFileInfo::exists(factory));
    QObject::connect(&server, &QLocalServer::newConnection, this, [&, factory] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [socket, factory] {
            socket->readAll();
            socket->write(response(snapshot(factory, QStringLiteral("ready"), QStringLiteral("none"))));
            socket->flush();
        });
    });

    SettingsWallpaperController controller(endpoint);
    controller.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QVERIFY(!controller.effectiveSource().startsWith(QStringLiteral("qrc:")));
    QImageReader reader(controller.effectiveSource());
    QVERIFY2(reader.canRead(), qPrintable(reader.errorString()));
}

void SettingsWallpaperControllerTest::listsStableWallpaperIdsForSettings()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto endpoint = temp.filePath(QStringLiteral("w.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    QObject::connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [socket] {
            socket->readAll();
            const auto reply = QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("completed"), true},
                {QStringLiteral("snapshot"),
                 snapshot(QStringLiteral("/tmp/default.png"),
                          QStringLiteral("ready"),
                          QStringLiteral("none"))},
                {QStringLiteral("wallpapers"),
                 QJsonArray{
                     QJsonObject{{QStringLiteral("logicalId"),
                                  QStringLiteral("astrea://wallpaper/default")},
                                 {QStringLiteral("displayName"),
                                  QStringLiteral("Astrea Default")}},
                     QJsonObject{{QStringLiteral("logicalId"),
                                  QStringLiteral("astrea://wallpaper/user/abc")},
                                 {QStringLiteral("displayName"), QStringLiteral("Blue")}},
                 }}};
            socket->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n');
            socket->flush();
        });
    });

    SettingsWallpaperController controller(endpoint);
    controller.refreshLibrary();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QCOMPARE(controller.wallpapers().size(), 2);
    QCOMPARE(controller.wallpapers().at(1).toMap().value(QStringLiteral("logicalId")).toString(),
             QStringLiteral("astrea://wallpaper/user/abc"));
}

void SettingsWallpaperControllerTest::projectsNativePresentationMetadata()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto endpoint = temp.filePath(QStringLiteral("w.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    QObject::connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [socket] {
            socket->readAll();
            auto state = snapshot(QStringLiteral("/tmp/night-drive.png"),
                                  QStringLiteral("ready"),
                                  QStringLiteral("none"));
            state.insert(QStringLiteral("effective"),
                         QJsonObject{{QStringLiteral("logicalId"), QStringLiteral("astrea://wallpaper/system/night-drive")},
                                     {QStringLiteral("displayName"), QStringLiteral("Night Drive")},
                                     {QStringLiteral("source"), QStringLiteral("/tmp/night-drive.png")},
                                     {QStringLiteral("resolvedSource"), QStringLiteral("/tmp/night-drive.png")},
                                     {QStringLiteral("fit"), QStringLiteral("cover")}});
            const QJsonArray entries{
                QJsonObject{{QStringLiteral("logicalId"), QStringLiteral("astrea://wallpaper/system/dynamic")},
                            {QStringLiteral("kind"), QStringLiteral("dynamic")},
                            {QStringLiteral("origin"), QStringLiteral("system")},
                            {QStringLiteral("displayName"), QStringLiteral("Dynamic")},
                            {QStringLiteral("resolvedSource"), QStringLiteral("/tmp/dynamic.png")}},
                QJsonObject{{QStringLiteral("logicalId"), QStringLiteral("astrea://wallpaper/user/blue")},
                            {QStringLiteral("kind"), QStringLiteral("image")},
                            {QStringLiteral("origin"), QStringLiteral("user")},
                            {QStringLiteral("displayName"), QStringLiteral("Blue")},
                            {QStringLiteral("resolvedSource"), QStringLiteral("/tmp/blue.png")}},
                QJsonObject{{QStringLiteral("logicalId"), QStringLiteral("astrea://wallpaper/system/mountain")},
                            {QStringLiteral("kind"), QStringLiteral("image")},
                            {QStringLiteral("origin"), QStringLiteral("system")},
                            {QStringLiteral("displayName"), QStringLiteral("Mountain")},
                            {QStringLiteral("resolvedSource"), QStringLiteral("/tmp/mountain.png")}},
            };
            socket->write(QJsonDocument(QJsonObject{{QStringLiteral("ok"), true},
                                                    {QStringLiteral("completed"), true},
                                                    {QStringLiteral("snapshot"), state},
                                                    {QStringLiteral("wallpapers"), entries}})
                              .toJson(QJsonDocument::Compact)
                          + '\n');
            socket->flush();
        });
    });

    SettingsWallpaperController controller(endpoint);
    controller.refreshLibrary();
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QCOMPARE(controller.currentDisplayName(), QStringLiteral("Night Drive"));
    QCOMPARE(controller.dynamicWallpapers().size(), 1);
    QCOMPARE(controller.userWallpapers().size(), 1);
    QCOMPARE(controller.landscapeWallpapers().size(), 1);
    const auto user = controller.userWallpapers().constFirst().toMap();
    QCOMPARE(user.value(QStringLiteral("logicalId")).toString(), QStringLiteral("astrea://wallpaper/user/blue"));
    QCOMPARE(user.value(QStringLiteral("resolvedSource")).toString(), QStringLiteral("/tmp/blue.png"));
}

void SettingsWallpaperControllerTest::selectsStableIdsAndImportsPaths()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto endpoint = temp.filePath(QStringLiteral("w.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    QByteArray history;
    QObject::connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [&, socket] {
            const auto line = socket->readAll();
            history += line;
            const auto text = QString::fromUtf8(line).trimmed();
            auto state = snapshot(QStringLiteral("/tmp/managed.png"),
                                  QStringLiteral("ready"),
                                  QStringLiteral("none"));
            const auto fit = text.startsWith(QStringLiteral("wallpaper import"))
                ? QStringLiteral("contain")
                : QStringLiteral("center");
            state.insert(QStringLiteral("configured"),
                         QJsonObject{{QStringLiteral("logicalId"),
                                      QStringLiteral("astrea://wallpaper/user/abc")},
                                     {QStringLiteral("source"), QStringLiteral("/tmp/managed.png")},
                                     {QStringLiteral("fit"), fit}});
            auto effective = state.value(QStringLiteral("effective")).toObject();
            effective.insert(QStringLiteral("logicalId"),
                             QStringLiteral("astrea://wallpaper/user/abc"));
            effective.insert(QStringLiteral("fit"), fit);
            state.insert(QStringLiteral("effective"), effective);
            socket->write(response(state));
            socket->flush();
        });
    });

    SettingsWallpaperController controller(endpoint);
    controller.importWallpaper(QStringLiteral("/tmp/source.png"), QStringLiteral("contain"));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    controller.selectWallpaper(QStringLiteral("astrea://wallpaper/user/abc"),
                               QStringLiteral("center"));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QVERIFY(history.contains("wallpaper import"));
    QVERIFY(history.contains("\"path\":\"/tmp/source.png\""));
    QVERIFY(history.contains("wallpaper set"));
    QVERIFY(history.contains("\"id\":\"astrea://wallpaper/user/abc\""));
}

void SettingsWallpaperControllerTest::serializesImportAndCatalogAddWithDisplayNames()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const auto endpoint = temp.filePath(QStringLiteral("w.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    QByteArray history;
    QObject::connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [&, socket] {
            const auto line = socket->readAll();
            history += line;
            const auto text = QString::fromUtf8(line).trimmed();
            auto state = snapshot(QStringLiteral("/tmp/managed.png"),
                                  QStringLiteral("ready"),
                                  QStringLiteral("none"));
            if (text.startsWith(QStringLiteral("wallpaper import"))) {
                state.insert(QStringLiteral("configured"),
                             QJsonObject{{QStringLiteral("logicalId"),
                                          QStringLiteral("astrea://wallpaper/user/imported")},
                                         {QStringLiteral("displayName"), QStringLiteral("Snow Café")},
                                         {QStringLiteral("source"), QStringLiteral("/tmp/managed.png")},
                                         {QStringLiteral("fit"), QStringLiteral("contain")}});
            }
            socket->write(response(state));
            socket->flush();
        });
    });

    SettingsWallpaperController controller(endpoint);
    controller.importAndSelectWallpaper(QStringLiteral("/tmp/snow.png"),
                                         QStringLiteral("Snow Café"),
                                         QStringLiteral("contain"));
    QVERIFY(controller.busy());
    controller.addUserWallpaper(QStringLiteral("/tmp/library.png"), QStringLiteral("Library B"));
    QCOMPARE(controller.errorCode(), QStringLiteral("paper-request-busy"));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);

    controller.addUserWallpaper(QStringLiteral("/tmp/library.png"), QStringLiteral("Library B"));
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 1000);
    QVERIFY(history.contains("wallpaper import"));
    QVERIFY(history.contains("wallpaper add"));
    QVERIFY(history.contains("\"displayName\":\"Snow Café\""));
    QVERIFY(history.contains("\"displayName\":\"Library B\""));
}

QTEST_GUILESS_MAIN(SettingsWallpaperControllerTest)
#include "SettingsWallpaperControllerTest.moc"
