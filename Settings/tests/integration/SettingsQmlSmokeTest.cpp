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
#include <QQuickWindow>
#include <QRegularExpression>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QTemporaryDir>
#include <QtTest>

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
    QVERIFY(findVisualItem(hubItem, QStringLiteral("hubNavigationRow-wallpaper")) != nullptr);
    QVERIFY(findVisualItem(hubItem, QStringLiteral("hubNavigationRow-dock")) != nullptr);
    QCOMPARE(countVisualItems(hubItem, QRegularExpression(QStringLiteral("^hubNavigationRow-"))), 2);
    QCOMPARE(settingsController.currentDestinationChildren().size(), 2);
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
    QVERIFY(page->findChild<QObject *>(QStringLiteral("iconSizeSlider")) != nullptr);
    QVERIFY(page->findChild<QObject *>(QStringLiteral("magnificationScaleSlider")) != nullptr);
    QVERIFY(page->findChild<QObject *>(QStringLiteral("animationSpeedSlider")) != nullptr);
    QVERIFY(page->findChild<QObject *>(QStringLiteral("indicatorSizeSlider")) != nullptr);
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
    QStringList requests;
    QByteArray requestBuffer;
    bool removed = false;

    const auto makeSnapshot = [&] {
        const QJsonObject configured{{QStringLiteral("logicalId"), currentId},
                                     {QStringLiteral("source"), QStringLiteral(":/private/current.png")},
                                     {QStringLiteral("resolvedSource"), QStringLiteral(":/private/current.png")},
                                     {QStringLiteral("fit"), QStringLiteral("cover")}};
        const QJsonObject effective{{QStringLiteral("logicalId"), currentId},
                                    {QStringLiteral("source"), QStringLiteral(":/private/current.png")},
                                    {QStringLiteral("resolvedSource"), QStringLiteral(":/private/current.png")},
                                    {QStringLiteral("previewSource"), currentPath},
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

    connect(&server, &QLocalServer::newConnection, this, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [&, socket] {
            requestBuffer += socket->readAll();
            if (!requestBuffer.endsWith('\n'))
                return;
            const auto line = QString::fromUtf8(requestBuffer).trimmed();
            requestBuffer.clear();
            requests.append(line);

            auto wallpapers = QJsonArray{
                QJsonObject{{QStringLiteral("logicalId"), currentId},
                            {QStringLiteral("kind"), QStringLiteral("image")},
                            {QStringLiteral("origin"), QStringLiteral("user")},
                            {QStringLiteral("source"), QStringLiteral(":/private/current.png")},
                            {QStringLiteral("resolvedSource"), QStringLiteral(":/private/current.png")},
                            {QStringLiteral("previewSource"), currentPath},
                            {QStringLiteral("displayName"), QStringLiteral("Current")}},
                QJsonObject{{QStringLiteral("logicalId"), inactiveId},
                            {QStringLiteral("kind"), QStringLiteral("image")},
                            {QStringLiteral("origin"), QStringLiteral("user")},
                            {QStringLiteral("source"), QStringLiteral(":/private/inactive.png")},
                            {QStringLiteral("resolvedSource"), QStringLiteral(":/private/inactive.png")},
                            {QStringLiteral("previewSource"), inactivePath},
                            {QStringLiteral("displayName"), QStringLiteral("Snow")}}
            };
            if (line.startsWith(QStringLiteral("wallpaper remove"))) {
                removed = true;
                wallpapers = QJsonArray{wallpapers.at(0)};
            }
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
    const auto inactiveButtonName = QStringLiteral("wallpaperTileRemoveButton-") + inactiveId;
    const auto currentTileName = QStringLiteral("wallpaperTile-") + currentId;
    QTRY_VERIFY_WITH_TIMEOUT(findVisualItem(page, inactiveTileName) != nullptr, 1500);
    QTRY_VERIFY_WITH_TIMEOUT(findVisualItem(page, currentTileName) != nullptr, 1500);
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
    auto *currentRemoveButton = findVisualItem(
        page, QStringLiteral("wallpaperTileRemoveButton-") + currentId);
    QVERIFY(currentRemoveButton != nullptr);
    QCOMPARE(currentRemoveButton->property("visible").toBool(), false);

    const auto bodyPoint = inactiveTile->mapToItem(
        window->contentItem(), QPointF(inactiveTile->width() / 2, inactiveTile->height() / 2));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, bodyPoint.toPoint());
    QTRY_VERIFY_WITH_TIMEOUT(!settingsController.wallpaper()->busy(), 1500);
    QVERIFY(requests.last().startsWith(QStringLiteral("wallpaper set ")));
    QVERIFY(!requests.last().startsWith(QStringLiteral("wallpaper remove ")));
    QCOMPARE(settingsController.wallpaper()->effectiveId(), currentId);

    auto *inactiveRemoveButton = findVisualItem(page, inactiveButtonName);
    QVERIFY(inactiveRemoveButton != nullptr);
    auto *inactiveRemoveItem = qobject_cast<QQuickItem *>(inactiveRemoveButton);
    QVERIFY(inactiveRemoveItem != nullptr);
    const auto removePoint = inactiveRemoveItem->mapToItem(
        window->contentItem(), QPointF(inactiveRemoveItem->width() / 2, inactiveRemoveItem->height() / 2));
    QTest::mouseMove(window, removePoint.toPoint());
    QTRY_VERIFY_WITH_TIMEOUT(inactiveRemoveButton->property("visible").toBool(), 1000);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, removePoint.toPoint());
    auto *removeDialog = page->findChild<QObject *>(QStringLiteral("wallpaperRemoveDialog"));
    QVERIFY(removeDialog != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(removeDialog->property("visible").toBool(), 1000);
    QVERIFY(!removed);
    QVERIFY(requests.last().startsWith(QStringLiteral("wallpaper set ")));

    auto *confirmButton = page->findChild<QObject *>(QStringLiteral("wallpaperRemoveConfirmButton"));
    QVERIFY(confirmButton != nullptr);
    auto *confirmItem = qobject_cast<QQuickItem *>(confirmButton);
    QVERIFY(confirmItem != nullptr);
    const auto confirmPoint = confirmItem->mapToItem(
        window->contentItem(), QPointF(confirmItem->width() / 2, confirmItem->height() / 2));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, confirmPoint.toPoint());
    QTRY_VERIFY_WITH_TIMEOUT(removed, 1500);
    QCOMPARE(requests.count(QStringLiteral("wallpaper remove {\"id\":\"%1\"}").arg(inactiveId)), 1);
    QTRY_VERIFY_WITH_TIMEOUT(findVisualItem(page, inactiveTileName) == nullptr, 1500);
    QVERIFY(findVisualItem(page, currentTileName) != nullptr);
    QCOMPARE(settingsController.wallpaper()->effectiveId(), currentId);

    if (hadPreviousRuntime)
        qputenv("XDG_RUNTIME_DIR", previousRuntime);
    else
        qunsetenv("XDG_RUNTIME_DIR");
}

QTEST_MAIN(SettingsQmlSmokeTest)
#include "SettingsQmlSmokeTest.moc"
