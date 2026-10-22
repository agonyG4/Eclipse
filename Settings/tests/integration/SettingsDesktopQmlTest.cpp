#include "core/SettingsController.hpp"
#include "services/i18n/SettingsTranslationController.hpp"
#include "theme/ThemeController.hpp"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

class DesktopEnvironment
{
  public:
    DesktopEnvironment()
    {
        for (const auto &key : QList<QByteArray>{"HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME",
                                                 "XDG_DATA_DIRS", "XDG_STATE_HOME", "PATH"})
            previous[key] = qgetenv(key);
        for (const auto &dir :
             QStringList{"home", "config", "data/applications", "system", "bin", "home/Desktop"})
            QDir().mkpath(root.filePath(dir));
        qputenv("HOME", root.filePath("home").toUtf8());
        qputenv("XDG_CONFIG_HOME", root.filePath("config").toUtf8());
        qputenv("XDG_DATA_HOME", root.filePath("data").toUtf8());
        qputenv("XDG_DATA_DIRS", root.filePath("system").toUtf8());
        qputenv("XDG_STATE_HOME", root.filePath("state").toUtf8());
        qputenv("PATH", root.filePath("bin").toUtf8());
    }
    ~DesktopEnvironment()
    {
        for (auto it = previous.cbegin(); it != previous.cend(); ++it) {
            if (it.value().isNull())
                qunsetenv(it.key());
            else
                qputenv(it.key(), it.value());
        }
    }
    QString file(const QString &path) const { return root.filePath(path); }
    bool write(const QString &path, const QByteArray &bytes)
    {
        QDir().mkpath(QFileInfo(file(path)).absolutePath());
        QFile f(file(path));
        return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
    }

  private:
    QTemporaryDir root;
    QMap<QByteArray, QByteArray> previous;
};

