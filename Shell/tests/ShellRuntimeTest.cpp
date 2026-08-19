#include <QCoreApplication>
#include <QFile>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "AltTab/core/AltTabController.hpp"
#include "Bar/core/BarController.hpp"
#include "Dock/core/DockController.hpp"
#include "Spotlight/core/SpotlightController.hpp"
#include "launch/ApplicationLauncher.hpp"
#include "runtime/ShellRuntime.hpp"
#include "theme/ThemeController.hpp"

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

} // namespace

class ShellRuntimeTest final : public QObject {
    Q_OBJECT

private slots:
    void createsOneSharedOwnershipGraph();
    void selectsTyphonShellControlBridgeForTyphonRuntime();
    void sharedLauncherUsesTyphonBridgeAndPreservesHelperEnvironment();
};

void ShellRuntimeTest::createsOneSharedOwnershipGraph()
{
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
    QCOMPARE(runtime.spotlightController()->catalog(), runtime.catalog());
    QCOMPARE(runtime.spotlightController()->launcher(), runtime.launcher());
    QCOMPARE(runtime.altTabController()->identityResolver(), runtime.identityResolver());
    QCOMPARE(runtime.barController()->workspaceModel(), runtime.workspaceModel());
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

QTEST_GUILESS_MAIN(ShellRuntimeTest)
#include "ShellRuntimeTest.moc"
