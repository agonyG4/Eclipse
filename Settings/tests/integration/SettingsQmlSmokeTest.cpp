#include "core/SettingsController.hpp"
#include "services/i18n/SettingsTranslationController.hpp"
#include "theme/ThemeController.hpp"

#include <QGuiApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QTemporaryDir>
#include <QtTest>

class SettingsQmlSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsCompositorRouteOffscreen();
    void loadsWallpaperRouteOffscreen();
    void loadsDockRouteOffscreen();
    void wallpaperTranslationKeysExist();
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
    QVERIFY(settingsController.selectSection(QStringLiteral("compositor")));
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("compositorPage")) != nullptr, 1000);
    QObject *page = root->findChild<QObject *>(QStringLiteral("compositorPage"));
    QVERIFY(page != nullptr);
    QCOMPARE(page->property("animationsEnabled").toBool(), true);
    page->setProperty("animationsEnabled", false);
    QCOMPARE(page->property("animationsEnabled").toBool(), false);

    QVERIFY(settingsController.selectSection(QStringLiteral("system")));
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("compositorPage")) == nullptr, 1000);
    QVERIFY(settingsController.selectSection(QStringLiteral("compositor")));
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("compositorPage")) != nullptr, 1000);
    QCOMPARE(root->findChild<QObject *>(QStringLiteral("compositorPage"))->property("animationsEnabled").toBool(), true);
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(qmlWarnings.isEmpty() ? QString() : qmlWarnings.constFirst().toString()));
}

void SettingsQmlSmokeTest::loadsWallpaperRouteOffscreen()
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
    QVERIFY(settingsController.selectSection(QStringLiteral("wallpaper")));
    QObject *root = engine.rootObjects().constFirst();
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
                            "userWallpapersAddButton"}) {
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
}

void SettingsQmlSmokeTest::loadsDockRouteOffscreen()
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
    QVERIFY(settingsController.selectSection(QStringLiteral("dock")));
    QObject *root = engine.rootObjects().constFirst();
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("dockPage")) != nullptr, 1000);
    QObject *page = root->findChild<QObject *>(QStringLiteral("dockPage"));
    QVERIFY(page != nullptr);
    QVERIFY(page->findChild<QObject *>(QStringLiteral("dockPreview")) != nullptr);
    QVERIFY(page->findChild<QObject *>(QStringLiteral("iconSizeSlider")) != nullptr);
    QVERIFY(page->findChild<QObject *>(QStringLiteral("magnificationScaleSlider")) != nullptr);
    QVERIFY(page->findChild<QObject *>(QStringLiteral("animationSpeedSlider")) != nullptr);
    QVERIFY(page->findChild<QObject *>(QStringLiteral("indicatorSizeSlider")) != nullptr);
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
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.current"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.choose_wallpaper"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.name_this_wallpaper"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.no_wallpapers_found"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.my_wallpaper"),
        QStringLiteral("apps.settings.pages.paper.wallpaper.text.preview_fail"),
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
    };
    for (const auto &key : requiredKeys)
        QVERIFY2(messages.contains(key), qPrintable(QStringLiteral("Missing key: ") + key));
}

QTEST_MAIN(SettingsQmlSmokeTest)
#include "SettingsQmlSmokeTest.moc"
