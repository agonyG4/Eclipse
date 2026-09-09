#include "core/SettingsController.hpp"
#include "services/i18n/SettingsTranslationController.hpp"
#include "theme/ThemeController.hpp"

#include <QGuiApplication>
#include <QImage>
#include <QColor>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQuickItem>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QThread>
#include <QTimer>
#include <QQmlComponent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQmlExpression>
#include <QTemporaryDir>
#include <QVariantMap>
#include <QtTest>

#include <algorithm>
#include <memory>

namespace {

QQuickItem *findVisualItem(QQuickItem *item, const QString &objectName)
{
    if (!item)
        return nullptr;
    if (item->objectName() == objectName)
        return item;
    for (QQuickItem *child : item->childItems()) {
        if (QQuickItem *match = findVisualItem(child, objectName))
            return match;
    }
    return nullptr;
}

int countVisualItems(QQuickItem *item, const QRegularExpression &pattern)
{
    if (!item)
        return 0;
    int count = pattern.match(item->objectName()).hasMatch() ? 1 : 0;
    for (QQuickItem *child : item->childItems())
        count += countVisualItems(child, pattern);
    return count;
}

} // namespace

class SettingsQmlSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsCompositorRouteOffscreen();
    void loadsCustomizationHubOffscreen();
    void loadsAppearanceRouteFromHubOffscreen();
    void appearancePreviewsUseCurrentWallpaperSnapshot();
    void materialPreviewFrostedGeometryMatchesFallback();
    void materialShowcaseIdentityNamesAreDistinct();
    void appearanceReusesSnapshotAndUpdatesWithoutRecreation();
    void appearanceDoesNotRefreshWhileWallpaperBusy();
    void appearancePreviewFallsBackWithoutWallpaperService();
    void materialPreviewRendererHandoffIsFailSafe();
    void appearanceChoicesUpdateController();
    void appearanceChoicesPreserveExternalControllerPropagation();
    void appearanceIconChoicesPreserveControllerPropagation();
    void appearanceAccentChoicesUpdateController();
    void loadsWallpaperRouteFromHubOffscreen();
    void loadsDockRouteFromHubOffscreen();
    void navigatesBackAndForwardFromHub();
    void sidebarHidesNestedDestinations();
    void resolvesHubHeroIconsByMetadataPrecedence();
    void wallpaperTranslationKeysExist();
    void wallpaperPreviewAndRemovalUseRealPointerEvents();
};

QString writeWallpaperImage(const QString &path, const QColor &color)
{
    QImage image(96, 64, QImage::Format_ARGB32);
    image.fill(color);
    if (!image.save(path))
        qFatal("Could not create wallpaper fixture at %s", qPrintable(path));
    return path;
}

QJsonObject appearanceWallpaperSnapshot(const QString &previewSource,
                                        const QString &fit = QStringLiteral("cover"),
                                        const int generation = 0)
{
    const QJsonObject effective{
        {QStringLiteral("logicalId"), QStringLiteral("astrea://wallpaper/test/current")},
        {QStringLiteral("source"), QStringLiteral("/paper/internal/current.png")},
        {QStringLiteral("resolvedSource"), QStringLiteral("/paper/internal/current.png")},
        {QStringLiteral("previewSource"), previewSource},
        {QStringLiteral("fit"), fit},
        {QStringLiteral("displayName"), QStringLiteral("Test Wallpaper")},
    };
    return {
        {QStringLiteral("configured"), effective},
        {QStringLiteral("factoryDefault"), effective},
        {QStringLiteral("effective"), effective},
        {QStringLiteral("state"), QStringLiteral("ready")},
        {QStringLiteral("fallback"), QStringLiteral("none")},
        {QStringLiteral("generation"), generation},
        {QStringLiteral("errorCode"), QString()},
        {QStringLiteral("lastError"), QString()},
    };
}

QByteArray appearanceWallpaperResponse(const QString &previewSource,
                                       const QString &fit = QStringLiteral("cover"),
                                       const int generation = 0)
{
    return QJsonDocument(QJsonObject{
                             {QStringLiteral("ok"), true},
                             {QStringLiteral("completed"), true},
                             {QStringLiteral("snapshot"),
                              appearanceWallpaperSnapshot(previewSource, fit, generation)},
                         })
        .toJson(QJsonDocument::Compact)
        + '\n';
}

class RuntimeEnvironmentGuard final
{
public:
    explicit RuntimeEnvironmentGuard(const QString &runtimePath)
        : m_previous(qgetenv("XDG_RUNTIME_DIR"))
        , m_hadPrevious(qEnvironmentVariableIsSet("XDG_RUNTIME_DIR"))
    {
        qputenv("XDG_RUNTIME_DIR", runtimePath.toUtf8());
    }

    ~RuntimeEnvironmentGuard()
    {
        if (m_hadPrevious)
            qputenv("XDG_RUNTIME_DIR", m_previous);
        else
            qunsetenv("XDG_RUNTIME_DIR");
    }

private:
    QByteArray m_previous;
    bool m_hadPrevious = false;
};

class DelayedMaterialPreviewProvider final : public QQuickImageProvider
{
public:
    explicit DelayedMaterialPreviewProvider(const int delayMs)
        : QQuickImageProvider(QQuickImageProvider::Image,
                              QQuickImageProvider::ForceAsynchronousImageLoading)
        , m_delayMs(delayMs)
    {
    }

    QImage requestImage(const QString &, QSize *size, const QSize &requestedSize) override
    {
        QThread::msleep(static_cast<unsigned long>(m_delayMs));
        const auto imageSize = requestedSize.isValid() ? requestedSize : QSize(96, 64);
        if (size)
            *size = imageSize;
        QImage image(imageSize, QImage::Format_ARGB32);
        image.fill(QColor("#b87333"));
        return image;
    }

private:
    int m_delayMs = 0;
};

void SettingsQmlSmokeTest::loadsCompositorRouteOffscreen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController(directory.filePath(QStringLiteral("missing-theme.json")));
    QQmlApplicationEngine engine;
    QList<QQmlError> qmlWarnings;

    connect(&engine, &QQmlApplicationEngine::warnings, this,
            [&qmlWarnings](const QList<QQmlError> &warnings) {
                qmlWarnings.append(warnings);
            });

    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();
    QObject *loader = root->findChild<QObject *>(QStringLiteral("settingsPageLoader"));
    QVERIFY(loader != nullptr);
    QVERIFY(settingsController.navigateTo(QStringLiteral("compositor")));
    auto loadedPage = [&loader]() {
        return qvariant_cast<QObject *>(loader->property("item"));
    };
    QTRY_VERIFY_WITH_TIMEOUT(loadedPage() != nullptr
                                 && loadedPage()->objectName() == QStringLiteral("compositorPage"),
                             1000);
    QObject *page = loadedPage();
    QVERIFY(page != nullptr);
    QCOMPARE(page->property("animationsEnabled").toBool(), true);
    page->setProperty("animationsEnabled", false);
    QCOMPARE(page->property("animationsEnabled").toBool(), false);

    QVERIFY(!settingsController.navigateTo(QStringLiteral("system")));
    QCOMPARE(settingsController.currentDestinationId(), QStringLiteral("compositor"));
    QVERIFY(settingsController.navigateTo(QStringLiteral("wallpaper")));
    QTRY_VERIFY_WITH_TIMEOUT(loadedPage() != nullptr
                                 && loadedPage()->objectName() == QStringLiteral("wallpaperPage"),
                             1000);
    QVERIFY(settingsController.navigateTo(QStringLiteral("compositor")));
    QTRY_VERIFY_WITH_TIMEOUT(loadedPage() != nullptr
                                 && loadedPage()->objectName() == QStringLiteral("compositorPage"),
                             1000);
    QCOMPARE(loadedPage()->property("animationsEnabled").toBool(), true);
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(qmlWarnings.isEmpty() ? QString() : qmlWarnings.constFirst().toString()));
}

void SettingsQmlSmokeTest::loadsCustomizationHubOffscreen()
{
    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController;
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(settingsController.navigateTo(QStringLiteral("customization")));
    QObject *root = engine.rootObjects().constFirst();
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("settingsHubPage")) != nullptr, 1000);
    QObject *hub = root->findChild<QObject *>(QStringLiteral("settingsHubPage"));
    QVERIFY(hub != nullptr);
    QQuickItem *hubItem = qobject_cast<QQuickItem *>(hub);
    QVERIFY(hubItem != nullptr);
    QVERIFY(findVisualItem(hubItem, QStringLiteral("hubNavigationRow-appearance")) != nullptr);
    QVERIFY(findVisualItem(hubItem, QStringLiteral("hubNavigationRow-wallpaper")) != nullptr);
    QVERIFY(findVisualItem(hubItem, QStringLiteral("hubNavigationRow-dock")) != nullptr);
    QCOMPARE(countVisualItems(hubItem, QRegularExpression(QStringLiteral("^hubNavigationRow-"))), 3);
    const QVariantList children = settingsController.currentDestinationChildren();
    QCOMPARE(children.size(), 3);
    QCOMPARE(children.at(0).toMap().value(QStringLiteral("entryId")).toString(),
             QStringLiteral("appearance"));
}

