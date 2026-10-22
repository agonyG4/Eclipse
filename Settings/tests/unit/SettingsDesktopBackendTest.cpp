#include "services/BarClockService.hpp"
#include "services/desktop/ApplicationsBackend.hpp"
#include "services/desktop/ProfileBackend.hpp"
#include "services/desktop/RegionSettings.hpp"
#include "services/desktop/SddmAutologin.hpp"
#include "settings/RegionalPreferences.hpp"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>

class SettingsDesktopBackendTest : public QObject
{
    Q_OBJECT
  private slots:
    void regionalReadDoesNotWrite()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("settings.json");
        const auto state = RegionSettings::read(path);
        QCOMPARE(state["language"].toString(), QString("en_US"));
        QVERIFY(!QFile::exists(path));
    }
    void regionalPatchPreservesUnknownFieldsAndRejectsInvalidValues()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("settings.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"future":42,"region":{"custom":"kept"}})");
        file.close();
        RegionSettings::update(path, "country_code", "JP");
        const auto data = RegionSettings::read(path);
        QCOMPARE(data["future"].toInt(), 42);
        QCOMPARE(data["region"].toMap()["custom"].toString(), QString("kept"));
        QVERIFY_EXCEPTION_THROWN(RegionSettings::update(path, "time_format", "invalid"),
                                 std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(RegionSettings::update(path, "automatic_location", "false"),
                                 std::runtime_error);
    }
    void corruptRegionalFileIsNotOverwritten()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("settings.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{bad");
        file.close();
        QVERIFY_EXCEPTION_THROWN(RegionSettings::update(path, "language", "en_US"),
                                 std::runtime_error);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("{bad"));
    }
    void desktopEntriesRespectPrecedenceAndHiddenMasks()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.filePath("user/applications"));
        QDir().mkpath(dir.filePath("system/applications"));
        auto write = [&](QString path, QByteArray body) {
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly))
                return false;
            return file.write(body) > 0;
        };
        QVERIFY(write(dir.filePath("system/applications/demo.desktop"),
                      "[Desktop Entry]\nType=Application\nName=System demo\nExec=/bin/true\n"));
        QVERIFY(write(dir.filePath("user/applications/demo.desktop"),
                      "[Desktop Entry]\nType=Application\nName=User demo\nExec=/bin/true\n"));
        ApplicationsBackend backend(dir.filePath("user"), {dir.filePath("system")},
                                    dir.filePath("Desktop"));
        auto apps = backend.inventory();
        QCOMPARE(apps.size(), 1);
        QCOMPARE(apps[0].toMap()["name"].toString(), QString("User demo"));
        QVERIFY(write(dir.filePath("user/applications/demo.desktop"),
                      "[Desktop Entry]\nType=Application\nName=Hidden\nHidden=true\n"));
        QVERIFY(backend.inventory().isEmpty());
        QVERIFY_EXCEPTION_THROWN(backend.details("../../etc/passwd"), std::runtime_error);
    }
    void shortcutNeverOverwritesExistingFile()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.filePath("user/applications"));
        QDir().mkpath(dir.filePath("Desktop"));
        QFile file(dir.filePath("user/applications/demo.desktop"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[Desktop Entry]\nType=Application\nName=Demo\nExec=/bin/true\n");
        file.close();
        ApplicationsBackend backend(dir.filePath("user"), {}, dir.filePath("Desktop"));
        const QString id = backend.inventory()[0].toMap()["id"].toString();
        backend.shortcut(id);
        QVERIFY_EXCEPTION_THROWN(backend.shortcut(id), std::runtime_error);
        QVERIFY(QFile::exists(dir.filePath("Desktop/demo.desktop")));
    }
    void regionalConsumersObserveSavedFormat()
    {
        QTemporaryDir dir;
        const auto path = dir.filePath("settings.json");
        RegionalPreferences preferences(path);
        RegionSettings::update(path, "country_code", "US");
        preferences.reload();
        QCOMPARE(preferences.timeFormat(), QString("12h"));
        RegionSettings::update(path, "time_format", "24h");
        preferences.reload();
        QCOMPARE(preferences.timeFormat(), QString("24h"));
    }
    void sddmWritesOnlyItsOwnDropInAndHonorsMainFilePrecedence()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.filePath("sddm.conf.d"));
        QFile other(dir.filePath("sddm.conf.d/10-theme.conf"));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.write("[Theme]\nCurrent=example\n");
        other.close();
        SddmAutologin::write("testuser", true, "astrea.desktop", dir.path());
        QCOMPARE(SddmAutologin::read(dir.path(), QString())["User"].toString(),
                 QString("testuser"));
        SddmAutologin::write("testuser", false, QString(), dir.path());
        QVERIFY(SddmAutologin::read(dir.path(), QString())["User"].toString().isEmpty());
        QFile main(dir.filePath("sddm.conf"));
        QVERIFY(main.open(QIODevice::WriteOnly));
        main.write("[Autologin]\nUser=other\n");
        main.close();
        QCOMPARE(SddmAutologin::read(dir.path(), QString())["User"].toString(), QString("other"));
        QVERIFY_EXCEPTION_THROWN(
            SddmAutologin::write("testuser", true, "astrea.desktop", dir.path()),
            std::runtime_error);
        QVERIFY(other.open(QIODevice::ReadOnly));
        QCOMPARE(other.readAll(), QByteArray("[Theme]\nCurrent=example\n"));
    }
    void barClockConsumesSavedRegionalFormat()
    {
        QTemporaryDir dir;
        const auto previous = qgetenv("XDG_CONFIG_HOME");
        qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
        const auto restore = qScopeGuard([previous] {
            if (previous.isNull())
                qunsetenv("XDG_CONFIG_HOME");
            else
                qputenv("XDG_CONFIG_HOME", previous);
        });
        const auto path = dir.filePath("AstreaOS/system/settings.json");
        RegionSettings::update(path, "language", "en_US");
        RegionSettings::update(path, "country_code", "US");
        RegionSettings::update(path, "time_format", "12h");
        BarClockService clock;
        clock.start();
        QVERIFY(QRegularExpression("^[0-9]{2}:[0-9]{2} [AP]M$").match(clock.timeText()).hasMatch());
        RegionSettings::update(path, "time_format", "24h");
        QTRY_VERIFY_WITH_TIMEOUT(
            QRegularExpression("^[0-9]{2}:[0-9]{2}$").match(clock.timeText()).hasMatch(), 6000);
        clock.stop();
    }
    void profileValidationRejectsControlCharacters()
    {
        QCOMPARE(ProfileBackend::validateName("  Jane   Doe  "), QString("Jane Doe"));
        QVERIFY_EXCEPTION_THROWN(ProfileBackend::validateName("bad\nname"), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(ProfileBackend::validateName(""), std::runtime_error);
    }
};
QTEST_GUILESS_MAIN(SettingsDesktopBackendTest)
#include "SettingsDesktopBackendTest.moc"
