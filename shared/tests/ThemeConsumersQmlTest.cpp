#include "theme/ThemeController.hpp"
#include "core/DockSurfaceGeometry.hpp"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQmlPropertyMap>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

Q_IMPORT_QML_PLUGIN(Astrea_SharedPlugin)
Q_IMPORT_QML_PLUGIN(Astrea_SettingsPlugin)
Q_IMPORT_QML_PLUGIN(Astrea_EffectsPlugin)

namespace {
QString dataRoot;

bool writeJson(const QString &path, const QJsonObject &object)
{
    QFile file(path);
    if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !file.open(QIODevice::WriteOnly))
        return false;
    return file.write(QJsonDocument(object).toJson()) >= 0;
}

QJsonObject manifest(const QString &id, const QJsonObject &style)
{
    return {{QStringLiteral("schema"), 1}, {QStringLiteral("id"), id},
            {QStringLiteral("name"), id}, {QStringLiteral("style"), style}};
}

QString themePath(const QString &id)
{
    return QDir(dataRoot).filePath(QStringLiteral("AstreaOS/themes/") + id
                                   + QStringLiteral("/theme.json"));
}

class ThemeConsumersQmlTest final : public QObject {
    Q_OBJECT
private slots:
    void actualConsumersResolveLivePackageTokens();
    void clockAndSpotlightConsumeCompleteSurfaceStyles();
};