void SettingsQmlSmokeTest::loadsAppearanceRouteFromHubOffscreen()
{
    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController;
    QQmlApplicationEngine engine;
    QList<QQmlError> qmlWarnings;

    connect(&engine, &QQmlApplicationEngine::warnings, this,
            [&qmlWarnings](const QList<QQmlError> &warnings) {
                qmlWarnings.append(warnings);
            });

    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(settingsController.navigateTo(QStringLiteral("customization")));
    QObject *root = engine.rootObjects().constFirst();
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("settingsHubPage")) != nullptr,
                             1000);
    QObject *hub = root->findChild<QObject *>(QStringLiteral("settingsHubPage"));
    QVERIFY(hub != nullptr);
    QObject *row = findVisualItem(qobject_cast<QQuickItem *>(hub),
                                  QStringLiteral("hubNavigationRow-appearance"));
    QVERIFY(row != nullptr);
    qmlWarnings.clear();
    QVERIFY(QMetaObject::invokeMethod(row, "clicked"));
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("appearancePage")) != nullptr,
                             1000);
    QObject *page = root->findChild<QObject *>(QStringLiteral("appearancePage"));
    QVERIFY(page != nullptr);
    QVERIFY(page->findChild<QObject *>(QStringLiteral("appearanceScrollPage")) != nullptr);
    for (const auto name : {"appearanceOption-auto", "appearanceOption-light",
                            "appearanceOption-dark", "interfaceStyleOption-default",
                            "interfaceStyleOption-transparent", "interfaceStyleOption-frosted"}) {
        QVERIFY2(page->findChild<QObject *>(QString::fromLatin1(name)) != nullptr, name);
    }
    QVERIFY2(qmlWarnings.isEmpty(),
             qPrintable(qmlWarnings.isEmpty() ? QString() : qmlWarnings.constFirst().toString()));
}

void SettingsQmlSmokeTest::appearancePreviewsUseCurrentWallpaperSnapshot()
{
    QTemporaryDir runtime;
    QTemporaryDir images;
    QVERIFY(runtime.isValid());
    QVERIFY(images.isValid());
    QVERIFY(QDir(runtime.path()).mkpath(QStringLiteral("astrea-shell")));
    RuntimeEnvironmentGuard runtimeGuard(runtime.path());

    const auto previewPath = writeWallpaperImage(
        images.filePath(QStringLiteral("appearance-current.png")), QColor("#456e9d"));
    const auto endpoint = QDir(runtime.path()).filePath(QStringLiteral("astrea-shell/wallpaper.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    int requestCount = 0;
    QByteArray requestBuffer;
    QObject::connect(&server, &QLocalServer::newConnection, this, [&, previewPath] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [&, socket, previewPath] {
            requestBuffer += socket->readAll();
            if (!requestBuffer.endsWith('\n'))
                return;
            const auto line = QString::fromUtf8(requestBuffer).trimmed();
            requestBuffer.clear();
            if (!line.startsWith(QStringLiteral("wallpaper get")))
                return;
            ++requestCount;
            socket->write(appearanceWallpaperResponse(previewPath, QStringLiteral("contain"), 0));
            socket->flush();
        });
    });

    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"),
                                             &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(settingsController.navigateTo(QStringLiteral("appearance")));
    auto *root = engine.rootObjects().constFirst();
    QVERIFY(root != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("appearancePage")) != nullptr,
                             1000);
    auto *page = qobject_cast<QQuickItem *>(root->findChild<QObject *>(
        QStringLiteral("appearancePage")));
    QVERIFY(page != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(requestCount, 1, 1500);

    const auto expectedSource = QUrl::fromLocalFile(previewPath);
    const QStringList appearancePreviewNames{
        QStringLiteral("materialPreview-appearance-auto"),
        QStringLiteral("materialPreview-appearance-light"),
        QStringLiteral("materialPreview-appearance-dark"),
        QStringLiteral("materialPreview-interface-default"),
        QStringLiteral("materialPreview-interface-transparent"),
        QStringLiteral("materialPreview-interface-frosted"),
    };
    for (const auto &name : appearancePreviewNames) {
        auto *preview = findVisualItem(page, name);
        QVERIFY2(preview != nullptr, qPrintable(name));
        QTRY_COMPARE_WITH_TIMEOUT(preview->property("wallpaperSource").toUrl(), expectedSource,
                                  1500);
        QCOMPARE(preview->property("wallpaperFit").toString(), QStringLiteral("contain"));
        QCOMPARE(preview->property("usingRendererPreview").toBool(), false);
        QVERIFY(findVisualItem(preview, QStringLiteral("materialPreviewWallpaper")) != nullptr);
        QVERIFY(findVisualItem(preview, QStringLiteral("materialPreviewShowcase")) != nullptr);
        QVERIFY(findVisualItem(preview, QStringLiteral("materialPreviewFallback")) != nullptr);
    }

    QCOMPARE(findVisualItem(page, QStringLiteral("materialPreview-appearance-auto"))
                 ->property("themeVariant")
                 .toString(),
             QStringLiteral("auto"));
    QCOMPARE(findVisualItem(page, QStringLiteral("materialPreview-appearance-light"))
                 ->property("themeVariant")
                 .toString(),
             QStringLiteral("light"));
    QCOMPARE(findVisualItem(page, QStringLiteral("materialPreview-appearance-dark"))
                 ->property("themeVariant")
                 .toString(),
             QStringLiteral("dark"));
    QCOMPARE(findVisualItem(page, QStringLiteral("materialPreview-interface-default"))
                 ->property("materialId")
                 .toString(),
             QStringLiteral("default"));
    QCOMPARE(findVisualItem(page, QStringLiteral("materialPreview-interface-transparent"))
                 ->property("materialId")
                 .toString(),
             QStringLiteral("transparent"));
    QCOMPARE(findVisualItem(page, QStringLiteral("materialPreview-interface-frosted"))
                 ->property("materialId")
                 .toString(),
             QStringLiteral("frosted"));
    auto *frostedPreview = findVisualItem(
        page, QStringLiteral("materialPreview-interface-frosted"));
    QVERIFY(frostedPreview != nullptr);
    QVERIFY(frostedPreview->findChild<QObject *>(QStringLiteral("materialPreviewLiveFrosted"))
            != nullptr);
    QCOMPARE(frostedPreview->property("liveFrostedAvailable").toBool(), false);
    QCOMPARE(frostedPreview->property("liveFrostedActive").toBool(), false);
}

void SettingsQmlSmokeTest::materialPreviewFrostedGeometryMatchesFallback()
{
    ThemeController themeController;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    QQmlComponent component(
        &engine,
        QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/MaterialPreview.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));
    std::unique_ptr<QObject> previewObject(component.create());
    QVERIFY(previewObject != nullptr);

    auto *preview = qobject_cast<QQuickItem *>(previewObject.get());
    QVERIFY(preview != nullptr);
    preview->setWidth(1000);
    preview->setHeight(600);
    preview->setProperty("materialId", QStringLiteral("frosted"));
    QCoreApplication::processEvents();

    auto *live = findVisualItem(preview, QStringLiteral("materialPreviewLiveFrosted"));
    auto *fallback = findVisualItem(preview, QStringLiteral("materialPreviewShowcase"));
    QVERIFY(live != nullptr);
    QVERIFY(fallback != nullptr);
    QCOMPARE(live->width(), fallback->width());
    QCOMPARE(live->height(), fallback->height());
    QCOMPARE(live->x(), fallback->x());
    QCOMPARE(live->y(), fallback->y());
}

void SettingsQmlSmokeTest::materialShowcaseIdentityNamesAreDistinct()
{
    ThemeController themeController;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    QQmlComponent component(
        &engine,
        QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/pages/appearance/MaterialShowcase.qml")));
    QVERIFY2(component.status() == QQmlComponent::Ready, qPrintable(component.errorString()));

    const QVariantMap fallbackProperties{{QStringLiteral("canonicalIdentity"), false}};
    const QVariantMap liveProperties{{QStringLiteral("canonicalIdentity"), true}};
    std::unique_ptr<QObject> fallback(component.createWithInitialProperties(fallbackProperties));
    std::unique_ptr<QObject> live(component.createWithInitialProperties(liveProperties));
    QVERIFY(fallback != nullptr);
    QVERIFY(live != nullptr);
    QCOMPARE(fallback->objectName(), QStringLiteral("materialPreviewShowcaseFallback"));
    QCOMPARE(live->objectName(), QStringLiteral("materialPreviewShowcase"));
    QVERIFY(fallback->objectName() != live->objectName());
}

void SettingsQmlSmokeTest::appearanceReusesSnapshotAndUpdatesWithoutRecreation()
{
    QTemporaryDir runtime;
    QTemporaryDir images;
    QVERIFY(runtime.isValid());
    QVERIFY(images.isValid());
    QVERIFY(QDir(runtime.path()).mkpath(QStringLiteral("astrea-shell")));
    RuntimeEnvironmentGuard runtimeGuard(runtime.path());

    const auto firstPath = writeWallpaperImage(
        images.filePath(QStringLiteral("first.png")), QColor("#496d9c"));
    const auto secondPath = writeWallpaperImage(
        images.filePath(QStringLiteral("second.png")), QColor("#9b6549"));
    const auto endpoint = QDir(runtime.path()).filePath(QStringLiteral("astrea-shell/wallpaper.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    int requestCount = 0;
    QByteArray requestBuffer;
    QObject::connect(&server, &QLocalServer::newConnection, this, [&, firstPath, secondPath] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [&, socket, firstPath, secondPath] {
            requestBuffer += socket->readAll();
            if (!requestBuffer.endsWith('\n'))
                return;
            const auto line = QString::fromUtf8(requestBuffer).trimmed();
            requestBuffer.clear();
            if (!line.startsWith(QStringLiteral("wallpaper get")))
                return;
            ++requestCount;
            const auto path = requestCount == 1 ? firstPath : secondPath;
            const auto fit = requestCount == 1 ? QStringLiteral("cover") : QStringLiteral("stretch");
            const auto response = appearanceWallpaperResponse(path, fit, requestCount == 1 ? 0 : 1);
            if (requestCount == 1) {
                socket->write(response);
                socket->flush();
                return;
            }
            QTimer::singleShot(250, socket, [socket, response] {
                if (!socket->isValid())
                    return;
                socket->write(response);
                socket->flush();
            });
        });
    });

    SettingsController settingsController;
    settingsController.wallpaper()->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!settingsController.wallpaper()->busy(), 1500);
    QCOMPARE(requestCount, 1);
    QCOMPARE(settingsController.wallpaper()->stateName(), QStringLiteral("ready"));

    SettingsTranslationController translationController;
    ThemeController themeController;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"),
                                             &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(settingsController.navigateTo(QStringLiteral("appearance")));
    auto *root = engine.rootObjects().constFirst();
    QVERIFY(root != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("appearancePage")) != nullptr,
                             1000);
    auto *page = qobject_cast<QQuickItem *>(root->findChild<QObject *>(
        QStringLiteral("appearancePage")));
    QVERIFY(page != nullptr);
    auto *preview = findVisualItem(page, QStringLiteral("materialPreview-appearance-auto"));
    QVERIFY(preview != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(requestCount, 2, 1500);
    QCOMPARE(preview->property("wallpaperSource").toUrl(), QUrl::fromLocalFile(firstPath));
    QVERIFY(settingsController.wallpaper()->busy());

    QTRY_VERIFY_WITH_TIMEOUT(!settingsController.wallpaper()->busy(), 1500);
    QTRY_COMPARE_WITH_TIMEOUT(preview->property("wallpaperSource").toUrl(),
                              QUrl::fromLocalFile(secondPath), 1500);
    QCOMPARE(preview->property("wallpaperFit").toString(), QStringLiteral("stretch"));
}

