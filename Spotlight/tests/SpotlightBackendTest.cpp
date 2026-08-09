#include <QTest>
#include <QSignalSpy>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QTemporaryDir>
#include <QLocalServer>
#include <QLocalSocket>

#include "../core/SpotlightResultsModel.hpp"
#include "../platform/runtime/SpotlightRuntimePaths.hpp"
#include "../platform/rust/RustSpotlightBackend.hpp"
#include "../services/AstreaI18n.hpp"
#include "icons/AstreaIconProvider.hpp"
#include "../platform/ipc/SpotlightIpcServer.hpp"
#include "../services/GameModeMonitor.hpp"
#include "../services/SpotlightConfigWatcher.hpp"
#include "../services/ApplicationLauncher.hpp"
#include "../core/SpotlightController.hpp"
#include "apps/DesktopEntryCatalog.hpp"
#include "icons/AstreaIconTheme.hpp"

class TestSpotlightBackend : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void testResultsRoles();
    void testResultsClear();
    void testExternalCatalogSnapshot();
    void testSharedCatalogSnapshot();
    void testIpcParseCommands();
    void testIpcRoundtrip();
    void testIpcStatusResponse();
    void testIpcLargeCommandRejected();
    void testI18nFallback();
    void testI18nParams();
    void testIconProvider();
    void testGameModePoll();
    void testGameModeInactive();
    void testConfigWatcherDefaults();
    void testConfigWatcherWeatherFlag();
    void testSelectionWrapping();
    void testConfigSync_weatherDisabled();
    void testConfigSync_weatherEnabled();
    void testConfigSync_componentDisabled();
    void testConfigSync_liveUpdate();
    void testConfigSync_atomicReplacement();
    void testResolveIconJson_validJson();
    void testApplicationLauncher();
    void testIconThemePriority_astreaEnv();
    void testIconThemePriority_qsEnv();
    void testIconThemePriority_qt6ct();
    void testIconThemePriority_astreaOverridesQs();
    void testIconThemeFallback_hicolor();
    void testIconThemeFallback_compatibility();
    void testIconThemeWhitespaceIgnored();
    void testIconThemeProviderSwitch();
    void testIconThemeInheritance();
};

void TestSpotlightBackend::initTestCase() {
    QCoreApplication::setOrganizationName(QStringLiteral("AstreaOS"));
    QCoreApplication::setApplicationName(QStringLiteral("Astrea Spotlight Test"));
}

void TestSpotlightBackend::testResultsRoles() {
    SpotlightResultsModel model;
    QHash<int, QByteArray> roles = model.roleNames();

    QVERIFY(roles.contains(Qt::UserRole + 1));
    QCOMPARE(roles.value(Qt::UserRole + 1), QByteArray("name"));
    QVERIFY(roles.contains(Qt::UserRole + 5));
    QCOMPARE(roles.value(Qt::UserRole + 5), QByteArray("entryId"));

    QVERIFY(model.rowCount() == 0);
}

void TestSpotlightBackend::testResultsClear() {
    SpotlightResultsModel model;
    QJsonArray arr;
    QJsonObject obj;
    obj[QStringLiteral("name")] = QStringLiteral("TestApp");
    obj[QStringLiteral("id")] = QStringLiteral("testapp");
    obj[QStringLiteral("icon")] = QStringLiteral("test-icon");
    arr.append(obj);

    model.setResults(arr);
    QCOMPARE(model.rowCount(), 1);

    model.clear();
    QCOMPARE(model.rowCount(), 0);
}

void TestSpotlightBackend::testExternalCatalogSnapshot() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QJsonObject entry{
        {QStringLiteral("id"), QStringLiteral("shared-spotlight")},
        {QStringLiteral("name"), QStringLiteral("Shared Spotlight")},
        {QStringLiteral("generic_name"), QStringLiteral("Unified Launcher")},
        {QStringLiteral("comment"), QStringLiteral("Provided by the shared catalog")},
        {QStringLiteral("icon"), QStringLiteral("shared-spotlight-icon")},
        {QStringLiteral("exec"), QStringLiteral("shared-spotlight")},
        {QStringLiteral("try_exec"), QString()},
        {QStringLiteral("keywords"), QJsonArray{QStringLiteral("unified"), QStringLiteral("catalog")}},
        {QStringLiteral("categories"), QJsonArray{QStringLiteral("Utility")}},
        {QStringLiteral("path"), QStringLiteral("/shared/spotlight.desktop")},
        {QStringLiteral("startup_wm_class"), QStringLiteral("SharedSpotlight")},
        {QStringLiteral("desktop_file_name"), QStringLiteral("shared-spotlight.desktop")},
        {QStringLiteral("terminal"), false},
        {QStringLiteral("hidden"), false},
        {QStringLiteral("no_display"), false},
        {QStringLiteral("only_show_in"), QJsonArray{}},
        {QStringLiteral("not_show_in"), QJsonArray{}}
    };

    RustSpotlightBackend backend;
    QString error;
    QVERIFY2(backend.createWithCatalog(tempDir.path(), QStringLiteral("en_US"),
                                       QJsonArray{entry}, &error),
             qPrintable(error));

    QJsonArray results = backend.search(QStringLiteral("shared spotlight"), 6, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().toObject().value(QStringLiteral("id")).toString(),
             QStringLiteral("shared-spotlight"));

    QVERIFY2(backend.reload(&error), qPrintable(error));
    results = backend.search(QStringLiteral("unified launcher"), 6, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().toObject().value(QStringLiteral("desktopFileName")).toString(),
             QStringLiteral("shared-spotlight.desktop"));
}

