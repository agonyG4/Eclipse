#define ASTREA_TYPHON_PROTOCOL_INTEGRATION_NO_MAIN
#include "../../shared/tests/TyphonProtocolIntegrationTest.cpp"
#undef ASTREA_TYPHON_PROTOCOL_INTEGRATION_NO_MAIN

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "Dock/core/DockController.hpp"
#include "AltTab/core/AltTabController.hpp"
#include "Spotlight/core/SpotlightController.hpp"
#include "runtime/ShellRuntime.hpp"

namespace {

bool writeTextFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    return file.write(contents) == contents.size();
}

bool waitForSnapshot(FakeTyphonCompositor &compositor, ShellRuntime &runtime)
{
    return compositor.pumpUntil([&runtime] {
        return runtime.typhonToplevelConnection()->hasSnapshot()
            && runtime.dockController()->runtimeKnown();
    });
}

void publishUnifiedWindow(FakeTyphonCompositor &compositor, const QString &id,
                          quint64 revision)
{
    auto *window = compositor.beginWindow(id);
    QVERIFY(window);
    compositor.pumpServerEventsForTest();
    compositor.pumpUntil([] { return false; }, 10);
    compositor.sendMetadata(window, QStringLiteral("unified"), QStringLiteral("Unified Window"),
                            4242, ASTREA_TOPLEVEL_V1_KIND_XDG_TOPLEVEL, 0, 100);
    compositor.pumpServerEventsForTest();
    compositor.pumpUntil([] { return false; }, 10);
    compositor.sendDone(window, revision);
    compositor.sendManagerDone(revision, 1);
}

} // namespace

class ShellUnifiedRuntimeIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void unifiedRuntimeLifecycleAndStress();
};