void SettingsQmlSmokeTest::appearanceDoesNotRefreshWhileWallpaperBusy()
{
    QTemporaryDir runtime;
    QTemporaryDir images;
    QVERIFY(runtime.isValid());
    QVERIFY(images.isValid());
    QVERIFY(QDir(runtime.path()).mkpath(QStringLiteral("astrea-shell")));
    RuntimeEnvironmentGuard runtimeGuard(runtime.path());

    const auto previewPath = writeWallpaperImage(
        images.filePath(QStringLiteral("busy.png")), QColor("#647f9e"));
    const auto endpoint = QDir(runtime.path()).filePath(QStringLiteral("astrea-shell/wallpaper.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    int requestCount = 0;
    QByteArray requestBuffer;
    QObject::connect(&server, &QLocalServer::newConnection, this, [&, previewPath] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [&, socket, previewPath] {
            requestBuffer += socket->readAll();
            if (!requestBuffer.endsWith('\n'))
                return;
            requestBuffer.clear();
            ++requestCount;
            QTimer::singleShot(250, socket, [socket, previewPath] {
                if (!socket->isValid())
                    return;
                socket->write(appearanceWallpaperResponse(previewPath, QStringLiteral("center"), 0));
                socket->flush();
            });
        });
    });

    SettingsController settingsController;
    settingsController.wallpaper()->refresh();
    QTRY_COMPARE_WITH_TIMEOUT(requestCount, 1, 1000);
    QVERIFY(settingsController.wallpaper()->busy());

    SettingsTranslationController translationController;
    ThemeController themeController;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"),
                                             &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(settingsController.navigateTo(QStringLiteral("appearance")));
    auto *root = engine.rootObjects().constFirst();
    QVERIFY(root != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("appearancePage")) != nullptr,
                             1000);
    auto *page = qobject_cast<QQuickItem *>(root->findChild<QObject *>(
        QStringLiteral("appearancePage")));
    QVERIFY(page != nullptr);
    QTest::qWait(100);
    QCOMPARE(requestCount, 1);

    auto *preview = findVisualItem(page, QStringLiteral("materialPreview-appearance-auto"));
    QVERIFY(preview != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(!settingsController.wallpaper()->busy(), 1500);
    QTRY_COMPARE_WITH_TIMEOUT(preview->property("wallpaperSource").toUrl(),
                              QUrl::fromLocalFile(previewPath), 1500);
    QCOMPARE(preview->property("wallpaperFit").toString(), QStringLiteral("center"));
}

void SettingsQmlSmokeTest::appearancePreviewFallsBackWithoutWallpaperService()
{
    QTemporaryDir runtime;
    QVERIFY(runtime.isValid());
    QVERIFY(QDir(runtime.path()).mkpath(QStringLiteral("astrea-shell")));
    RuntimeEnvironmentGuard runtimeGuard(runtime.path());

    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"),
                                             &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(settingsController.navigateTo(QStringLiteral("appearance")));
    auto *root = engine.rootObjects().constFirst();
    QVERIFY(root != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("appearancePage")) != nullptr,
                             1000);
    auto *page = qobject_cast<QQuickItem *>(root->findChild<QObject *>(
        QStringLiteral("appearancePage")));
    QVERIFY(page != nullptr);
    auto *preview = findVisualItem(page, QStringLiteral("materialPreview-appearance-auto"));
    QVERIFY(preview != nullptr);
    QVERIFY(findVisualItem(preview, QStringLiteral("materialPreviewFallback")) != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(
        findVisualItem(preview, QStringLiteral("materialPreviewFallback"))->property("visible")
            .toBool(),
        1000);
    QVERIFY(page->findChild<QObject *>(QStringLiteral("appearanceOption-light")) != nullptr);
}

void SettingsQmlSmokeTest::materialPreviewRendererHandoffIsFailSafe()
{
    QTemporaryDir runtime;
    QTemporaryDir images;
    QVERIFY(runtime.isValid());
    QVERIFY(images.isValid());
    QVERIFY(QDir(runtime.path()).mkpath(QStringLiteral("astrea-shell")));
    RuntimeEnvironmentGuard runtimeGuard(runtime.path());

    const auto wallpaperPath = writeWallpaperImage(
        images.filePath(QStringLiteral("renderer-wallpaper.png")), QColor("#496d9c"));
    const auto endpoint = QDir(runtime.path()).filePath(QStringLiteral("astrea-shell/wallpaper.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));
    QByteArray requestBuffer;
    QObject::connect(&server, &QLocalServer::newConnection, this, [&, wallpaperPath] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [&, socket, wallpaperPath] {
            requestBuffer += socket->readAll();
            if (!requestBuffer.endsWith('\n'))
                return;
            requestBuffer.clear();
            socket->write(appearanceWallpaperResponse(wallpaperPath, QStringLiteral("cover"), 0));
            socket->flush();
        });
    });

    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController;
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("material-preview-test"),
                            new DelayedMaterialPreviewProvider(250));
    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"),
                                             &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(settingsController.navigateTo(QStringLiteral("appearance")));
    auto *root = engine.rootObjects().constFirst();
    QVERIFY(root != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("appearancePage")) != nullptr,
                             1000);
    auto *page = qobject_cast<QQuickItem *>(root->findChild<QObject *>(
        QStringLiteral("appearancePage")));
    QVERIFY(page != nullptr);
    auto *preview = findVisualItem(page, QStringLiteral("materialPreview-appearance-auto"));
    QVERIFY(preview != nullptr);
    auto *wallpaper = findVisualItem(preview, QStringLiteral("materialPreviewWallpaper"));
    auto *rendererFrame = findVisualItem(preview, QStringLiteral("materialPreviewRendererFrame"));
    auto *showcase = findVisualItem(preview, QStringLiteral("materialPreviewShowcase"));
    QVERIFY(wallpaper != nullptr);
    QVERIFY(rendererFrame != nullptr);
    QVERIFY(showcase != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(preview->property("wallpaperReady").toBool(), 1500);
    QVERIFY(wallpaper->property("visible").toBool());
    QVERIFY(!rendererFrame->property("visible").toBool());
    QVERIFY(showcase->property("visible").toBool());

    preview->setProperty("rendererPreviewSource",
                         QUrl(QStringLiteral("image://material-preview-test/slow")));
    preview->setProperty("rendererPreviewReady", true);
    QVERIFY(preview->property("rendererPreviewRequested").toBool());
    QVERIFY(!preview->property("usingRendererPreview").toBool());
    QVERIFY(wallpaper->property("visible").toBool());
    QVERIFY(!rendererFrame->property("visible").toBool());
    QVERIFY(showcase->property("visible").toBool());

    QTRY_VERIFY_WITH_TIMEOUT(preview->property("usingRendererPreview").toBool(), 2000);
    QVERIFY(rendererFrame->property("visible").toBool());
    QVERIFY(!showcase->property("visible").toBool());

    preview->setProperty("rendererPreviewSource",
                         QUrl::fromLocalFile(images.filePath(QStringLiteral("missing-renderer.png"))));
    QVERIFY(preview->property("rendererPreviewRequested").toBool());
    QTRY_VERIFY_WITH_TIMEOUT(preview->property("rendererPreviewFailed").toBool(), 1500);
    QVERIFY(!preview->property("usingRendererPreview").toBool());
    QTRY_VERIFY_WITH_TIMEOUT(wallpaper->property("visible").toBool(), 1500);
    QVERIFY(!rendererFrame->property("visible").toBool());
    QVERIFY(showcase->property("visible").toBool());
}