void ThemeConsumersQmlTest::clockAndSpotlightConsumeCompleteSurfaceStyles()
{
    QTemporaryDir dir;
    const QString id = "unified-fonts";
    QVERIFY(writeJson(dir.filePath("navigation-icons.json"), {{"visual_theme",id}}));
    QVERIFY(writeJson(themePath(id), manifest(id, {{"typography",QJsonObject{
        {"fontFamily","Unified Theme Font"},
        {"surfaces",QJsonObject{
            {"topbar",QJsonObject{{"label",QJsonObject{{"pixelSize",16},{"weight",350},{"letterSpacing",0.75}}},
                                  {"caption",QJsonObject{{"pixelSize",12},{"weight",400},{"letterSpacing",0.5}}}}},
            {"spotlight",QJsonObject{{"title",QJsonObject{{"pixelSize",24},{"weight",300},{"letterSpacing",-0.2}}}}}}}}}})));
    ThemeController controller(dir.filePath("theme.json"));
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(ASTREA_QML_IMPORT_PATH));
    engine.rootContext()->setContextProperty("ThemeController", &controller);
    QQmlPropertyMap spotlightController;
    spotlightController.insert("open",false);
    spotlightController.insert("query",QString());
    spotlightController.insert("selectedIndex",0);
    spotlightController.insert("resultsModel",QVariantList{});
    spotlightController.insert("weatherEnabled",false);
    spotlightController.insert("weatherReady",false);
    spotlightController.insert("weatherIconSource",QString());
    engine.rootContext()->setContextProperty("SpotlightController",&spotlightController);
    QQmlPropertyMap i18n;
    i18n.insert("messages",false);
    engine.rootContext()->setContextProperty("AstreaI18n",&i18n);
    QQmlComponent clockComponent(&engine,QUrl::fromLocalFile(QStringLiteral(ASTREA_BAR_CLOCK_QML)));
    QVERIFY2(clockComponent.isReady(),qPrintable(clockComponent.errorString()));
    std::unique_ptr<QObject> clock(clockComponent.create());
    QVERIFY2(clock,qPrintable(clockComponent.errorString()));
    QQmlComponent searchComponent(&engine,QUrl::fromLocalFile(QStringLiteral(ASTREA_SPOTLIGHT_PANEL_QML)));
    QVERIFY2(searchComponent.isReady(),qPrintable(searchComponent.errorString()));
    std::unique_ptr<QObject> search(searchComponent.create());
    QVERIFY2(search,qPrintable(searchComponent.errorString()));
    auto *date=clock->findChild<QObject *>("clockDate");
    auto *time=clock->findChild<QObject *>("clockTime");
    auto *input=search->findChild<QObject *>("spotlightSearchInput");
    QVERIFY(date && time && input);
    const auto font=[](QObject *object) { return object->property("font").value<QFont>(); };
    QCOMPARE(font(date).family(),QString("Unified Theme Font"));
    QCOMPARE(font(time).family(),font(input).family());
    QCOMPARE(font(time).pixelSize(),16);
    QCOMPARE(int(font(time).weight()),350);
    QCOMPARE(font(time).letterSpacing(),0.75);
    QCOMPARE(font(date).pixelSize(),12);
    QCOMPARE(font(date).letterSpacing(),0.5);
    QCOMPARE(font(input).pixelSize(),24);
    QCOMPARE(int(font(input).weight()),300);
    QVERIFY(qAbs(font(input).letterSpacing() + 0.2) < 1.0/32); // Qt quantizes font metrics.
    const QString chosen=QFontInfo(QFont("serif")).family();
    controller.setFontFamilyPreference("fontFamily",chosen);
    controller.setTypographyPreference("label","weight",600);
    controller.setTypographyPreference("title","letterSpacing",1.0);
    controller.setTextScale(1.5);
    QTRY_COMPARE(font(date).family(),chosen);
    QTRY_COMPARE(font(time).family(),chosen);
    QTRY_COMPARE(font(input).family(),chosen);
    QTRY_COMPARE(font(time).pixelSize(),24);
    QTRY_COMPARE(int(font(time).weight()),600);
    QTRY_COMPARE(font(input).pixelSize(),36);
    QTRY_COMPARE(font(input).letterSpacing(),1.0);
    controller.resetFontPreferences();
    QTRY_COMPARE(font(time).pixelSize(),16);
    QTRY_COMPARE(font(input).pixelSize(),24);
    QTRY_VERIFY(qAbs(font(input).letterSpacing() + 0.2) < 1.0/32);
    QVERIFY(writeJson(themePath(id),manifest(id,{{"typography",QJsonObject{
        {"fontFamily","Unified Theme Font"}, {"surfaces",QJsonObject{
            {"topbar",QJsonObject{{"label",QJsonObject{{"pixelSize",14},{"weight",450}}}}},
            {"spotlight",QJsonObject{{"title",QJsonObject{{"pixelSize",26},{"weight",400}}}}}}}}}})));
    QTRY_COMPARE(font(time).pixelSize(),14);
    QTRY_COMPARE(int(font(time).weight()),450);
    QTRY_COMPARE(font(input).pixelSize(),26);
    const QString captures=qEnvironmentVariable("ASTREA_TYPOGRAPHY_CAPTURE_DIR");
    if (!captures.isEmpty()) {
        QVERIFY(QFileInfo(captures).isAbsolute());
        QVERIFY(QDir().mkpath(captures));
        QQuickWindow window;
        window.setColor(QColor("#202124"));
        window.resize(760,260);
        auto *clockItem=qobject_cast<QQuickItem *>(clock.get());
        auto *searchItem=qobject_cast<QQuickItem *>(search.get());
        QVERIFY(clockItem && searchItem);
        QQmlPropertyMap clockService;
        clockService.insert("dateText",QString("Mon, Nov 1"));
        clockService.insert("timeText",QString("20:30"));
        clock->setProperty("clockService",QVariant::fromValue(static_cast<QObject *>(&clockService)));
        clockItem->setParentItem(window.contentItem());
        clockItem->setPosition(QPointF(36,20));
        searchItem->setParentItem(window.contentItem());
        searchItem->setPosition(QPointF(36,100));
        spotlightController.insert("open",true);
        window.show();
        QTest::qWait(450);
        const auto capture=window.grabWindow();
        QVERIFY(!capture.isNull());
        QVERIFY(capture.save(QDir(captures).filePath("topbar-spotlight.png")));
        clockItem->setParentItem(nullptr);
        searchItem->setParentItem(nullptr);
    }


}

