#include "services/theme/ThemeController.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

class ThemeControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void usesLegacyDefaultsWhenConfigIsMissing();
    void loadsLegacyConfigValues();
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

QTEST_GUILESS_MAIN(ThemeControllerTest)
#include "ThemeControllerTest.moc"
