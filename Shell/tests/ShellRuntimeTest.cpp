#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "AltTab/core/AltTabController.hpp"
#include "Bar/core/BarController.hpp"
#include "Dock/core/DockController.hpp"
#include "Spotlight/core/SpotlightController.hpp"
#include "launch/ApplicationLauncher.hpp"
#include "runtime/ShellRuntime.hpp"
#include "services/SpatialAudioConfigWatcher.hpp"
#include "theme/ThemeController.hpp"
#include "system/audio/AudioService.hpp"
#include "system/audio/spatial/SpatialAudioEngine.hpp"
#include "system/network/NetworkService.hpp"
#include "system/bluetooth/BluetoothService.hpp"

#include <initializer_list>
#include <utility>

namespace {

class EnvironmentGuard {
public:
    explicit EnvironmentGuard(std::initializer_list<const char *> names)
    {
        for (const char *name : names) {
            const QByteArray key(name);
            m_values.append({key, qEnvironmentVariableIsSet(name), qgetenv(name)});
        }
    }

    ~EnvironmentGuard()
    {
        for (const Value &value : std::as_const(m_values)) {
            if (value.wasSet)
                qputenv(value.name.constData(), value.value);
            else
                qunsetenv(value.name.constData());
        }
    }

private:
    struct Value {
        QByteArray name;
        bool wasSet = false;
        QByteArray value;
    };
    QList<Value> m_values;
};

bool writeExecutable(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size())
        return false;
    file.close();
    return file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                               | QFileDevice::ExeOwner);
}

bool writeAtomically(const QString &path, const QByteArray &contents)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size()
        && file.commit();
}

} // namespace

class ShellRuntimeTest final : public QObject {
    Q_OBJECT

private slots:
    void createsOneSharedOwnershipGraph();
    void barSearchCapabilityTracksSpotlightComposition();
    void systemServicesRemainRestartableAcrossRuntimeLifecycle();
    void spatialAudioConfigurationDefaultsDisabledAndUsesXdgPath();
    void spatialAudioConfigurationWatcherHandlesAtomicReplacementAndSemanticChanges();
    void malformedSpatialAudioConfigurationFailsSafelyDisabled();
    void selectsTyphonShellControlBridgeForTyphonRuntime();
    void sharedLauncherUsesTyphonBridgeAndPreservesHelperEnvironment();
    void launcherRefreshesSystemdWaylandEnvironmentBeforeDispatch();
    void launcherRejectsDispatchWhenSystemdEnvironmentSyncFails();
    void desktopActionExecExpansionIsBoundedAndFieldAware();
    void malformedDesktopActionExecIsRejected();
};

void ShellRuntimeTest::createsOneSharedOwnershipGraph()
{
    EnvironmentGuard environmentGuard({"XDG_CONFIG_HOME"});
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    qputenv("XDG_CONFIG_HOME", configHome.path().toUtf8());

    ShellRuntime runtime;
    QString error;
    QVERIFY2(runtime.initialize(QStringLiteral("auto"), &error), qPrintable(error));

    QVERIFY(runtime.catalog());
    QVERIFY(runtime.identityResolver());
    QVERIFY(runtime.launcher());
    QVERIFY(runtime.typhonSession());
    QVERIFY(runtime.shortcutClient());
    QVERIFY(runtime.windowBackend());
    QVERIFY(runtime.ipcServer());
    QVERIFY(runtime.dockConfig());
    QVERIFY(runtime.altTabConfig());
    QVERIFY(runtime.spotlightConfig());
    QVERIFY(runtime.dockController());
    QVERIFY(runtime.altTabController());
    QVERIFY(runtime.spotlightController());
    QVERIFY(runtime.barController());
    QVERIFY(runtime.barClock());
    QVERIFY(runtime.themeController());
    QVERIFY(runtime.workspaceModel());
    QCOMPARE(runtime.dockController()->catalog(), runtime.catalog());
    QCOMPARE(runtime.altTabController()->identityResolver(), runtime.identityResolver());
    QCOMPARE(runtime.barController()->workspaceModel(), runtime.workspaceModel());
    QVERIFY(runtime.audioService());
    QVERIFY(runtime.spatialAudioEngine());
    QVERIFY(runtime.spatialAudioConfig());
    QVERIFY(!runtime.spatialAudioEngine()->enabled());
    QVERIFY(!runtime.spatialAudioEngine()->isPrepared());
    QVERIFY(runtime.networkService());
    QVERIFY(runtime.bluetoothService());
    QVERIFY(static_cast<QObject *>(runtime.audioService())
            != static_cast<QObject *>(runtime.networkService()));
    QVERIFY(static_cast<QObject *>(runtime.audioService())
            != static_cast<QObject *>(runtime.bluetoothService()));
    QVERIFY(static_cast<QObject *>(runtime.networkService())
            != static_cast<QObject *>(runtime.bluetoothService()));
}