void SettingsQmlSmokeTest::appearanceChoicesUpdateController()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController(directory.filePath(QStringLiteral("missing-theme.json")), nullptr,
                                    [] { return Qt::ColorScheme::Dark; });
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(settingsController.navigateTo(QStringLiteral("appearance")));
    QObject *root = engine.rootObjects().constFirst();
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("appearancePage")) != nullptr,
                             1000);
    QObject *page = root->findChild<QObject *>(QStringLiteral("appearancePage"));
    QVERIFY(page != nullptr);

    auto *automatic = page->findChild<QObject *>(QStringLiteral("appearanceOption-auto"));
    auto *light = page->findChild<QObject *>(QStringLiteral("appearanceOption-light"));
    auto *dark = page->findChild<QObject *>(QStringLiteral("appearanceOption-dark"));
    auto *defaultStyle = page->findChild<QObject *>(QStringLiteral("interfaceStyleOption-default"));
    auto *transparent = page->findChild<QObject *>(QStringLiteral("interfaceStyleOption-transparent"));
    auto *frosted = page->findChild<QObject *>(QStringLiteral("interfaceStyleOption-frosted"));
    QVERIFY(automatic != nullptr);
    QVERIFY(light != nullptr);
    QVERIFY(dark != nullptr);
    QVERIFY(defaultStyle != nullptr);
    QVERIFY(transparent != nullptr);
    QVERIFY(frosted != nullptr);

    QCOMPARE(themeController.themePreference(), QStringLiteral("auto"));
    QCOMPARE(themeController.shellStyle(), 1);
    QVERIFY(automatic->property("selected").toBool());
    QVERIFY(!light->property("selected").toBool());
    QVERIFY(!dark->property("selected").toBool());
    QVERIFY(defaultStyle->property("selected").toBool());
    QVERIFY(!transparent->property("selected").toBool());
    QVERIFY(!frosted->property("selected").toBool());

    QVERIFY(QMetaObject::invokeMethod(light, "activate"));
    QCOMPARE(themeController.themePreference(), QStringLiteral("light"));
    QCOMPARE(themeController.themeMode(), 1);
    QVERIFY(QMetaObject::invokeMethod(dark, "activate"));
    QCOMPARE(themeController.themePreference(), QStringLiteral("dark"));
    QCOMPARE(themeController.themeMode(), 0);
    QVERIFY(QMetaObject::invokeMethod(automatic, "activate"));
    QCOMPARE(themeController.themePreference(), QStringLiteral("auto"));

    QVERIFY(QMetaObject::invokeMethod(defaultStyle, "activate"));
    QCOMPARE(themeController.shellStyle(), 1);
    QVERIFY(QMetaObject::invokeMethod(transparent, "activate"));
    QCOMPARE(themeController.shellStyle(), 0);
    QVERIFY(QMetaObject::invokeMethod(frosted, "activate"));
    QCOMPARE(themeController.shellStyle(), 2);
    QCOMPARE(themeController.themePreference(), QStringLiteral("auto"));
}

void SettingsQmlSmokeTest::appearanceChoicesPreserveExternalControllerPropagation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController(directory.filePath(QStringLiteral("theme.json")), nullptr,
                                    [] { return Qt::ColorScheme::Dark; });
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(settingsController.navigateTo(QStringLiteral("appearance")));
    QObject *root = engine.rootObjects().constFirst();
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("appearancePage")) != nullptr,
                             1000);
    QObject *page = root->findChild<QObject *>(QStringLiteral("appearancePage"));
    QVERIFY(page != nullptr);

    auto *light = page->findChild<QObject *>(QStringLiteral("appearanceOption-light"));
    auto *dark = page->findChild<QObject *>(QStringLiteral("appearanceOption-dark"));
    auto *transparent = page->findChild<QObject *>(QStringLiteral("interfaceStyleOption-transparent"));
    auto *frosted = page->findChild<QObject *>(QStringLiteral("interfaceStyleOption-frosted"));
    QVERIFY(light != nullptr);
    QVERIFY(dark != nullptr);
    QVERIFY(transparent != nullptr);
    QVERIFY(frosted != nullptr);

    QVERIFY(QMetaObject::invokeMethod(light, "activate"));
    QVERIFY(QMetaObject::invokeMethod(transparent, "activate"));
    QCOMPARE(themeController.themePreference(), QStringLiteral("light"));
    QCOMPARE(themeController.shellStyle(), 0);

    QQmlExpression projectedPreference(qmlContext(page), page,
                                       QStringLiteral("Components.Theme.themePreference"));
    QQmlExpression projectedShellStyle(qmlContext(page), page,
                                       QStringLiteral("Components.Theme.shellStyle"));
    QCOMPARE(projectedPreference.evaluate().toString(), QStringLiteral("light"));
    QCOMPARE(projectedShellStyle.evaluate().toInt(), 0);

    QFile replacement(themeController.configPath());
    QVERIFY(replacement.open(QIODevice::WriteOnly | QIODevice::Truncate));
    replacement.write(QJsonDocument(QJsonObject{
        {QStringLiteral("theme_preference"), QStringLiteral("dark")},
        {QStringLiteral("shell_style"), 2},
    }).toJson(QJsonDocument::Compact));
    replacement.close();

    QTRY_COMPARE_WITH_TIMEOUT(themeController.themePreference(), QStringLiteral("dark"), 1500);
    QTRY_COMPARE_WITH_TIMEOUT(themeController.shellStyle(), 2, 1500);
    QTRY_VERIFY_WITH_TIMEOUT(projectedPreference.evaluate().toString() == QStringLiteral("dark")
                                 && projectedShellStyle.evaluate().toInt() == 2,
                             1500);
    QVERIFY(dark->property("selected").toBool());
    QVERIFY(!light->property("selected").toBool());
    QVERIFY(frosted->property("selected").toBool());
    QVERIFY(!transparent->property("selected").toBool());
}

