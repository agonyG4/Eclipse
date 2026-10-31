#include "core/SettingsController.hpp"
#include "icons/AstreaIconProvider.hpp"
#include "services/i18n/SettingsTranslationController.hpp"
#include "theme/ThemeController.hpp"
#include <QDir>
#include <QFile>
#include <QFont>
#include <QImage>
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
    Page(SettingsController &settings, const QString &path, int width = 960) : theme(themeDir.filePath("theme.json"))
    {
        QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                         [this](const QList<QQmlError> &items) {
                             for (const auto &item : items)
                                 warnings.append(item.toString());
                         });
        engine.addImageProvider(QStringLiteral("astrea-icon"), new AstreaIconProvider);
        engine.rootContext()->setContextProperty("SettingsController", &settings);
        engine.rootContext()->setContextProperty("ThemeController", &theme);
        engine.rootContext()->setContextProperty("I18n", &translations);
        QQmlComponent backdrop(&engine);
        backdrop.setData("import QtQuick\n"
                         "import \"qrc:/qt/qml/Astrea/Settings/qml/components\" as Components\n"
                         "Rectangle { anchors.fill: parent; color: Components.Theme.windowBackground }",
                         QUrl());
        background.reset(backdrop.create());
        if (auto *quick = qobject_cast<QQuickItem *>(background.get()))
            quick->setParentItem(window.contentItem());
        component = std::make_unique<QQmlComponent>(
            &engine, QUrl("qrc:/qt/qml/Astrea/Settings/qml/pages/" + path));
        item.reset(component->create());
        if (auto *quick = qobject_cast<QQuickItem *>(item.get())) {
            quick->setParentItem(window.contentItem());
            quick->setSize({qreal(width), 800});
            window.resize(width, 800);
            window.show();
        }
    }
    bool capture(const QString &name)
    {
        const auto directory = qEnvironmentVariable("ASTREA_SETTINGS_VISUAL_CAPTURE_DIR");
        if (directory.isEmpty())
            return true;
        QTest::qWait(150);
        return QDir().mkpath(directory) &&
               window.grabWindow().save(QDir(directory).filePath(name + ".png"));
    }
    QObject *find(const QString &name) { return item ? item->findChild<QObject *>(name) : nullptr; }
    QTemporaryDir themeDir;
    ThemeController theme;
    SettingsTranslationController translations;
    QStringList warnings;
    QQmlEngine engine;
    QQuickWindow window;
    std::unique_ptr<QObject> background;
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
    void applicationsUseRealCatalogAndCreateShortcut_data()
    {
        QTest::addColumn<int>("width");
        QTest::addColumn<int>("themeMode");
        for (const int width : {528, 778})
            for (const int mode : {0, 1})
                QTest::newRow(qPrintable(QString("%1-%2").arg(width).arg(mode))) << width << mode;
    }
    void applicationsUseRealCatalogAndCreateShortcut()
    {
        QFETCH(int, width);
        QFETCH(int, themeMode);
        DesktopEnvironment env;
        const auto iconPath = env.file("home/demo-app.png");
        QImage demoIcon(16, 16, QImage::Format_ARGB32);
        demoIcon.fill(Qt::cyan);
        QVERIFY(demoIcon.save(iconPath));
        QVERIFY(env.write("data/applications/org.test.Demo.desktop",
                          QByteArray("[Desktop Entry]\nType=Application\nName=Test Demo\n"
                                     "Comment=Real fixture\nExec=/bin/true\nIcon=")
                              + iconPath.toUtf8() + "\nCategories=Utility;\n"));
        SettingsController settings;
        Page page(settings, "apps/Applications.qml", width);
        page.theme.setThemeMode(themeMode);
        const auto captureName = QString("apps-%1-%2").arg(width).arg(themeMode);
        QVERIFY2(page.item, qPrintable(page.component->errorString()));
        QTRY_VERIFY_WITH_TIMEOUT(!settings.applications()->busy(), 5000);
        const auto apps = settings.applications()->state()["apps"].toList();
        bool found = false;
        for (const auto &entry : apps)
            if (entry.toMap()["id"].toString() == "org.test.Demo")
                found = true;
        QVERIFY(found);
        auto *catalog = page.find("installedApplicationsList");
        QVERIFY(catalog);
        QVERIFY(!page.item->property("categoryFilter").isValid());
        QVERIFY(page.find("applicationsListScrollbar"));
        auto *catalogTitle = page.find("applicationsCatalogTitle");
        auto *searchField = page.find("applicationsSearchField");
        auto *countLabel = page.find("applicationsCountLabel");
        QVERIFY(catalogTitle);
        QVERIFY(searchField);
        QVERIFY(countLabel);
        QVERIFY(catalogTitle->property("font").value<QFont>().pixelSize() >= 20);
        QVERIFY(searchField->property("height").toReal() >= 44);
        QVERIFY(searchField->property("width").toReal() > 250);
        QCOMPARE(page.item->property("filteredApps").toList().size(), apps.size());
        page.item->setProperty("searchText", "unmatched-application-name");
        QCOMPARE(page.item->property("filteredApps").toList().size(), 0);
        page.item->setProperty("searchText", "Test Demo");
        QCOMPARE(page.item->property("filteredApps").toList().size(), 1);
        page.item->setProperty("searchText", "");
        QVERIFY(page.capture(captureName + "-list"));
        QQuickItem *appRow = nullptr;
        QList<QQuickItem *> stack{qobject_cast<QQuickItem *>(catalog)};
        while (!stack.isEmpty()) {
            auto *item = stack.takeLast();
            if (item->objectName() == QStringLiteral("applicationRow_org_test_Demo")) {
                appRow = item;
                break;
            }
            stack.append(item->childItems());
        }
        QVERIFY(appRow);
        QVERIFY(appRow->height() >= 74);
        QVERIFY(appRow->activeFocusOnTab());
        const auto center = appRow->mapToScene(QPointF(appRow->width() / 2, appRow->height() / 2));
        QTest::mouseClick(&page.window, Qt::LeftButton, Qt::NoModifier, center.toPoint());
        QTRY_COMPARE(page.item->property("selectedAppId").toString(), QString("org.test.Demo"));
        QTRY_VERIFY_WITH_TIMEOUT(!settings.applications()->busy(), 5000);
        QCOMPARE(settings.applications()->state()["selected"].toMap()["name"].toString(),
                 QString("Test Demo"));
        auto *detailIcon = page.find("applicationDetailIcon");
        auto *detailTitle = page.find("applicationDetailTitle");
        QVERIFY(detailIcon);
        QVERIFY(detailTitle);
        QVERIFY(page.find("applicationInformationCard"));
        QVERIFY(detailIcon->property("iconSize").toInt() >= 72);
        QVERIFY(detailTitle->property("font").value<QFont>().pixelSize() >= 20);
        QCOMPARE(detailIcon->property("resolvedIconName").toString(), iconPath);
        QTRY_VERIFY(detailIcon->property("hasIcon").toBool());
        QVERIFY(page.capture(captureName + "-details"));
        auto *remove = page.find("uninstallApplicationButton");
        QVERIFY(remove);
        QVERIFY(QMetaObject::invokeMethod(remove, "clicked"));
        auto *dialog = page.find("applicationRemovalDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(page.capture(captureName + "-confirmation"));
        auto *cancel = page.find("cancelApplicationRemovalButton");
        QVERIFY(cancel);
        QVERIFY(QMetaObject::invokeMethod(cancel, "clicked"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(QFile::exists(env.file("data/applications/org.test.Demo.desktop")));
        settings.applications()->createShortcut("org.test.Demo");
        QTRY_VERIFY_WITH_TIMEOUT(!settings.applications()->busy(), 5000);
        QVERIFY2(settings.applications()->lastError().isEmpty(),
                 qPrintable(settings.applications()->lastError()));
        QVERIFY(QFile::exists(env.file("home/Desktop/org.test.Demo.desktop")));
        settings.applications()->uninstall("org.test.Demo", "wrong-target");
        QTRY_VERIFY_WITH_TIMEOUT(!settings.applications()->busy(), 5000);
        QVERIFY(!settings.applications()->lastError().isEmpty());
        QVERIFY(QFile::exists(env.file("data/applications/org.test.Demo.desktop")));
        QVERIFY(page.capture(captureName + "-error"));
        auto *back = qobject_cast<QQuickItem *>(page.find("closeApplicationDetailsButton"));
        QVERIFY(back);
        back->forceActiveFocus();
        QTest::keyClick(&page.window, Qt::Key_Return);
        QVERIFY(page.item->property("selectedAppId").toString().isEmpty());
        QVERIFY2(page.warnings.isEmpty(), qPrintable(page.warnings.join('\n')));
    }
    void applicationsUseNativeListScrollbarForLargeCatalog()
    {
        DesktopEnvironment env;
        for (int index = 0; index < 48; ++index) {
            const auto id = QString("org.test.Scroll%1").arg(index, 2, 10, QLatin1Char('0'));
            const QByteArray contents =
                "[Desktop Entry]\nType=Application\nName=Scroll demo "
                + QByteArray::number(index) + "\nExec=/bin/true\n";
            QVERIFY(env.write("data/applications/" + id + ".desktop", contents));
        }
        SettingsController settings;
        Page page(settings, "apps/Applications.qml", 528);
        QVERIFY2(page.item, qPrintable(page.component->errorString()));
        QTRY_VERIFY_WITH_TIMEOUT(!settings.applications()->busy(), 5000);
        auto *list = page.find("installedApplicationsList");
        QVERIFY(list);
        QVERIFY(page.find("applicationsListScrollbar"));
        QVERIFY(page.item->property("filteredApps").toList().size() >= 48);
        QTRY_VERIFY(list->property("contentHeight").toReal() > list->property("height").toReal());
        QVERIFY(list->property("height").toReal() > 100);
        QVERIFY(list->setProperty("contentY", 140.0));
        QTRY_VERIFY(list->property("contentY").toReal() > 100);
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
        auto *avatar = qobject_cast<QQuickItem *>(page.find("changeAvatarButton"));
        QVERIFY2(avatar, "Changing the avatar needs a keyboard-accessible button");
        QVERIFY(avatar->activeFocusOnTab());
        if (avatar->isEnabled()) {
            QSignalSpy activated(avatar, SIGNAL(clicked()));
            avatar->forceActiveFocus();
            QTest::keyClick(&page.window, Qt::Key_Return);
            QCOMPARE(activated.count(), 1);
        }
        auto *password = page.find("changePasswordButton");
        QVERIFY(password);
        QVERIFY(!password->property("enabled").toBool());
        QVERIFY2(page.warnings.isEmpty(), qPrintable(page.warnings.join('\n')));
    }
};
QTEST_MAIN(SettingsDesktopQmlTest)
#include "SettingsDesktopQmlTest.moc"