void ShellRuntimeTest::barSearchCapabilityTracksSpotlightComposition()
{
    EnvironmentGuard environmentGuard({"XDG_CONFIG_HOME"});
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    qputenv("XDG_CONFIG_HOME", configHome.path().toUtf8());

    ShellRuntime runtime;
    QString error;
    QVERIFY2(runtime.initialize(QStringLiteral("auto"), &error), qPrintable(error));
    auto *bar = runtime.barController();
    auto *spotlight = runtime.spotlightController();
    QVERIFY(bar);
    QVERIFY(spotlight);

    spotlight->setWeatherEnabled(false);
    QCOMPARE(bar->searchAvailable(), spotlight->componentEnabled());
    QVERIFY(bar->showSearch());
    QVERIFY(spotlight->isOpen());

    spotlight->setComponentEnabled(false);
    QVERIFY(!bar->searchAvailable());
    QVERIFY(!bar->showSearch());
    QVERIFY(!spotlight->isOpen());

    spotlight->setComponentEnabled(true);
    QVERIFY(bar->searchAvailable());
    QVERIFY(bar->showSearch());
    QVERIFY(spotlight->isOpen());
}

void ShellRuntimeTest::systemServicesRemainRestartableAcrossRuntimeLifecycle()
{
    EnvironmentGuard environmentGuard({"XDG_CONFIG_HOME"});
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    qputenv("XDG_CONFIG_HOME", configHome.path().toUtf8());

    ShellRuntime runtime;
    QString error;
    QVERIFY2(runtime.initialize(QStringLiteral("auto"), &error), qPrintable(error));
    auto *audio = runtime.audioService();
    auto *network = runtime.networkService();
    auto *bluetooth = runtime.bluetoothService();
    QVERIFY(audio && network && bluetooth);

    runtime.start();
    runtime.stop();
    runtime.start();
    QCOMPARE(runtime.audioService(), audio);
    QCOMPARE(runtime.networkService(), network);
    QCOMPARE(runtime.bluetoothService(), bluetooth);
    runtime.stop();
}

void ShellRuntimeTest::spatialAudioConfigurationDefaultsDisabledAndUsesXdgPath()
{
    EnvironmentGuard environmentGuard({"XDG_CONFIG_HOME"});
    QTemporaryDir configHome;
    QVERIFY(configHome.isValid());
    qputenv("XDG_CONFIG_HOME", configHome.path().toUtf8());

    const QString expected = QDir(configHome.path()).filePath(
        QStringLiteral("AstreaOS/audio/spatial.json"));
    QCOMPARE(SpatialAudioConfigWatcher::defaultConfigPath(), expected);

    SpatialAudioConfigWatcher watcher(expected);
    QVERIFY(!watcher.enabled());
    QVERIFY(watcher.errorString().isEmpty());

    QString fallbackHome = configHome.path();
    qunsetenv("XDG_CONFIG_HOME");
    QCOMPARE(SpatialAudioConfigWatcher::defaultConfigPath(),
             QDir(QDir::home().filePath(QStringLiteral(".config")))
                 .filePath(QStringLiteral("AstreaOS/audio/spatial.json")));
    qputenv("XDG_CONFIG_HOME", fallbackHome.toUtf8());
}