void SettingsQmlSmokeTest::appearanceIconChoicesPreserveControllerPropagation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController(directory.filePath(QStringLiteral("theme.json")), nullptr,
                                    [] { return Qt::ColorScheme::Dark; });
    QQmlApplicationEngine engine;
    QList<QQmlError> qmlWarnings;

    connect(&engine, &QQmlApplicationEngine::warnings, this,
            [&qmlWarnings](const QList<QQmlError> &warnings) {
                qmlWarnings.append(warnings);
            });

    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(settingsController.navigateTo(QStringLiteral("appearance")));
    QObject *root = engine.rootObjects().constFirst();
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("appearancePage")) != nullptr,
                             1000);
    QObject *page = root->findChild<QObject *>(QStringLiteral("appearancePage"));
    QVERIFY(page != nullptr);

    QObject *defaultAppearance = page->findChild<QObject *>(QStringLiteral("iconAppearance-default"));
    QObject *monochrome = page->findChild<QObject *>(QStringLiteral("iconAppearance-monochrome"));
    QObject *tinted = page->findChild<QObject *>(QStringLiteral("iconAppearance-tinted"));
    QObject *tintedDisplay = page->findChild<QObject *>(
        QStringLiteral("iconPreview-tinted-display"));
    QVERIFY(defaultAppearance != nullptr);
    QVERIFY(monochrome != nullptr);
    QVERIFY(tinted != nullptr);
    QVERIFY(tintedDisplay != nullptr);
    for (const auto choice : {"default", "monochrome", "tinted"}) {
        for (const auto asset : {"display", "network", "sound"}) {
            QVERIFY(page->findChild<QObject *>(QStringLiteral("iconPreview-%1-%2")
                                                   .arg(QString::fromLatin1(choice),
                                                        QString::fromLatin1(asset)))
                    != nullptr);
        }
    }
    QCOMPARE(tintedDisplay->property("appearanceOverride").toString(),
             QStringLiteral("tinted"));
    QVERIFY(tintedDisplay->property("hasTintColorOverride").toBool());
    QCOMPARE(tintedDisplay->property("tintColorOverride").value<QColor>(),
             QColor(QStringLiteral("#0a84ff")));

    QCOMPARE(themeController.iconAppearance(), QStringLiteral("default"));
    const QString initialThemePreference = themeController.themePreference();
    const int initialShellStyle = themeController.shellStyle();
    const QString initialAccent = themeController.accentHex();
    QVERIFY(defaultAppearance->property("selected").toBool());
    QVERIFY(!monochrome->property("selected").toBool());
    QVERIFY(!tinted->property("selected").toBool());

    QQmlExpression projectedAppearance(qmlContext(page), page,
                                       QStringLiteral("Components.Theme.iconAppearance"));
    QCOMPARE(projectedAppearance.evaluate().toString(), QStringLiteral("default"));

    QVERIFY(QMetaObject::invokeMethod(monochrome, "activate"));
    QCOMPARE(themeController.iconAppearance(), QStringLiteral("monochrome"));
    QVERIFY(QMetaObject::invokeMethod(tinted, "activate"));
    QCOMPARE(themeController.iconAppearance(), QStringLiteral("tinted"));
    QVERIFY(QMetaObject::invokeMethod(defaultAppearance, "activate"));
    QCOMPARE(themeController.iconAppearance(), QStringLiteral("default"));
    QCOMPARE(themeController.themePreference(), initialThemePreference);
    QCOMPARE(themeController.shellStyle(), initialShellStyle);
    QCOMPARE(themeController.accentHex(), initialAccent);
    QCOMPARE(projectedAppearance.evaluate().toString(), QStringLiteral("default"));

    QFile replacement(themeController.configPath());
    QVERIFY(replacement.open(QIODevice::WriteOnly | QIODevice::Truncate));
    replacement.write(QJsonDocument(QJsonObject{
        {QStringLiteral("icon_appearance"), QStringLiteral("TiNtEd")},
        {QStringLiteral("accent"), QStringLiteral("#30d158")},
    }).toJson(QJsonDocument::Compact));
    replacement.close();

    QTRY_COMPARE_WITH_TIMEOUT(themeController.iconAppearance(), QStringLiteral("tinted"), 1500);
    QTRY_COMPARE_WITH_TIMEOUT(themeController.accentHex(), QStringLiteral("#30d158"), 1500);
    QTRY_COMPARE_WITH_TIMEOUT(tintedDisplay->property("tintColorOverride").value<QColor>(),
                              QColor(QStringLiteral("#30d158")), 1500);
    QTRY_VERIFY_WITH_TIMEOUT(projectedAppearance.evaluate().toString() == QStringLiteral("tinted"),
                             1500);
    QVERIFY(tinted->property("selected").toBool());
    QVERIFY(!defaultAppearance->property("selected").toBool());

    QVERIFY(replacement.open(QIODevice::WriteOnly | QIODevice::Truncate));
    replacement.write(QByteArrayLiteral(R"({"accent":"#bf5af2"})"));
    replacement.close();

    QTRY_COMPARE_WITH_TIMEOUT(themeController.iconAppearance(), QStringLiteral("default"), 1500);
    QTRY_COMPARE_WITH_TIMEOUT(themeController.accentHex(), QStringLiteral("#bf5af2"), 1500);
    QTRY_COMPARE_WITH_TIMEOUT(tintedDisplay->property("tintColorOverride").value<QColor>(),
                              QColor(QStringLiteral("#bf5af2")), 1500);
    QTRY_VERIFY_WITH_TIMEOUT(projectedAppearance.evaluate().toString() == QStringLiteral("default"),
                             1500);
    QVERIFY(defaultAppearance->property("selected").toBool());
    QVERIFY(!tinted->property("selected").toBool());
    QVERIFY2(qmlWarnings.isEmpty(),
             qPrintable(qmlWarnings.isEmpty() ? QString() : qmlWarnings.constFirst().toString()));
}

void SettingsQmlSmokeTest::appearanceAccentChoicesUpdateController()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController(directory.filePath(QStringLiteral("theme.json")), nullptr,
                                    [] { return Qt::ColorScheme::Dark; });
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(settingsController.navigateTo(QStringLiteral("appearance")));
    QObject *root = engine.rootObjects().constFirst();
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("appearancePage")) != nullptr,
                             1000);
    QObject *page = root->findChild<QObject *>(QStringLiteral("appearancePage"));
    QVERIFY(page != nullptr);

    const QList<QPair<const char *, const char *>> accentOptions = {
        {"accentOption-blue", "#0a84ff"},
        {"accentOption-purple", "#bf5af2"},
        {"accentOption-red", "#ff453a"},
        {"accentOption-orange", "#ff9f0a"},
        {"accentOption-yellow", "#ffd60a"},
        {"accentOption-green", "#30d158"},
        {"accentOption-teal", "#40c8e0"},
    };
    QList<QObject *> swatches;
    for (const auto &[objectName, value] : accentOptions) {
        QObject *swatch = page->findChild<QObject *>(QString::fromLatin1(objectName));
        QVERIFY2(swatch != nullptr, objectName);
        QCOMPARE(swatch->property("accentValue").toString(), QString::fromLatin1(value));
        swatches.append(swatch);
    }

    QCOMPARE(themeController.accentHex(), QStringLiteral("#0a84ff"));
    QCOMPARE(themeController.themePreference(), QStringLiteral("auto"));
    QCOMPARE(themeController.shellStyle(), 1);
    QVERIFY(swatches.at(0)->property("selected").toBool());
    for (int index = 1; index < swatches.size(); ++index)
        QVERIFY(!swatches.at(index)->property("selected").toBool());

    QVERIFY(QMetaObject::invokeMethod(swatches.at(5), "activate"));
    QCOMPARE(themeController.accentHex(), QStringLiteral("#30d158"));
    QCOMPARE(themeController.themePreference(), QStringLiteral("auto"));
    QCOMPARE(themeController.shellStyle(), 1);
    QVERIFY(swatches.at(5)->property("selected").toBool());
    QVERIFY(!swatches.at(0)->property("selected").toBool());

    QVERIFY(QMetaObject::invokeMethod(swatches.at(1), "activate"));
    QCOMPARE(themeController.accentHex(), QStringLiteral("#bf5af2"));
    QCOMPARE(themeController.themePreference(), QStringLiteral("auto"));
    QCOMPARE(themeController.shellStyle(), 1);
    QVERIFY(swatches.at(1)->property("selected").toBool());
    QVERIFY(!swatches.at(5)->property("selected").toBool());

    QQmlExpression projectedAccent(qmlContext(page), page,
                                   QStringLiteral("Components.Theme.accentHex"));
    QCOMPARE(projectedAccent.evaluate().toString(), QStringLiteral("#bf5af2"));

    QFile replacement(themeController.configPath());
    QVERIFY(replacement.open(QIODevice::WriteOnly | QIODevice::Truncate));
    replacement.write(QJsonDocument(QJsonObject{
        {QStringLiteral("theme_preference"), QStringLiteral("light")},
        {QStringLiteral("shell_style"), 0},
        {QStringLiteral("accent"), QStringLiteral("#123456")},
    }).toJson(QJsonDocument::Compact));
    replacement.close();

    QTRY_COMPARE_WITH_TIMEOUT(themeController.accentHex(), QStringLiteral("#123456"), 1500);
    QTRY_COMPARE_WITH_TIMEOUT(themeController.themePreference(), QStringLiteral("light"), 1500);
    QTRY_COMPARE_WITH_TIMEOUT(themeController.shellStyle(), 0, 1500);
    QTRY_VERIFY_WITH_TIMEOUT(projectedAccent.evaluate().toString() == QStringLiteral("#123456"),
                             1500);
    QTRY_VERIFY_WITH_TIMEOUT([&swatches]() {
        return std::none_of(swatches.cbegin(), swatches.cend(), [](QObject *swatch) {
            return swatch->property("selected").toBool();
        });
    }(), 1500);
}

void SettingsQmlSmokeTest::loadsWallpaperRouteFromHubOffscreen()
{
    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController;
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(settingsController.navigateTo(QStringLiteral("customization")));
    QObject *root = engine.rootObjects().constFirst();
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("settingsHubPage")) != nullptr, 1000);
    QObject *hub = root->findChild<QObject *>(QStringLiteral("settingsHubPage"));
    QVERIFY(hub != nullptr);
    QObject *row = findVisualItem(qobject_cast<QQuickItem *>(hub), QStringLiteral("hubNavigationRow-wallpaper"));
    QVERIFY(row != nullptr);
    QVERIFY(QMetaObject::invokeMethod(row, "clicked"));
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("wallpaperPage")) != nullptr, 1000);
    QObject *page = root->findChild<QObject *>(QStringLiteral("wallpaperPage"));
    QVERIFY(page != nullptr);
    QObject *scroll = page->findChild<QObject *>(QStringLiteral("wallpaperScrollPage"));
    QVERIFY(scroll != nullptr);
    QCOMPARE(scroll->property("contentMargins").toInt(), 28);
    for (const auto name : {"currentWallpaperCard", "wallpaperPreview", "transitionCard",
                            "wallpaperLibraryCard", "dynamicWallpapersSection",
                            "userWallpapersSection", "landscapesSection"}) {
        QVERIFY2(page->findChild<QObject *>(QString::fromLatin1(name)) != nullptr, name);
    }
    QObject *preview = page->findChild<QObject *>(QStringLiteral("wallpaperPreview"));
    QCOMPARE(preview->property("width").toInt(), 180);
    QCOMPARE(preview->property("height").toInt(), 112);
    for (const auto name : {"wallpaperFileDialog", "wallpaperNameDialog", "wallpaperNameInput",
                            "userWallpapersAddButton", "wallpaperRemoveDialog",
                            "wallpaperRemoveConfirmButton"}) {
        QVERIFY2(page->findChild<QObject *>(QString::fromLatin1(name)) != nullptr, name);
    }
    QObject *dialog = page->findChild<QObject *>(QStringLiteral("wallpaperNameDialog"));
    QVERIFY(dialog != nullptr);
    QCOMPARE(dialog->property("width").toInt(), 320);
    QCOMPARE(dialog->property("padding").toInt(), 20);
    QObject *input = page->findChild<QObject *>(QStringLiteral("wallpaperNameInput"));
    QVERIFY(input != nullptr);
    QCOMPARE(input->property("maximumLength").toInt(), 128);
    QCOMPARE(input->property("placeholderText").toString(), QStringLiteral("e.g. Tokyo Night"));
    page->setProperty("pendingAddsToLibrary", true);
    QCOMPARE(input->property("placeholderText").toString(), QStringLiteral("e.g. Mountain Sunset"));
    page->setProperty("pendingAddsToLibrary", false);
    QVERIFY(input->property("enabled").toBool());
    QObject *feedback = page->findChild<QObject *>(QStringLiteral("wallpaperFeedback"));
    QVERIFY(feedback != nullptr);
    QVERIFY(feedback->property("height").toInt() > 0);
    QObject *confirm = page->findChild<QObject *>(QStringLiteral("wallpaperNameConfirmButton"));
    QVERIFY(confirm != nullptr);
    QCOMPARE(confirm->property("text").toString(), QStringLiteral("Confirm"));
    QVERIFY(confirm->property("enabled").toBool());

    QObject *allWorkspaces = page->findChild<QObject *>(QStringLiteral("allWorkspacesToggle"));
    QObject *blurredWallpaper = page->findChild<QObject *>(QStringLiteral("blurredWallpaperToggle"));
    QObject *transition = page->findChild<QObject *>(QStringLiteral("transitionSelector"));
    QVERIFY(allWorkspaces != nullptr);
    QVERIFY(blurredWallpaper != nullptr);
    QVERIFY(transition != nullptr);
    QVERIFY(!allWorkspaces->property("enabled").toBool());
    QVERIFY(!blurredWallpaper->property("enabled").toBool());
    QVERIFY(!transition->property("enabled").toBool());
    QCOMPARE(transition->property("selectedIndex").toInt(), 0);
    QCOMPARE(settingsController.selectedSidebarId(), QStringLiteral("customization"));
    QVERIFY(page->findChild<QObject *>(QStringLiteral("wallpaperPreview")) != nullptr);
}