void ThemeConsumersQmlTest::actualConsumersResolveLivePackageTokens()
{
    const QString id = QStringLiteral("consumer-test");
    const QString config = QDir(dataRoot).filePath(QStringLiteral("config/theme.json"));
    const QString navigation = QDir(dataRoot).filePath(QStringLiteral("config/navigation-icons.json"));
    QVERIFY(writeJson(navigation, {{QStringLiteral("visual_theme"), QStringLiteral("astrea")}}));
    const QJsonObject style{
        {QStringLiteral("colors"), QJsonObject{
             {QStringLiteral("accent"), QStringLiteral("#123456")},
             {QStringLiteral("shellIconAccent"), QStringLiteral("#abcdef")},
             {QStringLiteral("shellBackground"), QStringLiteral("#223344")},
             {QStringLiteral("shellBorder"), QStringLiteral("#334455")},
             {QStringLiteral("shellTextMain"), QStringLiteral("#445566")},
             {QStringLiteral("shellTextSecondary"), QStringLiteral("#445566")},
             {QStringLiteral("tooltipSurface"), QStringLiteral("#556677")},
             {QStringLiteral("textPrimary"), QStringLiteral("#667788")}}},
        {QStringLiteral("metrics"), QJsonObject{
             {QStringLiteral("dockRadius"), 0}, {QStringLiteral("contextMenuRadius"), 0},
             {QStringLiteral("altTabRadius"), 0}, {QStringLiteral("spotlightRadius"), 0},
             {QStringLiteral("radiusSmall"), 0}}},
        {QStringLiteral("typography"), QJsonObject{
             {QStringLiteral("fontFamily"), QStringLiteral("Consumer Font")},
             {QStringLiteral("fontSizeTitle"), 21}, {QStringLiteral("tooltipFontSize"), 15}}},
        {QStringLiteral("durations"), QJsonObject{{QStringLiteral("animationFast"), 0}}}
    };
    QVERIFY(writeJson(themePath(id), manifest(id, style)));

    ThemeController controller(config);
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(ASTREA_QML_IMPORT_PATH));
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &controller);
    DockSurfaceGeometry dockGeometry;
    engine.rootContext()->setContextProperty(QStringLiteral("DockSurfaceGeometry"), &dockGeometry);
    QQmlPropertyMap dockController;
    const QVariantMap dockState{
        {QStringLiteral("vertical"), false}, {QStringLiteral("position"), QStringLiteral("bottom")},
        {QStringLiteral("cornerRadius"), 17}, {QStringLiteral("panelPadding"), 8},
        {QStringLiteral("restingCrossThickness"), 70}, {QStringLiteral("chromeEdgeInset"), 10},
        {QStringLiteral("delegateWidth"), 64}, {QStringLiteral("delegateHeight"), 64},
        {QStringLiteral("itemSpacing"), 4}, {QStringLiteral("hoverEffect"), QStringLiteral("none")},
        {QStringLiteral("iconRestingTop"), 0}, {QStringLiteral("iconSize"), 48},
        {QStringLiteral("magnificationScale"), 1.0}, {QStringLiteral("magnificationRadius"), 1},
        {QStringLiteral("animationSpeed"), 1.0}, {QStringLiteral("animationsEnabled"), true},
        {QStringLiteral("visible"), true}, {QStringLiteral("revealed"), true},
        {QStringLiteral("appModel"), QVariantList{}}};
    for (auto it = dockState.cbegin(); it != dockState.cend(); ++it)
        dockController.insert(it.key(), it.value());
    engine.rootContext()->setContextProperty(QStringLiteral("DockController"), &dockController);
    QQmlPropertyMap altTabController;
    altTabController.insert(QStringLiteral("open"), false);
    engine.rootContext()->setContextProperty(QStringLiteral("AltTabController"), &altTabController);
    QVariantList emptyModel;
    engine.rootContext()->setContextProperty(QStringLiteral("AltTabWindowModel"), emptyModel);
    QQmlPropertyMap spotlightController;
    spotlightController.insert(QStringLiteral("open"), false);
    spotlightController.insert(QStringLiteral("query"), QString());
    spotlightController.insert(QStringLiteral("fontFamily"), QStringLiteral("Fallback Font"));
    spotlightController.insert(QStringLiteral("selectedIndex"), 0);
    spotlightController.insert(QStringLiteral("resultsModel"), emptyModel);
    spotlightController.insert(QStringLiteral("weatherEnabled"), false);
    spotlightController.insert(QStringLiteral("weatherReady"), false);
    engine.rootContext()->setContextProperty(QStringLiteral("SpotlightController"), &spotlightController);
    QQmlPropertyMap i18n;
    i18n.insert(QStringLiteral("messages"), false);
    engine.rootContext()->setContextProperty(QStringLiteral("AstreaI18n"), &i18n);

    const auto create = [&engine](const char *path) -> std::unique_ptr<QObject> {
        QQmlComponent component(&engine);
        component.loadUrl(QUrl::fromLocalFile(QString::fromUtf8(path)));
        if (!component.isReady())
            qWarning().noquote() << component.errorString();
        QObject *object = component.create();
        if (!object)
            qWarning().noquote() << component.errorString();
        return std::unique_ptr<QObject>(object);
    };

    auto bar = create(ASTREA_BAR_THEME_QML);
    QVERIFY(bar);
    const QColor defaultBarBackground = bar->property("shellBackground").value<QColor>();

    auto menu = create(ASTREA_MENU_THEME_QML);
    QVERIFY(menu);
    const double defaultMenuRadius = menu->property("contextMenuRadius").toDouble();

    auto spotlight = create(ASTREA_SPOTLIGHT_WEATHER_QML);
    QVERIFY(spotlight);
    const QColor defaultSpotlightColor = spotlight->property("textColor").value<QColor>();
    auto spotlightPanel = create(ASTREA_SPOTLIGHT_PANEL_QML);
    QVERIFY(spotlightPanel);
    const QColor defaultSpotlightBackground = spotlightPanel->property("color").value<QColor>();
    const double defaultSpotlightRadius = spotlightPanel->property("radius").toDouble();

    auto altTab = create(ASTREA_ALTTAB_DELEGATE_QML);
    QVERIFY(altTab);
    const QColor defaultAltTabAccent = altTab->property("accent").value<QColor>();
    QObject *highlight = altTab->findChild<QObject *>(QStringLiteral("altTabSelectionHighlight"));
    QVERIFY(highlight);
    auto altTabPanel = create(ASTREA_ALTTAB_PANEL_QML);
    QVERIFY(altTabPanel);
    QObject *altTabPanelBackground = altTabPanel->findChild<QObject *>(QStringLiteral("altTabPanelBackground"));
    QVERIFY(altTabPanelBackground);
    const QColor defaultAltTabBackground = altTabPanelBackground->property("color").value<QColor>();
    const double defaultAltTabRadius = altTabPanelBackground->property("radius").toDouble();

    QObject *settingsTheme = engine.singletonInstance<QObject *>("Astrea.Settings", "Theme");
    QVERIFY(settingsTheme);
    QCOMPARE(settingsTheme->property("shellIconAccent").value<QColor>(), QColor("#60aaff"));
    const QColor defaultSettingsAccent = settingsTheme->property("accent").value<QColor>();
    const QColor defaultSettingsText = settingsTheme->property("textPrimary").value<QColor>();
    const double defaultSettingsRadius = settingsTheme->property("radiusSmall").toDouble();

    auto dockPanel = create(ASTREA_DOCK_PANEL_QML);
    QVERIFY(dockPanel);
    QObject *dockChrome = dockPanel->findChild<QObject *>(QStringLiteral("dockChrome"));
    QVERIFY(dockChrome);
    const double defaultDockRadius = dockChrome->property("radius").toDouble();
    QVariant duration;
    QVERIFY(QMetaObject::invokeMethod(dockPanel.get(), "animationDuration",
                                      Q_RETURN_ARG(QVariant, duration),
                                      Q_ARG(QVariant, 90)));
    const int defaultDuration = duration.toInt();

    QVERIFY(writeJson(navigation, {{QStringLiteral("visual_theme"), id}}));
    controller.reload();
    QTRY_COMPARE_WITH_TIMEOUT(controller.visualTheme(), id, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(bar->property("shellBackground").value<QColor>(), QColor("#223344"), 3000);
    QCOMPARE(bar->property("shellTextMain").value<QColor>(), QColor("#445566"));
    QCOMPARE(bar->property("fontSizeTitle").toInt(), 21);
    QCOMPARE(menu->property("background").value<QColor>(), QColor("#223344"));
    QCOMPARE(menu->property("contextMenuRadius").toDouble(), 0.0);
    QCOMPARE(spotlight->property("textColor").value<QColor>(), QColor("#445566"));
    QCOMPARE(spotlightPanel->property("color").value<QColor>(), QColor("#223344"));
    QCOMPARE(spotlightPanel->property("radius").toDouble(), 0.0);
    QCOMPARE(altTab->property("accent").value<QColor>(), QColor("#123456"));
    QCOMPARE(altTabPanelBackground->property("color").value<QColor>(), QColor("#223344"));
    QCOMPARE(altTabPanelBackground->property("radius").toDouble(), 0.0);
    altTab->setProperty("windowSelected", true);
    QTRY_COMPARE_WITH_TIMEOUT(highlight->property("color").value<QColor>().alpha(), 64, 3000);
    QCOMPARE(settingsTheme->property("accent").value<QColor>(), QColor("#123456"));
    QCOMPARE(settingsTheme->property("shellIconAccent").value<QColor>(), QColor("#abcdef"));
    QCOMPARE(settingsTheme->property("accentForeground").value<QColor>(), QColor("#ffffff"));
    QCOMPARE(settingsTheme->property("textPrimary").value<QColor>(), QColor("#667788"));
    QCOMPARE(settingsTheme->property("radiusSmall").toDouble(), 0.0);
    QCOMPARE(settingsTheme->property("fontFamily").toString(), QStringLiteral("Consumer Font"));
    QCOMPARE(dockChrome->property("radius").toDouble(), 0.0);
    QVERIFY(QMetaObject::invokeMethod(dockPanel.get(), "animationDuration",
                                      Q_RETURN_ARG(QVariant, duration), Q_ARG(QVariant, 90)));
    QCOMPARE(duration.toInt(), 0);

    const QJsonObject changedStyle{{QStringLiteral("colors"), QJsonObject{
        {QStringLiteral("accent"), QStringLiteral("#ffff00")}}}};
    QVERIFY(writeJson(themePath(id), manifest(id, changedStyle)));
    QTRY_COMPARE_WITH_TIMEOUT(settingsTheme->property("accent").value<QColor>(), QColor("#ffff00"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(settingsTheme->property("accentForeground").value<QColor>(), QColor("#111111"), 3000);

    const QJsonObject partialStyle{{QStringLiteral("colors"), QJsonObject{
        {QStringLiteral("shellBackground"), QStringLiteral("#abcdef")}}}};
    QVERIFY(writeJson(themePath(id), manifest(id, partialStyle)));
    QTRY_COMPARE_WITH_TIMEOUT(bar->property("shellBackground").value<QColor>(), QColor("#abcdef"), 3000);
    QCOMPARE(bar->property("fontSizeTitle").toInt(), 18);
    QCOMPARE(menu->property("contextMenuRadius").toDouble(), 10.0);
    QCOMPARE(dockPanel->property("dockRadius").toDouble(), defaultDockRadius);
    QCOMPARE(settingsTheme->property("textPrimary").value<QColor>(), defaultSettingsText);
    QCOMPARE(settingsTheme->property("radiusSmall").toDouble(), defaultSettingsRadius);
    QCOMPARE(settingsTheme->property("fontFamily").toString(),
             QFontInfo(QFont(QStringLiteral("sans-serif"))).family());
    QCOMPARE(settingsTheme->property("shellIconAccent").value<QColor>(), QColor("#60aaff"));

    QVERIFY(writeJson(navigation, {{QStringLiteral("visual_theme"), QStringLiteral("astrea")}}));
    controller.reload();
    QTRY_COMPARE_WITH_TIMEOUT(controller.visualTheme(), QStringLiteral("astrea"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(bar->property("shellBackground").value<QColor>(), defaultBarBackground, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(menu->property("contextMenuRadius").toDouble(), defaultMenuRadius, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(spotlight->property("textColor").value<QColor>(), defaultSpotlightColor, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(spotlightPanel->property("color").value<QColor>(), defaultSpotlightBackground, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(spotlightPanel->property("radius").toDouble(), defaultSpotlightRadius, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(altTab->property("accent").value<QColor>(), defaultAltTabAccent, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(altTabPanelBackground->property("color").value<QColor>(), defaultAltTabBackground, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(altTabPanelBackground->property("radius").toDouble(), defaultAltTabRadius, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(settingsTheme->property("accent").value<QColor>(), defaultSettingsAccent, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(dockPanel->property("dockRadius").toDouble(), defaultDockRadius, 3000);
    QVERIFY(QMetaObject::invokeMethod(dockPanel.get(), "animationDuration",
                                      Q_RETURN_ARG(QVariant, duration), Q_ARG(QVariant, 90)));
    QCOMPARE(duration.toInt(), defaultDuration);

    const QString primary = QFontInfo(QFont("serif")).family();
    controller.setFontFamilyPreference("fontFamily", primary);
    controller.setTextScale(2.0);
    QTRY_COMPARE(settingsTheme->property("fontFamilyText").toString(), primary);
    QTRY_COMPARE(bar->property("fontFamily").toString(), primary);
    QTRY_COMPARE(menu->property("fontFamily").toString(), primary);
    auto *searchInput = spotlightPanel->findChild<QObject *>("spotlightSearchInput");
    QVERIFY(searchInput);
    QTRY_COMPARE(searchInput->property("font").value<QFont>().pixelSize(), 36);
    QTRY_COMPARE(bar->property("fontSizeTitle").toInt(), 36);
    QTRY_COMPARE(menu->property("fontSizeBody").toInt(), 26);
    QCOMPARE(dockPanel->property("dockRadius").toDouble(), defaultDockRadius);
    controller.setTypographyPreference("title", "weight", 300);
    controller.setTypographyPreference("title", "letterSpacing", 1.5);
    QTRY_COMPARE(searchInput->property("font").value<QFont>().weight(), QFont::Light);
    QTRY_COMPARE(searchInput->property("font").value<QFont>().letterSpacing(), 1.5);
    controller.setTypographyPreference("body", "pixelSize", 16);
    QTRY_COMPARE(settingsTheme->property("fontSizeNormal").toInt(), 32);
    controller.resetFontPreferences();
    QTRY_COMPARE(settingsTheme->property("fontSizeNormal").toInt(), 12);

    QQmlComponent semanticWrappers(&engine);
    semanticWrappers.setData("import QtQuick\nimport Astrea.Settings 1.0 as Settings\n"
        "Item { Settings.DisplayLabel { objectName: 'displayWrapper'; text: 'Display' } "
        "Settings.TextLabel { objectName: 'captionWrapper'; typographyRole: 'caption'; text: 'Caption' } }", QUrl());
    QVERIFY2(semanticWrappers.isReady(), qPrintable(semanticWrappers.errorString()));
    std::unique_ptr<QObject> wrappers(semanticWrappers.create());
    QVERIFY2(wrappers, qPrintable(semanticWrappers.errorString()));
    auto *displayWrapper = wrappers->findChild<QObject *>("displayWrapper");
    auto *captionWrapper = wrappers->findChild<QObject *>("captionWrapper");
    QVERIFY(displayWrapper && captionWrapper);
    controller.setTypographyPreference("display", "family", primary);
    controller.setTypographyPreference("caption", "family", primary);
    controller.setTypographyPreference("caption", "pixelSize", 16);
    controller.setTypographyPreference("caption", "lineHeight", 1.8);
    QTRY_COMPARE(displayWrapper->property("font").value<QFont>().family(), primary);
    QTRY_COMPARE(captionWrapper->property("font").value<QFont>().family(), primary);
    QTRY_COMPARE(captionWrapper->property("font").value<QFont>().pixelSize(), 16);
    QTRY_COMPARE(captionWrapper->property("lineHeight").toDouble(), 1.8);


}

} // namespace

int main(int argc, char **argv)
{
    QTemporaryDir fixture(QStringLiteral("/mnt/Aether/Desktop/GitHub/eclipse-theme-consumers-XXXXXX"));
    if (!fixture.isValid())
        return 2;
    dataRoot = fixture.path();
    qputenv("XDG_DATA_HOME", QFile::encodeName(dataRoot));
    qputenv("XDG_DATA_DIRS", QFile::encodeName(dataRoot));
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(QDir(dataRoot).filePath(QStringLiteral("config-home"))));
    QGuiApplication app(argc, argv);
    ThemeConsumersQmlTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThemeConsumersQmlTest.moc"