void TestSpotlightBackend::testSharedCatalogSnapshot()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString applications = home.path() + QStringLiteral("/.local/share/applications");
    QVERIFY(QDir().mkpath(applications));
    QFile file(applications + QStringLiteral("/m7d-shared.desktop"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("[Desktop Entry]\nType=Application\nName=M7D Unified Catalog\n"
               "GenericName=Shared Launcher\nComment=One catalog source\n"
               "Icon=m7d-shared-icon\nExec=m7d-shared\nKeywords=unified;shared;\n"
               "Categories=Utility;\n");
    file.close();

    DesktopEntryCatalog catalog;
    catalog.initialize(home.path());

    RustSpotlightBackend backend;
    QString error;
    QVERIFY2(backend.createWithCatalog(home.path(), QStringLiteral("en_US"),
                                       catalog.snapshotJson(), &error),
             qPrintable(error));
    const QJsonArray results = backend.search(QStringLiteral("m7d unified"), 6, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(!results.isEmpty());
    QCOMPARE(results.first().toObject().value(QStringLiteral("icon")).toString(),
             QStringLiteral("m7d-shared-icon"));
}

void TestSpotlightBackend::testIpcParseCommands() {
    using IpcCommand = SpotlightIpcServer::IpcCommand;

    auto cmd = SpotlightIpcServer::parseCommand(QStringLiteral("show"));
    QCOMPARE(cmd.type, SpotlightIpcServer::Command::Show);

    cmd = SpotlightIpcServer::parseCommand(QStringLiteral("hide"));
    QCOMPARE(cmd.type, SpotlightIpcServer::Command::Hide);

    cmd = SpotlightIpcServer::parseCommand(QStringLiteral("toggle"));
    QCOMPARE(cmd.type, SpotlightIpcServer::Command::Toggle);

    cmd = SpotlightIpcServer::parseCommand(QStringLiteral("query firefox"));
    QCOMPARE(cmd.type, SpotlightIpcServer::Command::Query);
    QCOMPARE(cmd.text, QStringLiteral("firefox"));

    cmd = SpotlightIpcServer::parseCommand(QStringLiteral("status"));
    QCOMPARE(cmd.type, SpotlightIpcServer::Command::Status);

    cmd = SpotlightIpcServer::parseCommand(QStringLiteral("activate"));
    QCOMPARE(cmd.type, SpotlightIpcServer::Command::Activate);

    cmd = SpotlightIpcServer::parseCommand(QStringLiteral("reload-index"));
    QCOMPARE(cmd.type, SpotlightIpcServer::Command::ReloadIndex);

    cmd = SpotlightIpcServer::parseCommand(QStringLiteral("unknown-command"));
    QCOMPARE(cmd.type, SpotlightIpcServer::Command::Unknown);

    cmd = SpotlightIpcServer::parseCommand(QStringLiteral("--toggle"));
    QCOMPARE(cmd.type, SpotlightIpcServer::Command::Toggle);

    cmd = SpotlightIpcServer::parseCommand(QStringLiteral("--query firefox test"));
    QCOMPARE(cmd.type, SpotlightIpcServer::Command::Query);

    cmd = SpotlightIpcServer::parseCommand(QStringLiteral("--daemon"));
    QCOMPARE(cmd.type, SpotlightIpcServer::Command::Hide);
}

void TestSpotlightBackend::testIpcRoundtrip() {
    QString serverName = QStringLiteral("astrea-spotlight-test-ipc");
    QLocalServer::removeServer(serverName);

    SpotlightIpcServer server;
    QSignalSpy spy(&server, &SpotlightIpcServer::commandReceived);
    QVERIFY(server.listen(serverName));

    {
        SpotlightIpcServer::IpcCommand cmd;
        cmd.type = SpotlightIpcServer::Command::Toggle;
        QVERIFY(SpotlightIpcServer::sendCommand(serverName, cmd));
    }

    QTRY_COMPARE(spy.count(), 1);
    auto received = spy.at(0).at(0).value<SpotlightIpcServer::IpcCommand>();
    QCOMPARE(received.type, SpotlightIpcServer::Command::Toggle);

    server.stopListening();
    QLocalServer::removeServer(serverName);
}

void TestSpotlightBackend::testIpcStatusResponse() {
    QString serverName = QStringLiteral("astrea-spotlight-test-status2");
    QLocalServer::removeServer(serverName);

    SpotlightIpcServer server;
    QSignalSpy spy(&server, &SpotlightIpcServer::commandReceived);
    QVERIFY(server.listen(serverName));

    bool callbackCalled = false;
    server.setReplyCallback([&](const SpotlightIpcServer::IpcCommand &cmd) {
        Q_UNUSED(cmd);
        callbackCalled = true;
        return QStringLiteral("visible=0;open=0;results=0;component=1;gamemode=0;layershell=unavailable");
    });

    SpotlightIpcServer::IpcCommand statusCmd;
    statusCmd.type = SpotlightIpcServer::Command::Status;
    QByteArray replyBytes = SpotlightIpcServer::requestReply(serverName, statusCmd, 500);
    QVERIFY(!replyBytes.isEmpty());
    QString reply = QString::fromUtf8(replyBytes);
    QVERIFY(spy.count() == 1 || spy.wait(500));
    QCOMPARE(spy.count(), 1);
    QVERIFY(callbackCalled);

    QVERIFY(reply.contains(QStringLiteral("visible=0")));
    QVERIFY(reply.contains(QStringLiteral("open=0")));
    QVERIFY(reply.contains(QStringLiteral("results=0")));
    QVERIFY(reply.contains(QStringLiteral("component=1")));
    QVERIFY(reply.contains(QStringLiteral("gamemode=0")));
    QVERIFY(reply.contains(QStringLiteral("layershell=unavailable")));

    server.stopListening();
    QLocalServer::removeServer(serverName);
}

void TestSpotlightBackend::testIpcLargeCommandRejected() {
    QString serverName = QStringLiteral("astrea-spotlight-test-large");
    QLocalServer::removeServer(serverName);

    SpotlightIpcServer server;
    QSignalSpy spy(&server, &SpotlightIpcServer::commandReceived);
    QVERIFY(server.listen(serverName));

    QString longQuery = QStringLiteral("a").repeated(5000);
    QLocalSocket socket;
    socket.connectToServer(serverName);
    QVERIFY(socket.waitForConnected(500));
    QByteArray data = "query " + longQuery.toUtf8() + "\n";
    QVERIFY(data.size() > 4096);
    socket.write(data);
    socket.waitForBytesWritten(500);
    socket.disconnectFromServer();

    QTest::qWait(200);
    QCOMPARE(spy.count(), 0);

    server.stopListening();
    QLocalServer::removeServer(serverName);
}

void TestSpotlightBackend::testI18nFallback() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir dir(tempDir.path());
    QFile file(dir.filePath(QStringLiteral("en_US.json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{\"test.key\": \"Test Value\"}");
    file.close();

    AstreaI18n i18n(tempDir.path());
    QCOMPARE(i18n.tr(QStringLiteral("test.key")), QStringLiteral("Test Value"));
    QCOMPARE(i18n.tr(QStringLiteral("missing.key"), QStringLiteral("Fallback")), QStringLiteral("Fallback"));
    QCOMPARE(i18n.tr(QStringLiteral("missing.key")), QStringLiteral("missing.key"));
}

void TestSpotlightBackend::testI18nParams() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir dir(tempDir.path());
    QFile file(dir.filePath(QStringLiteral("en_US.json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("{\"greeting\": \"Hello {name}!\"}");
    file.close();

    AstreaI18n i18n(tempDir.path());
    QVariantMap params;
    params[QStringLiteral("name")] = QStringLiteral("World");
    QCOMPARE(i18n.tr(QStringLiteral("greeting"), {}, params), QStringLiteral("Hello World!"));
}

void TestSpotlightBackend::testIconProvider() {
    AstreaIconProvider provider;
    QSize size;

    // Missing icon returns null pixmap (QML shows initials fallback)
    QPixmap missing = provider.requestPixmap(QStringLiteral("non-existent-icon-that-wont-crash"), &size, QSize(48, 48));
    QVERIFY(missing.isNull());

    // Empty icon name returns null
    QPixmap empty = provider.requestPixmap(QString(), &size, QSize(48, 48));
    QVERIFY(empty.isNull());

    // Blank icon name returns null
    QPixmap blank = provider.requestPixmap(QStringLiteral(" "), &size, QSize(48, 48));
    QVERIFY(blank.isNull());
}

void TestSpotlightBackend::testGameModePoll() {
    QVERIFY(GameModeMonitor::parseGameModeOutput("gamemoded is active"));
    QVERIFY(GameModeMonitor::parseGameModeOutput("active"));
    QVERIFY(GameModeMonitor::parseGameModeOutput("  GAMEMODED IS ACTIVE  "));

    QVERIFY(!GameModeMonitor::parseGameModeOutput("gamemoded is not active"));
    QVERIFY(!GameModeMonitor::parseGameModeOutput("inactive"));
    QVERIFY(!GameModeMonitor::parseGameModeOutput(""));
    QVERIFY(!GameModeMonitor::parseGameModeOutput("gamemoded: no client connected"));
    QVERIFY(!GameModeMonitor::parseGameModeOutput("garbage text"));
}

void TestSpotlightBackend::testGameModeInactive() {
    QVERIFY(!GameModeMonitor::parseGameModeOutput("gamemoded is not active"));
    QVERIFY(!GameModeMonitor::parseGameModeOutput("is not active"));
    QVERIFY(!GameModeMonitor::parseGameModeOutput("inactive"));
    QVERIFY(!GameModeMonitor::parseGameModeOutput("INACTIVE"));
    QVERIFY(!GameModeMonitor::parseGameModeOutput(""));
}

void TestSpotlightBackend::testConfigWatcherDefaults() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString configPath = tempDir.path() + QStringLiteral("/spotlight.json");
    QString componentsPath = tempDir.path() + QStringLiteral("/components.json");

    {
        QFile f(componentsPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"spotlight\": false}");
        f.close();
    }

    SpotlightConfigWatcher watcher(configPath, componentsPath);
    QVERIFY(!watcher.componentEnabled());

    watcher.refresh();
    QVERIFY(!watcher.componentEnabled());
}

void TestSpotlightBackend::testConfigWatcherWeatherFlag() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString configPath = tempDir.path() + QStringLiteral("/spotlight.json");
    QString componentsPath = tempDir.path() + QStringLiteral("/components.json");

    {
        QFile f(configPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"weather\": false}");
        f.close();
    }

    SpotlightConfigWatcher watcher(configPath, componentsPath);
    QVERIFY(!watcher.weatherEnabled());

    // Update config with weather: true
    {
        QFile f(configPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"weather\": true}");
        f.close();
    }
    watcher.refresh();
    QVERIFY(watcher.weatherEnabled());
}

void TestSpotlightBackend::testSelectionWrapping() {
    SpotlightRuntimePaths paths = SpotlightRuntimePaths::fromEnvironment();
    SpotlightController controller(paths);
    SpotlightResultsModel *model = controller.resultsModel();

    QJsonArray arr;
    for (int i = 0; i < 3; i++) {
        QJsonObject obj;
        obj[QStringLiteral("name")] = QStringLiteral("App%1").arg(i);
        obj[QStringLiteral("id")] = QStringLiteral("app%1").arg(i);
        arr.append(obj);
    }
    model->setResults(arr);
    QCOMPARE(model->rowCount(), 3);

    controller.setSelectedIndex(0);
    QCOMPARE(controller.selectedIndex(), 0);
    controller.setSelectedIndex(2);
    QCOMPARE(controller.selectedIndex(), 2);
    controller.setSelectedIndex(-1);
    QCOMPARE(controller.selectedIndex(), 0);
    controller.setSelectedIndex(100);
    QCOMPARE(controller.selectedIndex(), 2);

    controller.setSelectedIndex(0);
    controller.moveSelection(1);
    QCOMPARE(controller.selectedIndex(), 1);
    controller.moveSelection(1);
    QCOMPARE(controller.selectedIndex(), 2);
    controller.moveSelection(1);
    QCOMPARE(controller.selectedIndex(), 0);
    controller.moveSelection(-1);
    QCOMPARE(controller.selectedIndex(), 2);
}

void TestSpotlightBackend::testApplicationLauncher() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString launchPath = tempDir.path() + QStringLiteral("/astrea-launch");
    QFile f(launchPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("#!/bin/sh\necho \"$@\" > \"$0.output\"\n");
    f.close();
    f.setPermissions(QFileDevice::ExeOwner | QFileDevice::ExeUser |
                     QFileDevice::ReadOwner | QFileDevice::ReadUser |
                     QFileDevice::WriteOwner);

    ApplicationLauncher launcher(launchPath);
    QSignalSpy successSpy(&launcher, &ApplicationLauncher::launchSucceeded);
    QSignalSpy failSpy(&launcher, &ApplicationLauncher::launchFailed);

    launcher.launchDesktop(QString(), QString(), QString());
    QCOMPARE(failSpy.count(), 1);

    launcher.launchDesktop(QStringLiteral("firefox"), QStringLiteral("firefox.desktop"), QString());
    QTRY_VERIFY(successSpy.count() >= 1);

    QString outputFile = launchPath + QStringLiteral(".output");
    QFile output(outputFile);
    QVERIFY(output.open(QIODevice::ReadOnly));
    QString written = QString::fromUtf8(output.readAll()).trimmed();
    QCOMPARE(written, QStringLiteral("--desktop firefox.desktop"));
    QCOMPARE(successSpy.last().at(0).toString(), QStringLiteral("firefox.desktop"));

    successSpy.clear();
    failSpy.clear();
    output.remove();

    launcher.launchDesktop(QString(), QString(), QStringLiteral("myapp --option"));
    QTRY_VERIFY(successSpy.count() >= 1);

    QFile output2(outputFile);
    QVERIFY(output2.open(QIODevice::ReadOnly));
    QString written2 = QString::fromUtf8(output2.readAll()).trimmed();
    QVERIFY(written2.startsWith(QStringLiteral("--argv-json")));
    QVERIFY(written2.contains(QStringLiteral("myapp")));
    QVERIFY(written2.contains(QStringLiteral("--option")));
}

// ============================================================
// Icon theme helper functions
// ============================================================

static QString createFakeTheme(const QString &baseDir, const QString &themeName,
                               const QStringList &inherits = {}) {
    QString themeDir = baseDir + QStringLiteral("/icons/") + themeName;
    QDir().mkpath(themeDir);
    QFile index(themeDir + QStringLiteral("/index.theme"));
    if (index.open(QIODevice::WriteOnly)) {
        QTextStream out(&index);
        out << "[Icon Theme]\n";
        if (!inherits.isEmpty())
            out << "Inherits=" << inherits.join(QStringLiteral(",")) << "\n";
        out << "Directories=scalable/apps\n";
        index.close();
    }
    return themeDir;
}

static void createFakeIcon(const QString &path) {
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        QTextStream out(&f);
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
            << "width=\"48\" height=\"48\" viewBox=\"0 0 48 48\">\n"
            << "<rect width=\"48\" height=\"48\" fill=\"red\"/>\n"
            << "</svg>\n";
        f.close();
    }
}

// ============================================================
// Icon theme tests (inside TestSpotlightBackend)
// ============================================================

void TestSpotlightBackend::testIconThemePriority_astreaEnv() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    createFakeTheme(tempDir.path(), QStringLiteral("ThemeA"));
    createFakeTheme(tempDir.path(), QStringLiteral("ThemeB"));

    QByteArray oldDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", tempDir.path().toUtf8());
    qputenv("ASTREA_ICON_THEME", "ThemeA");
    qunsetenv("QS_ICON_THEME");

    QString theme = AstreaIconTheme::resolve();
    QCOMPARE(theme, QStringLiteral("ThemeA"));
    QCOMPARE(AstreaIconTheme::themeSource(), QStringLiteral("ASTREA_ICON_THEME"));

    qunsetenv("ASTREA_ICON_THEME");
    if (oldDataHome.isEmpty()) qunsetenv("XDG_DATA_HOME");
    else qputenv("XDG_DATA_HOME", oldDataHome);
}

void TestSpotlightBackend::testIconThemePriority_qsEnv() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    createFakeTheme(tempDir.path(), QStringLiteral("ThemeB"));

    QByteArray oldDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", tempDir.path().toUtf8());
    qputenv("QS_ICON_THEME", "ThemeB");

    QString theme = AstreaIconTheme::resolve();
    QCOMPARE(theme, QStringLiteral("ThemeB"));
    QCOMPARE(AstreaIconTheme::themeSource(), QStringLiteral("QS_ICON_THEME"));

    qunsetenv("QS_ICON_THEME");
    if (oldDataHome.isEmpty()) qunsetenv("XDG_DATA_HOME");
    else qputenv("XDG_DATA_HOME", oldDataHome);
}

