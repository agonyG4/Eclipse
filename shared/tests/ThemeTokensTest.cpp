#include "theme/ThemeController.hpp"
#include "theme/ThemePackCatalog.hpp"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTest>

namespace {
QString dataRoot;

bool writeJson(const QString &path, const QJsonObject &object)
{
    QFile file(path);
    if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !file.open(QIODevice::WriteOnly))
        return false;
    return file.write(QJsonDocument(object).toJson()) >= 0;
}

bool replaceJson(const QString &path, const QJsonObject &object)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QByteArray bytes = QJsonDocument(object).toJson();
    return file.write(bytes) == bytes.size() && file.commit();
}

QJsonObject packageManifest(const QString &id, const QJsonObject &style)
{
    return {{QStringLiteral("schema"), 1}, {QStringLiteral("id"), id},
            {QStringLiteral("name"), id}, {QStringLiteral("style"), style}};
}

QString packageManifestPath(const QString &id)
{
    return QDir(dataRoot).filePath(QStringLiteral("AstreaOS/themes/") + id
                                   + QStringLiteral("/theme.json"));
}

QString configPath(const QString &name)
{
    return QDir(dataRoot).filePath(QStringLiteral("config/") + name);
}

class ThemeTokensTest final : public QObject {
    Q_OBJECT
private slots:
    void catalogValidatesAndFlattensAllowedTokens();
    void catalogKeepsValidPartialValuesAndRejectsUnknownOrInvalidValues();
    void controllerReloadsManifestReplacementAndRecovery();
    void qmlBindingsResolveLiveTokensAndWorkWithoutController();
};

void ThemeTokensTest::catalogValidatesAndFlattensAllowedTokens()
{
    const QString id = QStringLiteral("token-test");
    const QJsonObject style{
        {QStringLiteral("fontFamily"), QStringLiteral("Legacy Font")},
        {QStringLiteral("animationFast"), 350},
        {QStringLiteral("colors"), QJsonObject{{QStringLiteral("accent"), QStringLiteral("#123456")},
                                                 {QStringLiteral("shellBackground"), QStringLiteral("#11223344")}}},
        {QStringLiteral("metrics"), QJsonObject{{QStringLiteral("radiusSmall"), 0},
                                                   {QStringLiteral("spacingLarge"), 20.5}}},
        {QStringLiteral("typography"), QJsonObject{{QStringLiteral("fontFamily"), QStringLiteral("Nested Font")},
                                                      {QStringLiteral("fontSizeCaption"), 11},
                                                      {QStringLiteral("tooltipFontSize"), 17}}},
        {QStringLiteral("durations"), QJsonObject{{QStringLiteral("animationFast"), 0},
                                                    {QStringLiteral("animationPulse"), 120}}}
    };
    QVERIFY(writeJson(packageManifestPath(id), packageManifest(id, style)));

    const QVariantMap tokens = Astrea::ThemePacks::styleTokens(id);
    QCOMPARE(tokens.value(QStringLiteral("accent")).toString(), QStringLiteral("#123456"));
    QCOMPARE(tokens.value(QStringLiteral("shellBackground")).toString(), QStringLiteral("#11223344"));
    QCOMPARE(tokens.value(QStringLiteral("radiusSmall")).toDouble(), 0.0);
    QCOMPARE(tokens.value(QStringLiteral("spacingLarge")).toDouble(), 20.5);
    QCOMPARE(tokens.value(QStringLiteral("fontFamily")).toString(), QStringLiteral("Nested Font"));
    QCOMPARE(tokens.value(QStringLiteral("fontSizeCaption")).toInt(), 11);
    QCOMPARE(tokens.value(QStringLiteral("tooltipFontSize")).toInt(), 17);
    QCOMPARE(tokens.value(QStringLiteral("animationFast")).toInt(), 0);
    QCOMPARE(tokens.value(QStringLiteral("animationPulse")).toInt(), 120);
}

void ThemeTokensTest::catalogKeepsValidPartialValuesAndRejectsUnknownOrInvalidValues()
{
    const QString id = QStringLiteral("partial-test");
    const QJsonObject style{
        {QStringLiteral("fontFamily"), QStringLiteral("Good Font")},
        {QStringLiteral("animationFast"), 700},
        {QStringLiteral("colors"), QJsonObject{{QStringLiteral("accent"), QStringLiteral("red")},
                                                 {QStringLiteral("invented"), QStringLiteral("#123456")}}},
        {QStringLiteral("metrics"), QJsonObject{{QStringLiteral("radiusLarge"), 257},
                                                   {QStringLiteral("invented"), 12}}},
        {QStringLiteral("typography"), QJsonObject{{QStringLiteral("fontFamily"), QStringLiteral("Bad/Path")},
                                                      {QStringLiteral("fontSizeHeader"), 97}}},
        {QStringLiteral("durations"), QJsonObject{{QStringLiteral("animationFast"), 1.5},
                                                    {QStringLiteral("animationQuick"), 1.5},
                                                    {QStringLiteral("invented"), 100}}}
    };
    QVERIFY(writeJson(packageManifestPath(id), packageManifest(id, style)));

    const QVariantMap tokens = Astrea::ThemePacks::styleTokens(id);
    QVERIFY(!tokens.contains(QStringLiteral("accent")));
    QVERIFY(!tokens.contains(QStringLiteral("invented")));
    QVERIFY(!tokens.contains(QStringLiteral("radiusLarge")));
    QVERIFY(!tokens.contains(QStringLiteral("fontSizeHeader")));
    QVERIFY(!tokens.contains(QStringLiteral("animationQuick")));
    QCOMPARE(tokens.value(QStringLiteral("fontFamily")).toString(), QStringLiteral("Good Font"));
    QCOMPARE(tokens.value(QStringLiteral("animationFast")).toInt(), 700);
}