void ShellRuntimeTest::spatialAudioConfigurationWatcherHandlesAtomicReplacementAndSemanticChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = QDir(directory.path()).filePath(
        QStringLiteral("AstreaOS/audio/spatial.json"));
    SpatialAudioConfigWatcher watcher(path);
    QSignalSpy enabledSpy(&watcher, &SpatialAudioConfigWatcher::enabledChanged);
    QVERIFY(enabledSpy.isValid());
    QVERIFY(!watcher.enabled());

    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QVERIFY(writeAtomically(path, R"({"enabled":true})"));
    QTRY_VERIFY_WITH_TIMEOUT(watcher.enabled(), 3000);
    QCOMPARE(enabledSpy.count(), 1);

    QVERIFY(writeAtomically(path, R"({"enabled":true})"));
    QTest::qWait(350);
    QCOMPARE(enabledSpy.count(), 1);
    QVERIFY(watcher.enabled());

    QVERIFY(writeAtomically(path, R"({"enabled":false})"));
    QTRY_VERIFY_WITH_TIMEOUT(!watcher.enabled(), 3000);
    QCOMPARE(enabledSpy.count(), 2);
}

void ShellRuntimeTest::malformedSpatialAudioConfigurationFailsSafelyDisabled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = QDir(directory.path()).filePath(
        QStringLiteral("AstreaOS/audio/spatial.json"));
    SpatialAudioConfigWatcher watcher(path);
    QVERIFY(!watcher.enabled());

    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QVERIFY(writeAtomically(path, R"({"enabled":true})"));
    QTRY_VERIFY_WITH_TIMEOUT(watcher.enabled(), 3000);

    QVERIFY(writeAtomically(path, QByteArrayLiteral("{")));
    QTRY_VERIFY_WITH_TIMEOUT(!watcher.errorString().isEmpty(), 3000);
    QVERIFY(!watcher.enabled());

    QVERIFY(writeAtomically(path, QByteArrayLiteral("{}")));
    QTRY_VERIFY_WITH_TIMEOUT(watcher.errorString().isEmpty(), 3000);
    QVERIFY(!watcher.enabled());
}

void ShellRuntimeTest::selectsTyphonShellControlBridgeForTyphonRuntime()
{
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("ASTREA_COMPOSITOR"), QStringLiteral("TYPHON"));
    environment.insert(QStringLiteral("ASTREA_SHELL_CONTROL_BRIDGE"),
                       QStringLiteral("/tmp/astrea-shell-control-bridge"));

    QCOMPARE(ApplicationLauncher::resolveLauncherPath(QStringLiteral("/tmp/astrea-launch"),
                                                      environment),
             QStringLiteral("/tmp/astrea-shell-control-bridge"));

    environment.remove(QStringLiteral("ASTREA_COMPOSITOR"));
    environment.insert(QStringLiteral("ASTREA_COMPOSITOR_BACKEND"), QStringLiteral("typhon"));
    QCOMPARE(ApplicationLauncher::resolveLauncherPath(QStringLiteral("/tmp/astrea-launch"),
                                                      environment),
             QStringLiteral("/tmp/astrea-shell-control-bridge"));

    environment.insert(QStringLiteral("ASTREA_COMPOSITOR_BACKEND"), QStringLiteral("hyprland"));
    QCOMPARE(ApplicationLauncher::resolveLauncherPath(QStringLiteral("/tmp/astrea-launch"),
                                                      environment),
             QStringLiteral("/tmp/astrea-launch"));

    environment.insert(QStringLiteral("ASTREA_COMPOSITOR_BACKEND"), QStringLiteral("typhon"));
    environment.remove(QStringLiteral("ASTREA_SHELL_CONTROL_BRIDGE"));
    QCOMPARE(ApplicationLauncher::resolveLauncherPath(QStringLiteral("/tmp/astrea-launch"),
                                                      environment),
             QStringLiteral("/tmp/astrea-launch"));
}

