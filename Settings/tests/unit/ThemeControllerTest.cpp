#include "theme/ThemeController.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

namespace {

QString writeConfig(QTemporaryDir &directory, const QByteArray &contents)
{
    const QString path = directory.filePath(QStringLiteral("theme.json"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    file.write(contents);
    return path;
}

QJsonObject readConfig(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

} // namespace

class ThemeControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void usesLegacyDefaultsWhenConfigIsMissing();
    void loadsLegacyConfigValues();
    void reloadsExternalConfigReplacement();
    void keepsLastGoodStateWhenExternalConfigIsInvalid();
    void legacyLightConfigurationMigratesInMemory();
    void legacyDarkConfigurationMigratesInMemory();
    void validShellStylesRemainCompatible();
    void invalidShellStyleUsesDefault();
    void saveWritesCanonicalAndLegacyThemeKeys();
    void legacySetThemeModeSelectsExplicitPreference();
    void automaticPlatformChangeUpdatesEffectiveModeOnly();
};

void ThemeControllerTest::usesLegacyDefaultsWhenConfigIsMissing()
{
    ThemeController controller(QStringLiteral("/tmp/astrea-settings-missing-theme.json"));

    QCOMPARE(controller.themeMode(), 0);
    QCOMPARE(controller.themePreference(), QStringLiteral("auto"));
    QCOMPARE(controller.shellStyle(), 1);
    QCOMPARE(controller.iconStyle(), 0);
    QCOMPARE(controller.iconTheme(), QStringLiteral("dark"));
    QCOMPARE(controller.accentHex(), QStringLiteral("#0a84ff"));
    QCOMPARE(controller.audioOsdStyle(), 0);
}

void ThemeControllerTest::loadsLegacyConfigValues()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("theme.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream << R"({
        "theme_mode": 1,
        "shell_style": 2,
        "icon_style": 1,
        "icon_theme": "dark",
        "accent": "#30d158",
        "audio_osd_style": 1
    })";
    file.close();

    ThemeController controller(path);

    QCOMPARE(controller.themeMode(), 1);
    QCOMPARE(controller.shellStyle(), 2);
    QCOMPARE(controller.iconStyle(), 1);
    QCOMPARE(controller.iconTheme(), QStringLiteral("dark"));
    QCOMPARE(controller.accentHex(), QStringLiteral("#30d158"));
    QCOMPARE(controller.audioOsdStyle(), 1);
}

void ThemeControllerTest::reloadsExternalConfigReplacement()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("theme.json"));
    QFile initial(path);
    QVERIFY(initial.open(QIODevice::WriteOnly));
    initial.write(R"({"theme":"dark","shell_style":0})");
    initial.close();

    ThemeController controller(path);
    QCOMPARE(controller.themeMode(), 0);
    QSignalSpy modeSpy(&controller, &ThemeController::themeModeChanged);

    QFile replacement(path);
    QVERIFY(replacement.open(QIODevice::WriteOnly | QIODevice::Truncate));
    replacement.write(R"({"theme":"light","shell_style":2,"accent":"#30d158"})");
    replacement.close();

    QTRY_COMPARE_WITH_TIMEOUT(controller.themeMode(), 1, 1500);
    QCOMPARE(controller.shellStyle(), 2);
    QCOMPARE(controller.accentHex(), QStringLiteral("#30d158"));
    QCOMPARE(modeSpy.count(), 1);
}

void ThemeControllerTest::keepsLastGoodStateWhenExternalConfigIsInvalid()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("theme.json"));
    QFile initial(path);
    QVERIFY(initial.open(QIODevice::WriteOnly));
    initial.write(R"({"theme":"light","shell_style":2})");
    initial.close();

    ThemeController controller(path);
    QCOMPARE(controller.themeMode(), 1);
    QCOMPARE(controller.shellStyle(), 2);

    QFile invalid(path);
    QVERIFY(invalid.open(QIODevice::WriteOnly | QIODevice::Truncate));
    invalid.write(QByteArrayLiteral("{invalid"));
    invalid.close();

    QTest::qWait(450);
    QCOMPARE(controller.themeMode(), 1);
    QCOMPARE(controller.shellStyle(), 2);
}