void SettingsQmlSmokeTest::loadsDockRouteFromHubOffscreen()
{
    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController;
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(settingsController.navigateTo(QStringLiteral("customization")));
    QObject *root = engine.rootObjects().constFirst();
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("settingsHubPage")) != nullptr, 1000);
    QObject *hub = root->findChild<QObject *>(QStringLiteral("settingsHubPage"));
    QVERIFY(hub != nullptr);
    QObject *row = findVisualItem(qobject_cast<QQuickItem *>(hub), QStringLiteral("hubNavigationRow-dock"));
    QVERIFY(row != nullptr);
    QVERIFY(QMetaObject::invokeMethod(row, "clicked"));
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("dockPage")) != nullptr, 1000);
    QObject *page = root->findChild<QObject *>(QStringLiteral("dockPage"));
    QVERIFY(page != nullptr);
    QCOMPARE(settingsController.selectedSidebarId(), QStringLiteral("customization"));
    QQuickItem *sidebar = qobject_cast<QQuickItem *>(root->findChild<QObject *>(QStringLiteral("settingsSidebar")));
    QVERIFY(sidebar != nullptr);
    QQuickItem *customizationRow = findVisualItem(sidebar, QStringLiteral("settingsSidebarRow-customization"));
    QVERIFY(customizationRow != nullptr);
    QVERIFY(customizationRow->property("selected").toBool());
    QVERIFY(findVisualItem(sidebar, QStringLiteral("settingsSidebarRow-dock")) == nullptr);
    QVERIFY(page->findChild<QObject *>(QStringLiteral("dockPreview")) != nullptr);
    const struct SliderExpectation {
        const char *objectName;
        const char *defaultProperty;
    } sliders[] = {
        {"iconSizeSlider", "defaultIconSize"},
        {"itemSpacingSlider", "defaultItemSpacing"},
        {"panelPaddingSlider", "defaultPanelPadding"},
        {"edgeMarginSlider", "defaultEdgeMargin"},
        {"cornerRadiusSlider", "defaultCornerRadius"},
        {"magnificationScaleSlider", "defaultMagnificationScale"},
        {"magnificationRadiusSlider", "defaultMagnificationRadius"},
        {"animationSpeedSlider", "defaultAnimationSpeed"},
        {"indicatorSizeSlider", "defaultIndicatorSize"},
    };
    for (const auto &expectation : sliders) {
        QObject *slider = page->findChild<QObject *>(QString::fromLatin1(expectation.objectName));
        QVERIFY2(slider != nullptr, expectation.objectName);
        QVERIFY(slider->property("modelValueEnabled").toBool());
        QVERIFY(slider->property("detentEnabled").toBool());
        QCOMPARE(slider->property("detentValue").toDouble(),
                 settingsController.dock()->property(expectation.defaultProperty).toDouble());
        QVERIFY(!slider->property("valueText").toString().isEmpty());
    }

    QObject *restoreButton = page->findChild<QObject *>(QStringLiteral("restoreDefaultsButton"));
    QObject *restoreToast = page->findChild<QObject *>(QStringLiteral("dockRestoreToast"));
    QObject *undoButton = page->findChild<QObject *>(QStringLiteral("dockRestoreUndoButton"));
    QVERIFY(restoreButton != nullptr);
    QVERIFY(restoreToast != nullptr);
    QVERIFY(undoButton != nullptr);
    QCOMPARE(restoreButton->property("enabled").toBool(),
             !settingsController.dock()->property("isDefault").toBool());
    QCOMPARE(restoreToast->property("visible").toBool(),
             settingsController.dock()->property("canUndoRestore").toBool());
    QCOMPARE(undoButton->property("enabled").toBool(),
             settingsController.dock()->property("canUndoRestore").toBool());
}

void SettingsQmlSmokeTest::navigatesBackAndForwardFromHub()
{
    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController;
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(settingsController.navigateTo(QStringLiteral("customization")));
    QVERIFY(settingsController.navigateTo(QStringLiteral("dock")));
    QObject *root = engine.rootObjects().constFirst();
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("dockPage")) != nullptr, 1000);

    settingsController.goBack();
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("settingsHubPage")) != nullptr, 1000);
    QVERIFY(settingsController.canGoForward());

    settingsController.goForward();
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("dockPage")) != nullptr, 1000);
    QVERIFY(!settingsController.canGoForward());
}

void SettingsQmlSmokeTest::resolvesHubHeroIconsByMetadataPrecedence()
{
    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController;
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QVERIFY(settingsController.navigateTo(QStringLiteral("customization")));
    QObject *root = engine.rootObjects().constFirst();
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("settingsHubPage")) != nullptr, 1000);
    QObject *hub = root->findChild<QObject *>(QStringLiteral("settingsHubPage"));
    QVERIFY(hub != nullptr);

    const QString providedSource = QStringLiteral("qrc:/provided-icon.svg");
    const QVariantMap keyedDescriptor{
        {QStringLiteral("iconKey"), QStringLiteral("theme")},
        {QStringLiteral("iconSource"), providedSource},
    };
    QVariant resolved;
    QVERIFY(QMetaObject::invokeMethod(hub, "iconSourceFor", Q_RETURN_ARG(QVariant, resolved),
                                      Q_ARG(QVariant, QVariant::fromValue(keyedDescriptor))));
    QVERIFY(resolved.toString().endsWith(QStringLiteral("/theme.svg")));
    QVERIFY(resolved.toString() != providedSource);

    const QVariantMap sourceDescriptor{{QStringLiteral("iconSource"), providedSource}};
    resolved.clear();
    QVERIFY(QMetaObject::invokeMethod(hub, "iconSourceFor", Q_RETURN_ARG(QVariant, resolved),
                                      Q_ARG(QVariant, QVariant::fromValue(sourceDescriptor))));
    QCOMPARE(resolved.toString(), providedSource);

    const QVariantMap symbolDescriptor{{QStringLiteral("sym"), QStringLiteral("S")}};
    resolved.clear();
    QVERIFY(QMetaObject::invokeMethod(hub, "iconSourceFor", Q_RETURN_ARG(QVariant, resolved),
                                      Q_ARG(QVariant, QVariant::fromValue(symbolDescriptor))));
    QVERIFY(resolved.toString().isEmpty());
}

void SettingsQmlSmokeTest::sidebarHidesNestedDestinations()
{
    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController;
    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();
    QObject *sidebar = root->findChild<QObject *>(QStringLiteral("settingsSidebar"));
    QVERIFY(sidebar != nullptr);
    QVERIFY(sidebar->findChild<QObject *>(QStringLiteral("settingsSidebarRow-wallpaper")) == nullptr);
    QVERIFY(sidebar->findChild<QObject *>(QStringLiteral("settingsSidebarRow-dock")) == nullptr);
    QCOMPARE(settingsController.navigationModel()->rowCount(), 12);
}