void TestSpotlightBackend::testIconThemePriority_qt6ct() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    createFakeTheme(tempDir.path(), QStringLiteral("ThemeA"));

    QString configDir = tempDir.path() + QStringLiteral("/qt6ct");
    QDir().mkpath(configDir);
    QFile cfg(configDir + QStringLiteral("/qt6ct.conf"));
    QVERIFY(cfg.open(QIODevice::WriteOnly));
    QTextStream out(&cfg);
    out << "[Appearance]\nicon_theme=ThemeA\n";
    cfg.close();

    QByteArray oldConfigHome = qgetenv("XDG_CONFIG_HOME");
    qputenv("XDG_CONFIG_HOME", tempDir.path().toUtf8());

    // Temporarily override HOME so the AstreaIconTheme uses the right config path
    // Actually themeExists checks XDG_DATA_HOME/icons, so set that too
    QByteArray oldDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", tempDir.path().toUtf8());

    QString theme = AstreaIconTheme::resolve();
    QCOMPARE(theme, QStringLiteral("ThemeA"));
    QCOMPARE(AstreaIconTheme::themeSource(), QStringLiteral("qt6ct"));

    if (oldConfigHome.isEmpty()) qunsetenv("XDG_CONFIG_HOME");
    else qputenv("XDG_CONFIG_HOME", oldConfigHome);
    if (oldDataHome.isEmpty()) qunsetenv("XDG_DATA_HOME");
    else qputenv("XDG_DATA_HOME", oldDataHome);
}