void ShellUnifiedRuntimeIntegrationTest::unifiedRuntimeLifecycleAndStress()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString applications = home.path() + QStringLiteral("/.local/share/applications");
    const QString config = home.path() + QStringLiteral("/.config/AstreaOS");
    QVERIFY(QDir().mkpath(applications));
    QVERIFY(QDir().mkpath(config + QStringLiteral("/ui")));
    QVERIFY(writeTextFile(applications + QStringLiteral("/unified.desktop"),
                          "[Desktop Entry]\nType=Application\nName=Unified\n"
                          "Icon=unified\nExec=unified\nStartupWMClass=unified\n"));
    QVERIFY(writeTextFile(config + QStringLiteral("/dock.json"),
                          "{\"pins\":[\"unified.desktop\"]}\n"));
    QVERIFY(writeTextFile(config + QStringLiteral("/ui/components.json"),
                          "{\"dock\":true,\"alttab\":true,\"spotlight\":true}\n"));
    QVERIFY(writeTextFile(config + QStringLiteral("/spotlight.json"),
                          "{\"weather\":false}\n"));

    const QByteArray previousHome = qgetenv("HOME");
    qputenv("HOME", home.path().toUtf8());

    FakeTyphonCompositor compositor(true, 2, true, false, true);
    ShellRuntime runtime;
    QString error;
    QVERIFY2(runtime.initialize(QStringLiteral("auto"), &error), qPrintable(error));
    QCOMPARE(runtime.dockController()->catalog(), runtime.catalog());
    QCOMPARE(runtime.altTabController()->identityResolver(), runtime.identityResolver());
    QCOMPARE(runtime.spotlightController()->catalog(), runtime.catalog());
    QCOMPARE(runtime.shortcutClient()->registeredShortcutCount(), 0);
    runtime.start();

    QVERIFY(compositor.pumpUntil([&runtime] {
        return runtime.shortcutClient()->isReady()
            && runtime.typhonToplevelConnection()->state()
                == TyphonConnectionState::WaitingForInitialSnapshot;
    }));
    QCOMPARE(compositor.authenticationCount(), 1);
    QCOMPARE(compositor.clientCount(), 1);
    QCOMPARE(runtime.shortcutClient()->registeredShortcutCount(), 4);
    publishUnifiedWindow(compositor, QStringLiteral("1"), 1);
    QVERIFY(waitForSnapshot(compositor, runtime));
    QCOMPARE(runtime.typhonToplevelConnection()->snapshot().windows.size(), 1);
    QCOMPARE(runtime.dockController()->appModel()->rowCount(), 1);
    QVERIFY(runtime.dockController()->runtimeKnown());
    QVERIFY(runtime.dockController()->appModel()->data(
                 runtime.dockController()->appModel()->index(0, 0),
                 DockAppModel::RunningRole).toBool());

    runtime.dockController()->launch(0);
    QVERIFY(compositor.pumpUntil([&compositor] { return compositor.actionRequests().size() >= 1; }));
    QCOMPARE(compositor.actionRequests().at(0).window->id, QStringLiteral("1"));
    compositor.sendActionDone(compositor.actionRequests().at(0),
                              ASTREA_TOPLEVEL_MANAGER_V1_ACTION_RESULT_ACCEPTED);

    runtime.altTabController()->show();
    QVERIFY(compositor.pumpUntil([&runtime] {
        return runtime.altTabController()->state() == AltTabController::State::Open;
    }));
    QCOMPARE(runtime.altTabController()->selectedIndex(), 0);
    runtime.altTabController()->commit();
    QVERIFY(compositor.pumpUntil([&compositor] { return compositor.actionRequests().size() >= 2; }));
    QCOMPARE(compositor.actionRequests().at(1).window->id, QStringLiteral("1"));
    compositor.sendActionDone(compositor.actionRequests().at(1),
                              ASTREA_TOPLEVEL_MANAGER_V1_ACTION_RESULT_ACCEPTED);
    QVERIFY(compositor.pumpUntil([&runtime] {
        return runtime.altTabController()->state() == AltTabController::State::Hidden;
    }));

    QSignalSpy spotlightOpenSpy(runtime.spotlightController(),
                                &SpotlightController::openChanged);
    compositor.sendShortcutPressed(QStringLiteral("spotlight_toggle"), 1, 1);
    QVERIFY(compositor.pumpUntil([&runtime] { return runtime.spotlightController()->isOpen(); }));
    QCOMPARE(spotlightOpenSpy.count(), 1);
    compositor.sendShortcutPressed(QStringLiteral("spotlight_toggle"), 2, 2);
    QVERIFY(compositor.pumpUntil([&runtime] { return !runtime.spotlightController()->isOpen(); }));
    QCOMPARE(spotlightOpenSpy.count(), 2);

    for (int cycle = 0; cycle < 100; ++cycle) {
        runtime.altTabController()->show();
        QVERIFY(compositor.pumpUntil([&runtime] {
            return runtime.altTabController()->state() == AltTabController::State::Open;
        }));
        runtime.altTabController()->cancel();
        QVERIFY(compositor.pumpUntil([&runtime] {
            return runtime.altTabController()->state() == AltTabController::State::Hidden
                && runtime.altTabController()->selectedIndex() == -1;
        }));

        runtime.spotlightController()->toggle();
        QVERIFY(runtime.spotlightController()->isOpen());
        runtime.spotlightController()->close();
        QVERIFY(!runtime.spotlightController()->isOpen());
    }
    QCOMPARE(runtime.shortcutClient()->registeredShortcutCount(), 4);
    QCOMPARE(runtime.typhonSession()->connectionGeneration(), quint64(1));

    compositor.disconnectClients();
    QVERIFY(compositor.pumpUntil([&runtime] {
        return runtime.typhonSession()->state() == TyphonSharedConnection::State::Degraded
            || runtime.typhonSession()->state() == TyphonSharedConnection::State::Disconnected;
    }));
    QVERIFY(!runtime.dockController()->runtimeKnown());
    QVERIFY(runtime.shortcutClient()->registeredShortcutCount() == 0);

    runtime.typhonSession()->reconnectNowForTest();
    QVERIFY(compositor.pumpUntil([&runtime] {
        return runtime.shortcutClient()->isReady()
            && runtime.typhonToplevelConnection()->state()
                == TyphonConnectionState::WaitingForInitialSnapshot;
    }));
    QCOMPARE(runtime.typhonSession()->connectionGeneration(), quint64(2));
    QCOMPARE(compositor.authenticationCount(), 2);
    QCOMPARE(compositor.clientCount(), 1);
    QCOMPARE(runtime.shortcutClient()->registeredShortcutCount(), 4);
    publishUnifiedWindow(compositor, QStringLiteral("2"), 2);
    QVERIFY(waitForSnapshot(compositor, runtime));
    QCOMPARE(runtime.typhonToplevelConnection()->snapshot().windows.first().id,
             QStringLiteral("2"));

    compositor.sendShortcutPressed(QStringLiteral("spotlight_toggle"), 3, 3);
    QVERIFY(compositor.pumpUntil([&runtime] { return runtime.spotlightController()->isOpen(); }));
    runtime.stop();
    QCOMPARE(runtime.shortcutClient()->state(), TyphonShortcutConnectionState::Stopped);
    QCOMPARE(runtime.typhonSession()->state(), TyphonSharedConnection::State::Stopped);
    QVERIFY(!runtime.spotlightController()->isOpen());

    if (previousHome.isNull())
        qunsetenv("HOME");
    else
        qputenv("HOME", previousHome);
}

QTEST_GUILESS_MAIN(ShellUnifiedRuntimeIntegrationTest)
#include "ShellUnifiedRuntimeIntegrationTest.moc"