void ShellRuntimeTest::sharedLauncherUsesTyphonBridgeAndPreservesHelperEnvironment()
{
    EnvironmentGuard guard({"ASTREA_COMPOSITOR", "ASTREA_COMPOSITOR_BACKEND",
                            "ASTREA_SHELL_CONTROL_BRIDGE", "ASTREA_TEST_OUTPUT",
                            "ASTREA_TEST_VALUE", "WAYLAND_DISPLAY", "DISPLAY", "XAUTHORITY",
                            "OBLIVION_ONE_XWAYLAND_DISPLAY", "PATH", "HOME", "USER",
                            "XDG_RUNTIME_DIR", "XDG_SESSION_TYPE", "DBUS_SESSION_BUS_ADDRESS"});
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fallback = directory.filePath(QStringLiteral("astrea-launch"));
    const QString bridge = directory.filePath(QStringLiteral("bridge with spaces"));
    const QString outputPath = directory.filePath(QStringLiteral("observed environment"));
    QVERIFY(writeExecutable(fallback, QByteArrayLiteral("#!/bin/sh\nexit 23\n")));
    QVERIFY(writeExecutable(
        bridge,
        QByteArrayLiteral(
            "#!/bin/sh\n"
            "{\n"
            "  printf 'arg1=%s\\n' \"$1\"\n"
            "  printf 'arg2=%s\\n' \"$2\"\n"
            "  printf 'WAYLAND_DISPLAY=%s\\n' \"$WAYLAND_DISPLAY\"\n"
            "  printf 'DISPLAY=%s\\n' \"$DISPLAY\"\n"
            "  printf 'XAUTHORITY=%s\\n' \"$XAUTHORITY\"\n"
            "  printf 'OBLIVION_ONE_XWAYLAND_DISPLAY=%s\\n' \"$OBLIVION_ONE_XWAYLAND_DISPLAY\"\n"
            "  printf 'ASTREA_TEST_VALUE=%s\\n' \"$ASTREA_TEST_VALUE\"\n"
            "  printf 'PATH=%s\\n' \"$PATH\"\n"
            "  printf 'HOME=%s\\n' \"$HOME\"\n"
            "  printf 'USER=%s\\n' \"$USER\"\n"
            "  printf 'XDG_RUNTIME_DIR=%s\\n' \"$XDG_RUNTIME_DIR\"\n"
            "  printf 'XDG_SESSION_TYPE=%s\\n' \"$XDG_SESSION_TYPE\"\n"
            "  printf 'DBUS_SESSION_BUS_ADDRESS=%s\\n' \"$DBUS_SESSION_BUS_ADDRESS\"\n"
            "} > \"$ASTREA_TEST_OUTPUT\"\n")));

    qunsetenv("ASTREA_COMPOSITOR");
    qputenv("ASTREA_COMPOSITOR_BACKEND", "typhon");
    qputenv("ASTREA_SHELL_CONTROL_BRIDGE", bridge.toUtf8());
    qputenv("ASTREA_TEST_OUTPUT", outputPath.toUtf8());
    qputenv("ASTREA_TEST_VALUE", "path with spaces;[]=$value");
    qputenv("WAYLAND_DISPLAY", "oblivion-one-test");
    qputenv("DISPLAY", ":42");
    qputenv("XAUTHORITY", "/run/user/1000/typhon/xwayland/authority with spaces");
    qputenv("OBLIVION_ONE_XWAYLAND_DISPLAY", ":42");
    qputenv("PATH", "/opt/app tools/bin:/usr/bin");
    qputenv("HOME", "/home/test user");
    qputenv("USER", "test-user");
    qputenv("XDG_RUNTIME_DIR", "/run/user/1000 path");
    qputenv("XDG_SESSION_TYPE", "wayland");
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/run/user/1000/bus;guid=special value");

    ApplicationLauncher launcher(fallback);
    QSignalSpy succeeded(&launcher, &ApplicationLauncher::launchSucceeded);
    QSignalSpy failed(&launcher, &ApplicationLauncher::launchFailed);
    launcher.launchDesktop(QStringLiteral("org.example.App"),
                           QStringLiteral("org.example.App.desktop"), QString());

    QTRY_COMPARE_WITH_TIMEOUT(succeeded.count(), 1, 2000);
    QCOMPARE(failed.count(), 0);
    QFile output(outputPath);
    QVERIFY(output.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(output.readAll()),
             QStringLiteral("arg1=--desktop\n"
                            "arg2=org.example.App.desktop\n"
                            "WAYLAND_DISPLAY=oblivion-one-test\n"
                            "DISPLAY=:42\n"
                            "XAUTHORITY=/run/user/1000/typhon/xwayland/authority with spaces\n"
                            "OBLIVION_ONE_XWAYLAND_DISPLAY=:42\n"
                            "ASTREA_TEST_VALUE=path with spaces;[]=$value\n"
                            "PATH=/opt/app tools/bin:/usr/bin\n"
                            "HOME=/home/test user\n"
                            "USER=test-user\n"
                            "XDG_RUNTIME_DIR=/run/user/1000 path\n"
                            "XDG_SESSION_TYPE=wayland\n"
                            "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus;guid=special value\n"));
}