void TestSpotlightBackend::testIconThemePriority_astreaOverridesQs() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    createFakeTheme(tempDir.path(), QStringLiteral("ThemeA"));
    createFakeTheme(tempDir.path(), QStringLiteral("ThemeB"));

    QByteArray oldDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", tempDir.path().toUtf8());
    qputenv("ASTREA_ICON_THEME", "ThemeA");
    qputenv("QS_ICON_THEME", "ThemeB");

    // ASTREA_ICON_THEME should win over QS_ICON_THEME
    QString theme = AstreaIconTheme::resolve();
    QCOMPARE(theme, QStringLiteral("ThemeA"));

    qunsetenv("ASTREA_ICON_THEME");
    qunsetenv("QS_ICON_THEME");
    if (oldDataHome.isEmpty()) qunsetenv("XDG_DATA_HOME");
    else qputenv("XDG_DATA_HOME", oldDataHome);
}

void TestSpotlightBackend::testIconThemeFallback_hicolor() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    createFakeTheme(tempDir.path(), QStringLiteral("hicolor"));

    QByteArray oldDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", tempDir.path().toUtf8());
    QIcon::setThemeName(QStringLiteral("NonExistentTheme"));

    QString theme = AstreaIconTheme::resolve();
    QCOMPARE(theme, QStringLiteral("hicolor"));
    QCOMPARE(AstreaIconTheme::themeSource(), QStringLiteral("fallback"));

    QIcon::setThemeName(QStringLiteral("hicolor"));
    if (oldDataHome.isEmpty()) qunsetenv("XDG_DATA_HOME");
    else qputenv("XDG_DATA_HOME", oldDataHome);
}

