#include "theme/ThemeController.hpp"
#include "core/DockSurfaceGeometry.hpp"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
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
};

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
    QCOMPARE(bar->property("fontSizeTitle").toInt(), 13);
    QCOMPARE(menu->property("contextMenuRadius").toDouble(), 10.0);
    QCOMPARE(dockPanel->property("dockRadius").toDouble(), defaultDockRadius);
    QCOMPARE(settingsTheme->property("textPrimary").value<QColor>(), defaultSettingsText);
    QCOMPARE(settingsTheme->property("radiusSmall").toDouble(), defaultSettingsRadius);
    QCOMPARE(settingsTheme->property("fontFamily").toString(), QStringLiteral("Inter"));
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