void ShellRuntimeTest::launcherRefreshesSystemdWaylandEnvironmentBeforeDispatch()
{
    EnvironmentGuard guard({"ASTREA_COMPOSITOR", "ASTREA_COMPOSITOR_BACKEND",
                            "ASTREA_SHELL_CONTROL_BRIDGE", "ASTREA_TEST_ARGS",
                            "ASTREA_TEST_ORDER", "PATH", "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR",
                            "DISPLAY"});
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString updater = directory.filePath(QStringLiteral("dbus-update-activation-environment"));
    const QString launcher = directory.filePath(QStringLiteral("astrea-launch"));
    const QString argsPath = directory.filePath(QStringLiteral("activation args"));
    const QString orderPath = directory.filePath(QStringLiteral("launch order"));
    QVERIFY(writeExecutable(
        updater,
        QByteArrayLiteral("#!/bin/sh\n"
                         "printf '%s\\n' \"$@\" > \"$ASTREA_TEST_ARGS\"\n"
                         "printf 'sync\\n' >> \"$ASTREA_TEST_ORDER\"\n")));
    QVERIFY(writeExecutable(
        launcher,
        QByteArrayLiteral("#!/bin/sh\n"
                         "printf 'launch\\n' >> \"$ASTREA_TEST_ORDER\"\n")));

    qunsetenv("ASTREA_COMPOSITOR");
    qunsetenv("ASTREA_COMPOSITOR_BACKEND");
    qunsetenv("ASTREA_SHELL_CONTROL_BRIDGE");
    qputenv("ASTREA_TEST_ARGS", argsPath.toUtf8());
    qputenv("ASTREA_TEST_ORDER", orderPath.toUtf8());
    qputenv("PATH", directory.path().toUtf8());
    qputenv("WAYLAND_DISPLAY", "wayland-before-launch");
    qputenv("XDG_RUNTIME_DIR", "/run/user/before-launch");
    qputenv("DISPLAY", ":42");

    ApplicationLauncher appLauncher(launcher);
    qputenv("WAYLAND_DISPLAY", "wayland-current");
    qputenv("XDG_RUNTIME_DIR", "/run/user/1000");
    QSignalSpy succeeded(&appLauncher, &ApplicationLauncher::launchSucceeded);
    QSignalSpy failed(&appLauncher, &ApplicationLauncher::launchFailed);
    QSignalSpy completed(&appLauncher, &ApplicationLauncher::launchCompleted);
    appLauncher.launchDesktop(QStringLiteral("org.example.App"),
                              QStringLiteral("org.example.App.desktop"), QString());

    QTRY_COMPARE_WITH_TIMEOUT(succeeded.count(), 1, 2000);
    QCOMPARE(failed.count(), 0);
    QCOMPARE(completed.count(), 1);
    QCOMPARE(completed.at(0).at(1).toBool(), true);

    QFile argsFile(argsPath);
    QVERIFY(argsFile.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(argsFile.readAll()),
             QStringLiteral("--systemd\n"
                            "WAYLAND_DISPLAY=wayland-current\n"
                            "XDG_RUNTIME_DIR=/run/user/1000\n"));

    QFile orderFile(orderPath);
    QVERIFY(orderFile.open(QIODevice::ReadOnly));
    QCOMPARE(QString::fromUtf8(orderFile.readAll()), QStringLiteral("sync\nlaunch\n"));
}