void TestSpotlightBackend::testIconThemeFallback_compatibility() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    createFakeTheme(tempDir.path(), QStringLiteral("WhiteSur-dark"));
    createFakeTheme(tempDir.path(), QStringLiteral("hicolor"));

    QByteArray oldDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", tempDir.path().toUtf8());
    QIcon::setThemeName(QStringLiteral("NonExistentTheme"));

    QString theme = AstreaIconTheme::resolve();
    QCOMPARE(theme, QStringLiteral("WhiteSur-dark"));
    QCOMPARE(AstreaIconTheme::themeSource(), QStringLiteral("compatibility"));

    QIcon::setThemeName(QStringLiteral("hicolor"));
    if (oldDataHome.isEmpty()) qunsetenv("XDG_DATA_HOME");
    else qputenv("XDG_DATA_HOME", oldDataHome);
}

void TestSpotlightBackend::testIconThemeWhitespaceIgnored() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    createFakeTheme(tempDir.path(), QStringLiteral("WhiteSur-dark"));

    // Override both XDG_DATA_HOME and XDG_DATA_DIRS so only our temp dir is searched
    QByteArray oldDataHome = qgetenv("XDG_DATA_HOME");
    QByteArray oldDataDirs = qgetenv("XDG_DATA_DIRS");
    qputenv("XDG_DATA_HOME", tempDir.path().toUtf8());
    qputenv("XDG_DATA_DIRS", tempDir.path().toUtf8());

    // Make sure platform theme doesn't exist anywhere accessible
    QIcon::setThemeName(QStringLiteral("NonExistentPlatformTheme"));

    qputenv("ASTREA_ICON_THEME", "");
    qputenv("QS_ICON_THEME", "  ");

    QString theme = AstreaIconTheme::resolve();
    QCOMPARE(theme, QStringLiteral("WhiteSur-dark"));

    qunsetenv("ASTREA_ICON_THEME");
    qunsetenv("QS_ICON_THEME");
    if (oldDataHome.isEmpty()) qunsetenv("XDG_DATA_HOME");
    else qputenv("XDG_DATA_HOME", oldDataHome);
    if (oldDataDirs.isEmpty()) qunsetenv("XDG_DATA_DIRS");
    else qputenv("XDG_DATA_DIRS", oldDataDirs);
    QIcon::setThemeName(QStringLiteral("hicolor"));
}