void SettingsQmlSmokeTest::wallpaperTranslationKeysExist()
{
    QFile catalog(QDir(QStringLiteral(ASTREA_ECLIPSE_SOURCE_DIR))
                      .filePath(QStringLiteral("Settings/assets/i18n/en_US.json")));
    QVERIFY(catalog.open(QIODevice::ReadOnly));
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(catalog.readAll(), &parseError);
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QVERIFY(document.isObject());

    const auto messages = document.object();
    const QStringList requiredKeys{
        QStringLiteral("settings.nav.customization"),
        QStringLiteral("settings.nav.customization.subtitle"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.option.simple"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.option.fade"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.option.left"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.option.right"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.option.top"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.option.bottom"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.option.wipe"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.option.wave"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.option.grow"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.option.center"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.option.outer"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.option.any"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.option.random"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.label.dynamic_wallpapers"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.label.landscapes"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.label.transition"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.sublabel.awww_wallpaper_animation"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.change"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.action.remove"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.current"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.choose_wallpaper"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.name_this_wallpaper"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.no_wallpapers_found"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.my_wallpaper"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.preview_fail"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.remove_named_wallpaper"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.remove_help"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.original_not_affected"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.cancel"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.confirm"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.placeholder_change"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.placeholder_add_user"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.show_on_all_workspaces"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.use_blurred_wallpaper"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.user_wallpapers"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.wallpaper_library"),
        QStringLiteral("apps.settings.pages.paper.screensaver.text.screensaver"),
        QStringLiteral("apps.settings.pages.paper.lockscreen.text.lockscreen"),
        QStringLiteral("settings.nav.dock"),
        QStringLiteral("apps.settings.pages.appearance.dock.text.preview"),
        QStringLiteral("apps.settings.pages.appearance.dock.text.layout"),
        QStringLiteral("apps.settings.pages.appearance.dock.text.behavior"),
        QStringLiteral("apps.settings.pages.appearance.dock.text.indicators"),
        QStringLiteral("apps.settings.pages.appearance.dock.label.icon_size"),
        QStringLiteral("apps.settings.pages.appearance.dock.label.icon_spacing"),
        QStringLiteral("apps.settings.pages.appearance.dock.label.panel_padding"),
        QStringLiteral("apps.settings.pages.appearance.dock.label.position"),
        QStringLiteral("apps.settings.pages.appearance.dock.label.floating"),
        QStringLiteral("apps.settings.pages.appearance.dock.label.edge_margin"),
        QStringLiteral("apps.settings.pages.appearance.dock.label.corner_radius"),
        QStringLiteral("apps.settings.pages.appearance.dock.label.hover_effect"),
        QStringLiteral("apps.settings.pages.appearance.dock.label.magnification_strength"),
        QStringLiteral("apps.settings.pages.appearance.dock.label.magnification_radius"),
        QStringLiteral("apps.settings.pages.appearance.dock.label.auto_hide"),
        QStringLiteral("apps.settings.pages.appearance.dock.label.animations"),
        QStringLiteral("apps.settings.pages.appearance.dock.label.animation_speed"),
        QStringLiteral("apps.settings.pages.appearance.dock.label.indicator_style"),
        QStringLiteral("apps.settings.pages.appearance.dock.label.indicator_size"),
        QStringLiteral("apps.settings.pages.appearance.dock.option.bottom"),
        QStringLiteral("apps.settings.pages.appearance.dock.option.left"),
        QStringLiteral("apps.settings.pages.appearance.dock.option.right"),
        QStringLiteral("apps.settings.pages.appearance.dock.option.none"),
        QStringLiteral("apps.settings.pages.appearance.dock.option.lift"),
        QStringLiteral("apps.settings.pages.appearance.dock.option.magnification"),
        QStringLiteral("apps.settings.pages.appearance.dock.option.never"),
        QStringLiteral("apps.settings.pages.appearance.dock.option.intelligent"),
        QStringLiteral("apps.settings.pages.appearance.dock.option.always"),
        QStringLiteral("apps.settings.pages.appearance.dock.option.line"),
        QStringLiteral("apps.settings.pages.appearance.dock.option.dot"),
        QStringLiteral("apps.settings.pages.appearance.dock.unit.pixels"),
        QStringLiteral("apps.settings.pages.appearance.dock.action.restore_defaults"),
        QStringLiteral("apps.settings.pages.appearance.dock.status.restored"),
        QStringLiteral("apps.settings.pages.appearance.dock.action.undo"),
    };
    for (const auto &key : requiredKeys)
        QVERIFY2(messages.contains(key), qPrintable(QStringLiteral("Missing key: ") + key));
}

