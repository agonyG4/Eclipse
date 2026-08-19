#include "theme/ThemeController.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

class ThemeControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void usesLegacyDefaultsWhenConfigIsMissing();
    void loadsLegacyConfigValues();
    void reloadsExternalConfigReplacement();
    void keepsLastGoodStateWhenExternalConfigIsInvalid();
};

void ThemeControllerTest::usesLegacyDefaultsWhenConfigIsMissing()
{
    ThemeController controller(QStringLiteral("/tmp/astrea-settings-missing-theme.json"));

    QCOMPARE(controller.themeMode(), 0);
    QCOMPARE(controller.shellStyle(), 0);
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

QTEST_GUILESS_MAIN(ThemeControllerTest)
#include "ThemeControllerTest.moc"