void TestSpotlightBackend::testIconThemeProviderSwitch() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    createFakeTheme(tempDir.path(), QStringLiteral("ThemeA"));
    createFakeTheme(tempDir.path(), QStringLiteral("ThemeB"));
    createFakeTheme(tempDir.path(), QStringLiteral("hicolor"));
    createFakeIcon(tempDir.path() + QStringLiteral("/icons/ThemeA/scalable/apps/app.svg"));
    createFakeIcon(tempDir.path() + QStringLiteral("/icons/ThemeB/scalable/apps/app.svg"));
    createFakeIcon(tempDir.path() + QStringLiteral("/icons/hicolor/scalable/apps/app.svg"));

    QByteArray oldDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", tempDir.path().toUtf8());

    QIcon::setThemeName(QStringLiteral("ThemeA"));
    AstreaIconProvider provider;
    QSize size;
    const int initialRevision = provider.themeRevision();

    QPixmap pmA = provider.requestPixmap(QStringLiteral("app"), &size, QSize(48, 48));
    QVERIFY(!pmA.isNull());

    QIcon::setThemeName(QStringLiteral("ThemeB"));
    provider.clearCache();
    QCOMPARE(provider.themeRevision(), initialRevision + 1);

    QPixmap pmB = provider.requestPixmap(QStringLiteral("app"), &size, QSize(48, 48));
    QVERIFY(!pmB.isNull());

    QIcon::setThemeName(QStringLiteral("hicolor"));
    if (oldDataHome.isEmpty()) qunsetenv("XDG_DATA_HOME");
    else qputenv("XDG_DATA_HOME", oldDataHome);
}

