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
#include <QSignalSpy>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

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

QQuickItem *findPreviewSurface(QQuickItem *item)
{
    if (!item)
        return nullptr;
    if (item->property("iconExtent").isValid()
        && item->property("panelExtent").isValid()) {
        return item;
    }
    for (QQuickItem *child : item->childItems()) {
        if (QQuickItem *match = findPreviewSurface(child))
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
    void dockPreviewUsesFiveIconFootprint();
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

void SettingsQmlSmokeTest::dockPreviewUsesFiveIconFootprint()
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
    QVERIFY(settingsController.navigateTo(QStringLiteral("dock")));
    QObject *root = engine.rootObjects().constFirst();
    QTRY_VERIFY_WITH_TIMEOUT(root->findChild<QObject *>(QStringLiteral("dockPage")) != nullptr, 1000);
    auto *page = qobject_cast<QQuickItem *>(
        root->findChild<QObject *>(QStringLiteral("dockPage")));
    QVERIFY(page != nullptr);
    auto *previewCard = qobject_cast<QQuickItem *>(
        page->findChild<QObject *>(QStringLiteral("dockPreview")));
    QVERIFY(previewCard != nullptr);
    QQuickItem *previewSurface = findPreviewSurface(previewCard);
    QVERIFY(previewSurface != nullptr);
    QVERIFY(previewSurface->parentItem() != nullptr);

    const qreal iconExtent = previewSurface->property("iconExtent").toReal();
    const qreal panelPadding = settingsController.dock()->property("panelPadding").toReal();
    const qreal itemSpacing = settingsController.dock()->property("itemSpacing").toReal();
    const qreal expectedPrimary = panelPadding * 2 + iconExtent * 5 + itemSpacing * 4;
    const bool vertical = previewSurface->property("vertical").toBool();
    const qreal expected = vertical
        ? qMin(previewSurface->parentItem()->height(), expectedPrimary)
        : qMin(previewSurface->parentItem()->width() - 40, expectedPrimary);
    const qreal actual = vertical ? previewSurface->height() : previewSurface->width();
    QVERIFY2(qAbs(actual - expected) < 0.5,
             qPrintable(QStringLiteral("expected %1, got %2").arg(expected).arg(actual)));
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