void SettingsQmlSmokeTest::wallpaperPreviewAndRemovalUseRealPointerEvents()
{
    QTemporaryDir runtime;
    QTemporaryDir images;
    QVERIFY(runtime.isValid());
    QVERIFY(images.isValid());
    QVERIFY(QDir(runtime.path()).mkpath(QStringLiteral("astrea-shell")));

    const auto previousRuntime = qgetenv("XDG_RUNTIME_DIR");
    const auto hadPreviousRuntime = qEnvironmentVariableIsSet("XDG_RUNTIME_DIR");
    qputenv("XDG_RUNTIME_DIR", runtime.path().toUtf8());

    const auto currentPath = writeWallpaperImage(
        images.filePath(QStringLiteral("Current # Café 雪.png")), QColor("#466b9a"));
    const auto inactivePath = writeWallpaperImage(
        images.filePath(QStringLiteral("Snow # Café 雪.png")), QColor("#9a6b46"));
    const auto endpoint = QDir(runtime.path()).filePath(QStringLiteral("astrea-shell/wallpaper.sock"));
    QLocalServer server;
    QVERIFY(server.listen(endpoint));

    const auto currentId = QStringLiteral("astrea://wallpaper/user/") + QString(64, QLatin1Char('a'));
    const auto inactiveId = QStringLiteral("astrea://wallpaper/user/") + QString(64, QLatin1Char('b'));
    const auto systemId = QStringLiteral("astrea://wallpaper/system/landscape");
    QStringList requests;
    QByteArray requestBuffer;
    bool removed = false;
    auto effectiveId = currentId;

    const auto descriptor = [](const QString &logicalId,
                               const QString &source,
                               const QString &previewSource,
                               const QString &displayName,
                               const QString &origin) {
        return QJsonObject{{QStringLiteral("logicalId"), logicalId},
                           {QStringLiteral("kind"), QStringLiteral("image")},
                           {QStringLiteral("origin"), origin},
                           {QStringLiteral("source"), source},
                           {QStringLiteral("resolvedSource"), source},
                           {QStringLiteral("previewSource"), previewSource},
                           {QStringLiteral("displayName"), displayName}};
    };

    const auto makeSnapshot = [&] {
        const auto currentPathForSnapshot = effectiveId == inactiveId ? inactivePath : currentPath;
        const auto currentSource = effectiveId == inactiveId
            ? QStringLiteral(":/private/inactive.png")
            : QStringLiteral(":/private/current.png");
        const QJsonObject configured{{QStringLiteral("logicalId"), effectiveId},
                                     {QStringLiteral("source"), currentSource},
                                     {QStringLiteral("resolvedSource"), currentSource},
                                     {QStringLiteral("fit"), QStringLiteral("cover")}};
        const QJsonObject effective{{QStringLiteral("logicalId"), effectiveId},
                                    {QStringLiteral("source"), currentSource},
                                    {QStringLiteral("resolvedSource"), currentSource},
                                    {QStringLiteral("previewSource"), currentPathForSnapshot},
                                    {QStringLiteral("fit"), QStringLiteral("cover")}};
        return QJsonObject{{QStringLiteral("configured"), configured},
                           {QStringLiteral("factoryDefault"), effective},
                           {QStringLiteral("effective"), effective},
                           {QStringLiteral("state"), QStringLiteral("ready")},
                           {QStringLiteral("fallback"), QStringLiteral("none")},
                           {QStringLiteral("generation"), 2},
                           {QStringLiteral("errorCode"), QString()},
                           {QStringLiteral("lastError"), QString()}};
    };

    const auto makeWallpapers = [&] {
        QJsonArray wallpapers{
            descriptor(currentId,
                       QStringLiteral(":/private/current.png"),
                       currentPath,
                       QStringLiteral("Current"),
                       QStringLiteral("user")),
            descriptor(systemId,
                       QStringLiteral(":/landscape.jpg"),
                       QStringLiteral("qrc:/landscape.jpg"),
                       QStringLiteral("Landscape"),
                       QStringLiteral("system")),
        };
        if (!removed) {
            wallpapers.insert(1, descriptor(inactiveId,
                                             QStringLiteral(":/private/inactive.png"),
                                             inactivePath,
                                             QStringLiteral("Snow"),
                                             QStringLiteral("user")));
        }
        return wallpapers;
    };

    connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [&, socket] {
            requestBuffer += socket->readAll();
            if (!requestBuffer.endsWith('\n'))
                return;
            const auto line = QString::fromUtf8(requestBuffer).trimmed();
            requestBuffer.clear();
            requests.append(line);

            if (line.startsWith(QStringLiteral("wallpaper remove"))) {
                removed = true;
            } else if (line.startsWith(QStringLiteral("wallpaper set"))) {
                effectiveId = line.contains(inactiveId) ? inactiveId : currentId;
            }
            const auto wallpapers = makeWallpapers();
            socket->write(QJsonDocument(QJsonObject{
                                            {QStringLiteral("ok"), true},
                                            {QStringLiteral("completed"), true},
                                            {QStringLiteral("snapshot"), makeSnapshot()},
                                            {QStringLiteral("wallpapers"), wallpapers},
                                        })
                              .toJson(QJsonDocument::Compact)
                          + '\n');
            socket->flush();
        });
    });

    SettingsController settingsController;
    SettingsTranslationController translationController;
    ThemeController themeController;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("SettingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("I18n"), &translationController);
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &themeController);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Settings/qml/Main.qml")));

    QCOMPARE(engine.rootObjects().size(), 1);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    window->show();
    QVERIFY(settingsController.navigateTo(QStringLiteral("customization")));
    QTRY_VERIFY_WITH_TIMEOUT(window->findChild<QObject *>(QStringLiteral("settingsHubPage")) != nullptr,
                             1000);
    auto *hub = qobject_cast<QQuickItem *>(window->findChild<QObject *>(QStringLiteral("settingsHubPage")));
    QVERIFY(hub != nullptr);
    auto *wallpaperRow = findVisualItem(hub, QStringLiteral("hubNavigationRow-wallpaper"));
    QVERIFY(wallpaperRow != nullptr);
    const auto wallpaperRowPoint = wallpaperRow->mapToItem(
        window->contentItem(), QPointF(wallpaperRow->width() / 2, wallpaperRow->height() / 2));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, wallpaperRowPoint.toPoint());

    QTRY_VERIFY_WITH_TIMEOUT(window->findChild<QObject *>(QStringLiteral("wallpaperPage")) != nullptr,
                             1000);
    auto *page = qobject_cast<QQuickItem *>(window->findChild<QObject *>(QStringLiteral("wallpaperPage")));
    QVERIFY(page != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(!requests.isEmpty(), 1500);
    QTRY_VERIFY_WITH_TIMEOUT(page->findChild<QObject *>(QStringLiteral("wallpaperPreviewImage")) != nullptr,
                             1500);
    auto *previewImage = page->findChild<QObject *>(QStringLiteral("wallpaperPreviewImage"));
    QTRY_VERIFY_WITH_TIMEOUT(previewImage->property("source").toUrl().isLocalFile(), 1500);
    QTRY_COMPARE_WITH_TIMEOUT(previewImage->property("status").toInt(), 1, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(settingsController.wallpaper()->userWallpapers().size(), 2, 1500);

    const auto inactiveTileName = QStringLiteral("wallpaperTile-") + inactiveId;
    const auto currentTileName = QStringLiteral("wallpaperTile-") + currentId;
    const auto systemTileName = QStringLiteral("wallpaperTile-") + systemId;
    QTRY_VERIFY_WITH_TIMEOUT(findVisualItem(page, inactiveTileName) != nullptr, 1500);
    QTRY_VERIFY_WITH_TIMEOUT(findVisualItem(page, currentTileName) != nullptr, 1500);
    QTRY_VERIFY_WITH_TIMEOUT(findVisualItem(page, systemTileName) != nullptr, 1500);
    auto *inactiveTile = findVisualItem(page, inactiveTileName);
    auto *currentTile = findVisualItem(page, currentTileName);
    QVERIFY(inactiveTile != nullptr);
    QVERIFY(currentTile != nullptr);
    auto *inactiveImage = findVisualItem(
        page, QStringLiteral("wallpaperTileImage-") + inactiveId);
    QVERIFY(inactiveImage != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(inactiveImage->property("source").toUrl().isLocalFile(), 1500);
    QTRY_COMPARE_WITH_TIMEOUT(inactiveImage->property("status").toInt(), 1, 3000);
    QCOMPARE(currentTile->property("isCurrent").toBool(), true);
    QCOMPARE(settingsController.wallpaper()->landscapeWallpapers().size(), 1);

    auto countRequests = [&](const QString &prefix) {
        return std::count_if(requests.cbegin(), requests.cend(), [&](const QString &request) {
            return request.startsWith(prefix);
        });
    };
    auto pointFor = [&](QQuickItem *item, const QPointF &localPoint) {
        return item->mapToItem(window->contentItem(), localPoint).toPoint();
    };

    auto *contextMenu = page->findChild<QObject *>(QStringLiteral("wallpaperContextMenu"));
    QVERIFY(contextMenu != nullptr);
    auto *contextRemoveAction = page->findChild<QObject *>(
        QStringLiteral("wallpaperContextRemoveAction"));
    QVERIFY(contextRemoveAction != nullptr);

    const QPointF bLocalPoint(inactiveTile->width() / 2, inactiveTile->height() / 2);
    const auto bPoint = pointFor(inactiveTile, bLocalPoint);
    const auto bMenuPoint = inactiveTile->mapToItem(page, bLocalPoint).toPoint();
    const auto removeCountBefore = countRequests(QStringLiteral("wallpaper remove "));
    const auto setCountBefore = countRequests(QStringLiteral("wallpaper set "));
    QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, bPoint);
    QTRY_VERIFY_WITH_TIMEOUT(contextMenu->property("menuOpen").toBool(), 1000);
    QCOMPARE(contextMenu->property("requestedX").toReal(), bMenuPoint.x());
    QCOMPARE(contextMenu->property("requestedY").toReal(), bMenuPoint.y());
    QVERIFY(contextMenu->property("menuPositioned").toBool());
    QVERIFY(contextRemoveAction->property("visible").toBool());
    QVERIFY(contextRemoveAction->property("actionEnabled").toBool());
    QCOMPARE(contextRemoveAction->property("destructive").toBool(), true);
    QCOMPARE(countRequests(QStringLiteral("wallpaper set ")), setCountBefore);
    QCOMPARE(settingsController.wallpaper()->effectiveId(), currentId);

    auto *contextRemoveItem = qobject_cast<QQuickItem *>(contextRemoveAction);
    QVERIFY(contextRemoveItem != nullptr);
    QSignalSpy actionTriggered(contextRemoveAction, SIGNAL(triggered()));
    QVERIFY(actionTriggered.isValid());
    const auto contextRemovePoint = pointFor(
        contextRemoveItem, QPointF(contextRemoveItem->width() / 2, contextRemoveItem->height() / 2));
    QTest::mouseMove(window, contextRemovePoint);
    QTRY_VERIFY_WITH_TIMEOUT(contextRemoveAction->property("hovered").toBool(), 1000);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, contextRemovePoint);
    QCOMPARE(actionTriggered.count(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(!contextMenu->property("menuOpen").toBool(), 1000);
    auto *removeDialog = page->findChild<QObject *>(QStringLiteral("wallpaperRemoveDialog"));
    QVERIFY(removeDialog != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(removeDialog->property("visible").toBool(), 1000);
    QCOMPARE(countRequests(QStringLiteral("wallpaper remove ")), removeCountBefore);
    QVERIFY(!removed);
    QCOMPARE(settingsController.wallpaper()->effectiveId(), currentId);

    auto *confirmButton = page->findChild<QObject *>(QStringLiteral("wallpaperRemoveConfirmButton"));
    QVERIFY(confirmButton != nullptr);
    auto *confirmItem = qobject_cast<QQuickItem *>(confirmButton);
    QVERIFY(confirmItem != nullptr);
    const auto confirmPoint = confirmItem->mapToItem(
        window->contentItem(), QPointF(confirmItem->width() / 2, confirmItem->height() / 2));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, confirmPoint.toPoint());
    QTRY_VERIFY_WITH_TIMEOUT(removed, 1500);
    const auto removeRequest = QStringLiteral("wallpaper remove {\"id\":\"%1\"}").arg(inactiveId);
    QCOMPARE(requests.count(removeRequest), 1);
    QTRY_VERIFY_WITH_TIMEOUT(findVisualItem(page, inactiveTileName) == nullptr, 1500);
    QVERIFY(findVisualItem(page, currentTileName) != nullptr);
    QCOMPARE(settingsController.wallpaper()->effectiveId(), currentId);
    QCOMPARE(settingsController.wallpaper()->userWallpapers().size(), 1);

    auto *currentTileAfterRemoval = findVisualItem(page, currentTileName);
    QVERIFY(currentTileAfterRemoval != nullptr);
    const auto currentPoint = pointFor(currentTileAfterRemoval,
                                       QPointF(currentTileAfterRemoval->width() - 2,
                                               currentTileAfterRemoval->height() / 2));
    QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, currentPoint);
    QTRY_VERIFY_WITH_TIMEOUT(contextMenu->property("menuOpen").toBool(), 1000);
    QVERIFY(contextRemoveAction->property("visible").toBool());
    QCOMPARE(contextRemoveAction->property("actionEnabled").toBool(), false);
    const auto removeCountBeforeCurrent = countRequests(QStringLiteral("wallpaper remove "));
    auto *currentContextRemoveItem = qobject_cast<QQuickItem *>(contextRemoveAction);
    QVERIFY(currentContextRemoveItem != nullptr);
    const auto currentContextRemovePoint = pointFor(
        currentContextRemoveItem,
        QPointF(currentContextRemoveItem->width() / 2, currentContextRemoveItem->height() / 2));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, currentContextRemovePoint);
    QTest::qWait(50);
    QCOMPARE(countRequests(QStringLiteral("wallpaper remove ")), removeCountBeforeCurrent);
    QCOMPARE(settingsController.wallpaper()->effectiveId(), currentId);
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY_WITH_TIMEOUT(!contextMenu->property("menuOpen").toBool(), 1000);

    auto *systemTile = findVisualItem(page, systemTileName);
    QVERIFY(systemTile != nullptr);
    const auto systemPoint = pointFor(systemTile,
                                      QPointF(systemTile->width() / 2, systemTile->height() - 2));
    QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, systemPoint);
    QTRY_VERIFY_WITH_TIMEOUT(!contextMenu->property("menuOpen").toBool(), 1000);
    QCOMPARE(countRequests(QStringLiteral("wallpaper remove ")), removeCountBeforeCurrent);

    removed = false;
    effectiveId = currentId;
    settingsController.wallpaper()->refreshLibrary();
    QTRY_COMPARE_WITH_TIMEOUT(settingsController.wallpaper()->userWallpapers().size(), 2, 1500);
    QTRY_VERIFY_WITH_TIMEOUT(findVisualItem(page, inactiveTileName) != nullptr, 1500);
    auto *inactiveTileForSelection = findVisualItem(page, inactiveTileName);
    QVERIFY(inactiveTileForSelection != nullptr);
    const auto setCountBeforeSelection = countRequests(QStringLiteral("wallpaper set "));
    QTest::mouseClick(window, Qt::MiddleButton, Qt::NoModifier,
                      pointFor(inactiveTileForSelection,
                               QPointF(inactiveTileForSelection->width() / 2,
                                       inactiveTileForSelection->height() / 2)));
    QTest::qWait(50);
    QCOMPARE(countRequests(QStringLiteral("wallpaper set ")), setCountBeforeSelection);
    QVERIFY(!contextMenu->property("menuOpen").toBool());
    const auto selectionPoint = pointFor(
        inactiveTileForSelection,
        QPointF(inactiveTileForSelection->width() / 2, inactiveTileForSelection->height() / 2));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, selectionPoint);
    QTRY_COMPARE_WITH_TIMEOUT(settingsController.wallpaper()->effectiveId(), inactiveId, 1500);
    QCOMPARE(countRequests(QStringLiteral("wallpaper set ")), setCountBeforeSelection + 1);
    QVERIFY(requests.last().contains(inactiveId));
    QVERIFY(!contextMenu->property("menuOpen").toBool());

    if (hadPreviousRuntime)
        qputenv("XDG_RUNTIME_DIR", previousRuntime);
    else
        qunsetenv("XDG_RUNTIME_DIR");
}

QTEST_MAIN(SettingsQmlSmokeTest)
#include "SettingsQmlSmokeTest.moc"