void TestSpotlightBackend::testIconThemeInheritance() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    createFakeTheme(tempDir.path(), QStringLiteral("ParentTheme"));
    createFakeTheme(tempDir.path(), QStringLiteral("ChildTheme"),
                    {QStringLiteral("ParentTheme")});
    createFakeTheme(tempDir.path(), QStringLiteral("hicolor"));
    createFakeIcon(tempDir.path() + QStringLiteral("/icons/ParentTheme/scalable/apps/parent-app.svg"));

    QByteArray oldDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", tempDir.path().toUtf8());

    QIcon::setThemeName(QStringLiteral("ChildTheme"));
    AstreaIconProvider provider;
    QSize size;

    QPixmap pm = provider.requestPixmap(QStringLiteral("parent-app"), &size, QSize(48, 48));
    QVERIFY(!pm.isNull());

    QIcon::setThemeName(QStringLiteral("hicolor"));
    if (oldDataHome.isEmpty()) qunsetenv("XDG_DATA_HOME");
    else qputenv("XDG_DATA_HOME", oldDataHome);
}

// ============================================================
// Config sync tests — verify initial state propagation
// ============================================================

void TestSpotlightBackend::testConfigSync_weatherDisabled() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString configPath = tempDir.path() + QStringLiteral("/spotlight.json");
    QString componentsPath = tempDir.path() + QStringLiteral("/components.json");

    {
        QFile f(configPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"weather\": false}");
        f.close();
    }
    {
        QFile f(componentsPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{}");
        f.close();
    }

    SpotlightConfigWatcher watcher(configPath, componentsPath);
    QVERIFY(!watcher.weatherEnabled());

    SpotlightRuntimePaths paths = SpotlightRuntimePaths::fromEnvironment();
    SpotlightController controller(paths);
    QVERIFY(controller.weatherEnabled()); // default is true

    // Simulate the fix: apply initial watcher state after signal wiring
    controller.applyConfig(watcher.spotlightConfig());

    QVERIFY(!controller.weatherEnabled());
}

void TestSpotlightBackend::testConfigSync_weatherEnabled() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString configPath = tempDir.path() + QStringLiteral("/spotlight.json");
    QString componentsPath = tempDir.path() + QStringLiteral("/components.json");

    {
        QFile f(configPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"weather\": true}");
        f.close();
    }
    {
        QFile f(componentsPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{}");
        f.close();
    }

    SpotlightConfigWatcher watcher(configPath, componentsPath);
    QVERIFY(watcher.weatherEnabled());

    SpotlightRuntimePaths paths = SpotlightRuntimePaths::fromEnvironment();
    SpotlightController controller(paths);
    QVERIFY(controller.weatherEnabled());

    controller.applyConfig(watcher.spotlightConfig());

    QVERIFY(controller.weatherEnabled());
}