class Page
{
  public:
    Page(SettingsController &settings, const QString &path) : theme(themeDir.filePath("theme.json"))
    {
        QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                         [this](const QList<QQmlError> &items) {
                             for (const auto &item : items)
                                 warnings.append(item.toString());
                         });
        engine.rootContext()->setContextProperty("SettingsController", &settings);
        engine.rootContext()->setContextProperty("ThemeController", &theme);
        engine.rootContext()->setContextProperty("I18n", &translations);
        component = std::make_unique<QQmlComponent>(
            &engine, QUrl("qrc:/qt/qml/Astrea/Settings/qml/pages/" + path));
        item.reset(component->create());
        if (auto *quick = qobject_cast<QQuickItem *>(item.get())) {
            quick->setParentItem(window.contentItem());
            quick->setSize({960, 800});
            window.resize(960, 800);
            window.show();
        }
    }
    QObject *find(const QString &name) { return item ? item->findChild<QObject *>(name) : nullptr; }
    QTemporaryDir themeDir;
    ThemeController theme;
    SettingsTranslationController translations;
    QStringList warnings;
    QQmlEngine engine;
    QQuickWindow window;
    std::unique_ptr<QQmlComponent> component;
    std::unique_ptr<QObject> item;
};
class SettingsDesktopQmlTest : public QObject
{
    Q_OBJECT
  private slots:
    void languagePersistsAndReloads()
    {
        DesktopEnvironment env;
        SettingsController settings;
        Page page(settings, "system/Language.qml");
        QVERIFY2(page.item, qPrintable(page.component->errorString()));
        QTRY_VERIFY_WITH_TIMEOUT(!settings.region()->busy(), 5000);
        settings.region()->setOption("language", "pt_BR");
        QTRY_VERIFY_WITH_TIMEOUT(!settings.region()->busy(), 5000);
        QVERIFY2(settings.region()->lastError().isEmpty(),
                 qPrintable(settings.region()->lastError()));
        settings.region()->setOption("country_code", "US");
        QTRY_VERIFY_WITH_TIMEOUT(!settings.region()->busy(), 5000);
        settings.region()->setOption("time_format", "12h");
        QTRY_VERIFY_WITH_TIMEOUT(!settings.region()->busy(), 5000);
        QCOMPARE(settings.region()
                     ->state()["config"]
                     .toMap()["region"]
                     .toMap()["time_format"]
                     .toString(),
                 QString("12h"));
        QFile file(env.file("config/AstreaOS/system/settings.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object()["language"].toString(),
                 QString("pt_BR"));
        SettingsController reloaded;
        reloaded.region()->refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!reloaded.region()->busy(), 5000);
        QCOMPARE(reloaded.region()->state()["config"].toMap()["language"].toString(),
                 QString("pt_BR"));
        SettingsTranslationController translated;
        QCOMPARE(translated.tr(QStringLiteral("settings.language.header")),
                 QString::fromUtf8("IDIOMA E REGIÃO"));
        QVERIFY2(page.warnings.isEmpty(), qPrintable(page.warnings.join('\n')));
    }
    void applicationsUseRealCatalogAndCreateShortcut()
    {
        DesktopEnvironment env;
        QVERIFY(env.write("data/applications/org.test.Demo.desktop",
                          "[Desktop Entry]\nType=Application\nName=Test Demo\nComment=Real "
                          "fixture\nExec=/bin/true\nCategories=Utility;\n"));
        SettingsController settings;
        Page page(settings, "apps/Applications.qml");
        QVERIFY2(page.item, qPrintable(page.component->errorString()));
        QTRY_VERIFY_WITH_TIMEOUT(!settings.applications()->busy(), 5000);
        const auto apps = settings.applications()->state()["apps"].toList();
        bool found = false;
        for (const auto &entry : apps)
            if (entry.toMap()["id"].toString() == "org.test.Demo")
                found = true;
        QVERIFY(found);
        settings.applications()->select("org.test.Demo");
        QTRY_VERIFY_WITH_TIMEOUT(!settings.applications()->busy(), 5000);
        QCOMPARE(settings.applications()->state()["selected"].toMap()["name"].toString(),
                 QString("Test Demo"));
        settings.applications()->createShortcut("org.test.Demo");
        QTRY_VERIFY_WITH_TIMEOUT(!settings.applications()->busy(), 5000);
        QVERIFY2(settings.applications()->lastError().isEmpty(),
                 qPrintable(settings.applications()->lastError()));
        QVERIFY(QFile::exists(env.file("home/Desktop/org.test.Demo.desktop")));
        settings.applications()->uninstall("org.test.Demo", "wrong-target");
        QTRY_VERIFY_WITH_TIMEOUT(!settings.applications()->busy(), 5000);
        QVERIFY(!settings.applications()->lastError().isEmpty());
        QVERIFY(QFile::exists(env.file("data/applications/org.test.Demo.desktop")));
        QVERIFY2(page.warnings.isEmpty(), qPrintable(page.warnings.join('\n')));
    }
    void profileLoadsCurrentAccountWithoutMutation()
    {
        DesktopEnvironment env;
        SettingsController settings;
        Page page(settings, "personalization/UserProfile.qml");
        QVERIFY2(page.item, qPrintable(page.component->errorString()));
        QTRY_VERIFY_WITH_TIMEOUT(!settings.profile()->busy(), 10000);
        QVERIFY2(settings.profile()->lastError().isEmpty(),
                 qPrintable(settings.profile()->lastError()));
        QVERIFY(!settings.profile()->state()["user"].toString().isEmpty());
        QVERIFY(!page.find("userProfilePreviewNotice"));
        auto *password = page.find("changePasswordButton");
        QVERIFY(password);
        QVERIFY(!password->property("enabled").toBool());
        QVERIFY2(page.warnings.isEmpty(), qPrintable(page.warnings.join('\n')));
    }
};
QTEST_MAIN(SettingsDesktopQmlTest)
#include "SettingsDesktopQmlTest.moc"