void ThemeControllerTest::legacyLightConfigurationMigratesInMemory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(directory, R"({"theme":"light","shell_style":1})");
    QVERIFY(!path.isEmpty());

    ThemeController controller(path);

    QCOMPARE(controller.themePreference(), QStringLiteral("light"));
    QCOMPARE(controller.themeMode(), 1);
}

void ThemeControllerTest::legacyDarkConfigurationMigratesInMemory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(directory, R"({"theme":"dark","shell_style":1})");
    QVERIFY(!path.isEmpty());

    ThemeController controller(path);

    QCOMPARE(controller.themePreference(), QStringLiteral("dark"));
    QCOMPARE(controller.themeMode(), 0);
}

void ThemeControllerTest::validShellStylesRemainCompatible()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(directory, R"({"theme":"dark","shell_style":0})");
    QVERIFY(!path.isEmpty());

    ThemeController transparentController(path);
    QCOMPARE(transparentController.shellStyle(), 0);

    const QString frostedPath = writeConfig(directory, R"({"theme":"dark","shell_style":2})");
    QVERIFY(!frostedPath.isEmpty());
    ThemeController frostedController(frostedPath);
    QCOMPARE(frostedController.shellStyle(), 2);
}

void ThemeControllerTest::invalidShellStyleUsesDefault()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = writeConfig(directory, R"({"theme":"dark","shell_style":99})");
    QVERIFY(!path.isEmpty());

    ThemeController controller(path);

    QCOMPARE(controller.shellStyle(), 1);
}

void ThemeControllerTest::saveWritesCanonicalAndLegacyThemeKeys()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("theme.json"));
    ThemeController controller(path, nullptr, [] { return Qt::ColorScheme::Light; });

    controller.applyConfig({
        {QStringLiteral("theme_preference"), QStringLiteral("auto")},
        {QStringLiteral("shell_style"), 2},
        {QStringLiteral("accent"), QStringLiteral("#30d158")},
        {QStringLiteral("icon_style"), 1},
        {QStringLiteral("icon_theme"), QStringLiteral("dark")},
        {QStringLiteral("audio_osd_style"), 1},
    });
    controller.save();

    const QJsonObject object = readConfig(path);
    QCOMPARE(object.value(QStringLiteral("theme_preference")).toString(), QStringLiteral("auto"));
    QCOMPARE(object.value(QStringLiteral("theme")).toString(), QStringLiteral("light"));
    QCOMPARE(object.value(QStringLiteral("theme_mode")).toInt(), 1);
    QCOMPARE(object.value(QStringLiteral("shell_style")).toInt(), 2);
    QCOMPARE(object.value(QStringLiteral("accent")).toString(), QStringLiteral("#30d158"));
    QCOMPARE(object.value(QStringLiteral("icon_style")).toInt(), 1);
    QCOMPARE(object.value(QStringLiteral("icon_theme")).toString(), QStringLiteral("dark"));
    QCOMPARE(object.value(QStringLiteral("audio_osd_style")).toInt(), 1);
}

void ThemeControllerTest::legacySetThemeModeSelectsExplicitPreference()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ThemeController controller(directory.filePath(QStringLiteral("missing.json")));

    controller.setThemeMode(1);
    QCOMPARE(controller.themePreference(), QStringLiteral("light"));
    QCOMPARE(controller.themeMode(), 1);

    controller.setThemeMode(0);
    QCOMPARE(controller.themePreference(), QStringLiteral("dark"));
    QCOMPARE(controller.themeMode(), 0);
}

void ThemeControllerTest::automaticPlatformChangeUpdatesEffectiveModeOnly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    Qt::ColorScheme scheme = Qt::ColorScheme::Dark;
    ThemeController controller(directory.filePath(QStringLiteral("missing.json")), nullptr,
                               [&scheme] { return scheme; });

    QCOMPARE(controller.themePreference(), QStringLiteral("auto"));
    QCOMPARE(controller.themeMode(), 0);

    scheme = Qt::ColorScheme::Light;
    QVERIFY(QMetaObject::invokeMethod(&controller, "handlePlatformColorSchemeChanged",
                                      Qt::DirectConnection));

    QCOMPARE(controller.themePreference(), QStringLiteral("auto"));
    QCOMPARE(controller.themeMode(), 1);
}

QTEST_GUILESS_MAIN(ThemeControllerTest)
#include "ThemeControllerTest.moc"