void ThemeTokensTest::controllerReloadsManifestReplacementAndRecovery()
{
    const QString id = QStringLiteral("watch-test");
    const QString preferencePath = configPath(QStringLiteral("navigation-icons.json"));
    const QString manifestPath = packageManifestPath(id);
    QVERIFY(writeJson(preferencePath, {{QStringLiteral("visual_theme"), id}}));
    QVERIFY(writeJson(manifestPath, packageManifest(id,
        {{QStringLiteral("colors"), QJsonObject{{QStringLiteral("accent"), QStringLiteral("#102030")},
                                                 {QStringLiteral("shellBackground"), QStringLiteral("#506070")}}}})));

    ThemeController controller(configPath(QStringLiteral("theme.json")));
    QCOMPARE(controller.visualTheme(), id);
    QCOMPARE(controller.styleTokens().value(QStringLiteral("accent")).toString(), QStringLiteral("#102030"));

    for (const QString &color : {QStringLiteral("#203040"), QStringLiteral("#304050")}) {
        QVERIFY(replaceJson(manifestPath, packageManifest(id,
            {{QStringLiteral("colors"), QJsonObject{{QStringLiteral("accent"), color}}}})));
        QTRY_COMPARE_WITH_TIMEOUT(controller.styleTokens().value(QStringLiteral("accent")).toString(), color, 3000);
    }

    QVERIFY(replaceJson(manifestPath, QJsonObject{{QStringLiteral("broken"), true}}));
    QTRY_COMPARE_WITH_TIMEOUT(controller.visualTheme(), QStringLiteral("astrea"), 3000);
    QVERIFY(QFile::remove(manifestPath));
    QVERIFY(writeJson(manifestPath, packageManifest(id,
        {{QStringLiteral("colors"), QJsonObject{{QStringLiteral("accent"), QStringLiteral("#405060")}}}})));
    QTRY_COMPARE_WITH_TIMEOUT(controller.visualTheme(), id, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.styleTokens().value(QStringLiteral("accent")).toString(), QStringLiteral("#405060"), 3000);

    const QString lateId = QStringLiteral("late-watch-test");
    QVERIFY(writeJson(configPath(QStringLiteral("late/navigation-icons.json")),
                      {{QStringLiteral("visual_theme"), lateId}}));
    ThemeController lateController(configPath(QStringLiteral("late/theme.json")));
    QCOMPARE(lateController.visualTheme(), QStringLiteral("astrea"));
    QVERIFY(writeJson(packageManifestPath(lateId), packageManifest(lateId,
        {{QStringLiteral("colors"), QJsonObject{{QStringLiteral("accent"), QStringLiteral("#607080")}}}})));
    QTRY_COMPARE_WITH_TIMEOUT(lateController.visualTheme(), lateId, 3000);
}