void ShellRuntimeTest::launcherRejectsDispatchWhenSystemdEnvironmentSyncFails()
{
    EnvironmentGuard guard({"ASTREA_COMPOSITOR", "ASTREA_COMPOSITOR_BACKEND",
                            "ASTREA_SHELL_CONTROL_BRIDGE", "ASTREA_TEST_MARKER", "PATH",
                            "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR"});
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString updater = directory.filePath(QStringLiteral("dbus-update-activation-environment"));
    const QString launcher = directory.filePath(QStringLiteral("astrea-launch"));
    const QString markerPath = directory.filePath(QStringLiteral("launch marker"));
    QVERIFY(writeExecutable(
        updater,
        QByteArrayLiteral("#!/bin/sh\n"
                         "printf 'activation sync failed\\n' >&2\n"
                         "exit 7\n")));
    QVERIFY(writeExecutable(
        launcher,
        QByteArrayLiteral("#!/bin/sh\n"
                         ": > \"$ASTREA_TEST_MARKER\"\n")));

    qunsetenv("ASTREA_COMPOSITOR");
    qunsetenv("ASTREA_COMPOSITOR_BACKEND");
    qunsetenv("ASTREA_SHELL_CONTROL_BRIDGE");
    qputenv("ASTREA_TEST_MARKER", markerPath.toUtf8());
    qputenv("PATH", directory.path().toUtf8());
    qputenv("WAYLAND_DISPLAY", "wayland-current");
    qputenv("XDG_RUNTIME_DIR", "/run/user/1000");

    ApplicationLauncher appLauncher(launcher);
    QSignalSpy succeeded(&appLauncher, &ApplicationLauncher::launchSucceeded);
    QSignalSpy failed(&appLauncher, &ApplicationLauncher::launchFailed);
    QSignalSpy completed(&appLauncher, &ApplicationLauncher::launchCompleted);
    appLauncher.launchDesktop(QStringLiteral("org.example.App"),
                              QStringLiteral("org.example.App.desktop"), QString());

    QCOMPARE(succeeded.count(), 0);
    QCOMPARE(failed.count(), 1);
    QVERIFY(failed.at(0).at(1).toString().contains(QStringLiteral("activation sync failed")));
    QCOMPARE(completed.count(), 1);
    QCOMPARE(completed.at(0).at(1).toBool(), false);
    QVERIFY(!QFileInfo::exists(markerPath));
}

void ShellRuntimeTest::desktopActionExecExpansionIsBoundedAndFieldAware()
{
    ApplicationLaunchRequest request;
    request.exec = QStringLiteral("action \"file name\" %c %i %%");
    request.appName = QStringLiteral("Example");
    request.iconName = QStringLiteral("example-icon");

    QString error;
    QCOMPARE(ApplicationLauncher::expandDesktopExec(request, &error),
             QStringList({QStringLiteral("action"), QStringLiteral("file name"),
                          QStringLiteral("Example"), QStringLiteral("--icon"),
                          QStringLiteral("example-icon"), QStringLiteral("%")}));
    QVERIFY(error.isEmpty());
}

void ShellRuntimeTest::malformedDesktopActionExecIsRejected()
{
    ApplicationLaunchRequest request;
    request.exec = QStringLiteral("action \"unterminated");
    QString error;
    QVERIFY(ApplicationLauncher::expandDesktopExec(request, &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(ShellRuntimeTest)
#include "ShellRuntimeTest.moc"