void TestSpotlightBackend::testConfigSync_componentDisabled() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString configPath = tempDir.path() + QStringLiteral("/spotlight.json");
    QString componentsPath = tempDir.path() + QStringLiteral("/components.json");

    {
        QFile f(configPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{}");
        f.close();
    }
    {
        QFile f(componentsPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"spotlight\": false}");
        f.close();
    }

    SpotlightConfigWatcher watcher(configPath, componentsPath);
    QVERIFY(!watcher.componentEnabled());

    SpotlightRuntimePaths paths = SpotlightRuntimePaths::fromEnvironment();
    SpotlightController controller(paths);

    // The fix: sync component enabled state
    controller.setComponentEnabled(watcher.componentEnabled());

    // Should remain closed (was never open)
    QVERIFY(!controller.isOpen());
}

void TestSpotlightBackend::testConfigSync_liveUpdate() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString configPath = tempDir.path() + QStringLiteral("/spotlight.json");
    QString componentsPath = tempDir.path() + QStringLiteral("/components.json");

    {
        QFile f(configPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"weather\": true}");
        f.close();
    }
    {
        QFile f(componentsPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{}");
        f.close();
    }

    SpotlightConfigWatcher watcher(configPath, componentsPath);
    QVERIFY(watcher.weatherEnabled());

    SpotlightRuntimePaths paths = SpotlightRuntimePaths::fromEnvironment();
    SpotlightController controller(paths);

    controller.applyConfig(watcher.spotlightConfig());
    QVERIFY(controller.weatherEnabled());

    // Live update: change config file and refresh
    {
        QFile f(configPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"weather\": false}");
        f.close();
    }

    watcher.refresh();
    QVERIFY(!watcher.weatherEnabled());

    controller.applyConfig(watcher.spotlightConfig());
    QVERIFY(!controller.weatherEnabled());
}

void TestSpotlightBackend::testConfigSync_atomicReplacement() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString configPath = tempDir.path() + QStringLiteral("/spotlight.json");
    QString componentsPath = tempDir.path() + QStringLiteral("/components.json");

    {
        QFile f(configPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"weather\": true}");
        f.close();
    }
    {
        QFile f(componentsPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{}");
        f.close();
    }

    SpotlightConfigWatcher watcher(configPath, componentsPath);
    QVERIFY(watcher.weatherEnabled());

    // Atomic replace: write new file, rename over original
    QString tmpPath = tempDir.path() + QStringLiteral("/spotlight.json.new");
    {
        QFile f(tmpPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"weather\": false}");
        f.close();
    }

    QFile::remove(configPath);
    QVERIFY(QFile::rename(tmpPath, configPath));

    // The watcher must detect the new file
    watcher.refresh();
    QVERIFY(!watcher.weatherEnabled());
}

// ============================================================
// Resolve-icon JSON validity tests
// ============================================================

void TestSpotlightBackend::testResolveIconJson_validJson() {
    auto buildAndValidate = [](const QString &iconName) {
        AstreaIconProvider provider;
        QSize size;
        QPixmap pm = provider.requestPixmap(iconName, &size, QSize(48, 48));
        QJsonObject result;
        result.insert(QStringLiteral("iconName"), iconName);
        result.insert(QStringLiteral("selectedTheme"), QIcon::themeName());
        result.insert(QStringLiteral("fallbackTheme"), QIcon::fallbackThemeName());
        result.insert(QStringLiteral("resolved"), !pm.isNull());
        result.insert(QStringLiteral("requestedSize"), QStringLiteral("48x48"));
        if (!pm.isNull()) {
            result.insert(QStringLiteral("size"),
                          QStringLiteral("%1x%2").arg(size.width()).arg(size.height()));
        }
        QByteArray json = QJsonDocument(result).toJson(QJsonDocument::Indented);
        QJsonParseError err;
        QJsonDocument parsed = QJsonDocument::fromJson(json, &err);
        QVERIFY2(err.error == QJsonParseError::NoError,
                 qPrintable(QStringLiteral("JSON parse error for iconName '%1': %2")
                                .arg(iconName, err.errorString())));
        QVERIFY(parsed.isObject());
        QCOMPARE(parsed.object().value(QStringLiteral("iconName")).toString(), iconName);
    };

    buildAndValidate(QStringLiteral("firefox"));
    buildAndValidate(QStringLiteral("org.example.App"));
    buildAndValidate(QStringLiteral("name with spaces"));
    buildAndValidate(QStringLiteral("quotes: \"app\""));
    buildAndValidate(QStringLiteral("backslash: app\\name"));
    buildAndValidate(QStringLiteral("unicode: Configuração"));
    buildAndValidate(QString());
}

QTEST_MAIN(TestSpotlightBackend)
#include "SpotlightBackendTest.moc"