void ThemeTokensTest::qmlBindingsResolveLiveTokensAndWorkWithoutController()
{
    const QString id = QStringLiteral("qml-test");
    QVERIFY(writeJson(configPath(QStringLiteral("navigation-icons.json")),
                      {{QStringLiteral("visual_theme"), id}}));
    const QString manifestPath = packageManifestPath(id);
    QVERIFY(writeJson(manifestPath, packageManifest(id,
        {{QStringLiteral("colors"), QJsonObject{{QStringLiteral("accent"), QStringLiteral("#abcdef")},
                                                 {QStringLiteral("shellBackground"), QStringLiteral("#123456")}}}})));
    ThemeController controller(configPath(QStringLiteral("theme.json")));
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(ASTREA_QML_IMPORT_PATH));
    engine.rootContext()->setContextProperty(QStringLiteral("ThemeController"), &controller);
    QQmlComponent component(&engine);
    component.loadUrl(QUrl::fromLocalFile(QStringLiteral(ASTREA_THEME_TOKENS_QML)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> tokens(component.create());
    QVERIFY2(tokens, qPrintable(component.errorString()));
    QVariant value;
    QVERIFY(QMetaObject::invokeMethod(tokens.get(), "value", Q_RETURN_ARG(QVariant, value),
                                      Q_ARG(QVariant, QStringLiteral("accent")),
                                      Q_ARG(QVariant, QStringLiteral("#fallback"))));
    QCOMPARE(value.toString(), QStringLiteral("#abcdef"));
    QVERIFY(QMetaObject::invokeMethod(tokens.get(), "value", Q_RETURN_ARG(QVariant, value),
                                      Q_ARG(QVariant, QStringLiteral("missing")),
                                      Q_ARG(QVariant, QStringLiteral("#partial-fallback"))));
    QCOMPARE(value.toString(), QStringLiteral("#partial-fallback"));

    QQmlComponent bindingComponent(&engine);
    bindingComponent.setData(QByteArrayLiteral(
        "import QtQuick\nimport Astrea.Shared 1.0\n"
        "QtObject { property ThemeTokens tokens: ThemeTokens {}; property ShellMaterialTheme material: ShellMaterialTheme {}; property var accent: tokens.value(\"accent\", \"#fallback\") }"),
        QUrl());
    QVERIFY2(bindingComponent.isReady(), qPrintable(bindingComponent.errorString()));
    std::unique_ptr<QObject> binding(bindingComponent.create());
    QVERIFY2(binding, qPrintable(bindingComponent.errorString()));
    QCOMPARE(binding->property("accent").toString(), QStringLiteral("#abcdef"));
    QObject *material = binding->property("material").value<QObject *>();
    QVERIFY(material);
    QCOMPARE(material->property("background").value<QColor>(), QColor(QStringLiteral("#123456")));
    QVERIFY(replaceJson(manifestPath, packageManifest(id,
        {{QStringLiteral("colors"), QJsonObject{{QStringLiteral("accent"), QStringLiteral("#fedcba")},
                                                 {QStringLiteral("shellBackground"), QStringLiteral("#654321")}}}})));
    QTRY_COMPARE_WITH_TIMEOUT(controller.styleTokens().value(QStringLiteral("accent")).toString(),
                              QStringLiteral("#fedcba"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(binding->property("accent").toString(), QStringLiteral("#fedcba"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(material->property("background").value<QColor>(),
                              QColor(QStringLiteral("#654321")), 3000);
    QVERIFY(QMetaObject::invokeMethod(tokens.get(), "value", Q_RETURN_ARG(QVariant, value),
                                      Q_ARG(QVariant, QStringLiteral("accent")),
                                      Q_ARG(QVariant, QStringLiteral("#fallback"))));
    QCOMPARE(value.toString(), QStringLiteral("#fedcba"));

    QQmlEngine fallbackEngine;
    fallbackEngine.addImportPath(QStringLiteral(ASTREA_QML_IMPORT_PATH));
    QQmlComponent fallbackComponent(&fallbackEngine);
    fallbackComponent.loadUrl(QUrl::fromLocalFile(QStringLiteral(ASTREA_THEME_TOKENS_QML)));
    QVERIFY2(fallbackComponent.isReady(), qPrintable(fallbackComponent.errorString()));
    std::unique_ptr<QObject> fallback(fallbackComponent.create());
    QVERIFY2(fallback, qPrintable(fallbackComponent.errorString()));
    QVERIFY(QMetaObject::invokeMethod(fallback.get(), "value", Q_RETURN_ARG(QVariant, value),
                                      Q_ARG(QVariant, QStringLiteral("accent")),
                                      Q_ARG(QVariant, QStringLiteral("#fallback"))));
    QCOMPARE(value.toString(), QStringLiteral("#fallback"));

    QQmlComponent shellFallbackComponent(&fallbackEngine);
    shellFallbackComponent.setData(QByteArrayLiteral(
        "import QtQuick\nimport Astrea.Shared 1.0\n"
        "QtObject { property ShellMaterialTheme theme: ShellMaterialTheme {} }"), QUrl());
    QVERIFY2(shellFallbackComponent.isReady(), qPrintable(shellFallbackComponent.errorString()));
    std::unique_ptr<QObject> shellFallback(shellFallbackComponent.create());
    QVERIFY2(shellFallback, qPrintable(shellFallbackComponent.errorString()));
    QObject *fallbackTheme = shellFallback->property("theme").value<QObject *>();
    QVERIFY(fallbackTheme);
    QVERIFY(fallbackTheme->property("isTransparent").toBool());
    QCOMPARE(fallbackTheme->property("radiusLarge").toDouble(), 14.0);
    QCOMPARE(fallbackTheme->property("radiusMedium").toDouble(), 8.0);
    QCOMPARE(fallbackTheme->property("fontFamily").toString(), QStringLiteral("Inter Variable"));
    QCOMPARE(fallbackTheme->property("fontSizeCaption").toInt(), 11);
    QCOMPARE(fallbackTheme->property("tooltipFontSize").toInt(), 13);
}

} // namespace

int main(int argc, char **argv)
{
    QTemporaryDir fixture(QStringLiteral("/mnt/Aether/Desktop/GitHub/eclipse-theme-test-XXXXXX"));
    if (!fixture.isValid())
        return 2;
    dataRoot = fixture.path();
    qputenv("XDG_DATA_HOME", QFile::encodeName(dataRoot));
    qputenv("XDG_DATA_DIRS", QFile::encodeName(dataRoot));
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(QDir(dataRoot).filePath(QStringLiteral("config-home"))));
    QGuiApplication app(argc, argv);
    ThemeTokensTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThemeTokensTest.moc"
