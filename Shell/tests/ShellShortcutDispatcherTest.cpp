#include <QSignalSpy>
#include <QTest>

#include "AltTab/core/AltTabController.hpp"
#include "AltTab/core/WindowInfo.hpp"
#include "AltTab/platform/compositor/FakeBackend.hpp"
#include "apps/appidentity/AppIdentityResolver.hpp"
#include "Spotlight/core/SpotlightController.hpp"
#include "Spotlight/platform/runtime/SpotlightRuntimePaths.hpp"
#include "platform/shortcut/ShellShortcutDispatcher.hpp"

class ShellShortcutDispatcherTest final : public QObject {
    Q_OBJECT

private slots:
    void spotlightToggleIsIndependentFromAltTabGate();
    void altTabToggleIsIndependentFromSpotlightGate();
    void screenshotActionsAreShellWideAndPressOnly();
};

void ShellShortcutDispatcherTest::spotlightToggleIsIndependentFromAltTabGate()
{
    SpotlightController spotlight(SpotlightRuntimePaths::fromEnvironment());
    spotlight.setWeatherEnabled(false);
    ShellShortcutDispatcher dispatcher(nullptr, &spotlight);
    dispatcher.setAltTabEnabled(false);
    dispatcher.setSpotlightEnabled(true);
    QSignalSpy openSpy(&spotlight, &SpotlightController::openChanged);

    dispatcher.dispatch(QStringLiteral("astrea-shell"), QStringLiteral("spotlight_toggle"),
                         TyphonShortcutPhase::Pressed);
    QCOMPARE(openSpy.count(), 1);
    QVERIFY(spotlight.isOpen());

    dispatcher.dispatch(QStringLiteral("astrea-shell"), QStringLiteral("spotlight_toggle"),
                         TyphonShortcutPhase::Repeated);
    QCOMPARE(openSpy.count(), 1);

    dispatcher.setSpotlightEnabled(false);
    dispatcher.dispatch(QStringLiteral("astrea-shell"), QStringLiteral("spotlight_toggle"),
                         TyphonShortcutPhase::Pressed);
    QCOMPARE(openSpy.count(), 1);
}

void ShellShortcutDispatcherTest::altTabToggleIsIndependentFromSpotlightGate()
{
    WindowInfo window;
    window.windowId = WindowId{QStringLiteral("window-1")};
    window.displayName = QStringLiteral("Window 1");
    window.workspaceId = WorkspaceId{QStringLiteral("1")};
    FakeWindowSource backend({window});
    AppIdentityResolver identity;
    AltTabController altTab(&backend, &identity);
    SpotlightController spotlight(SpotlightRuntimePaths::fromEnvironment());
    spotlight.setWeatherEnabled(false);
    ShellShortcutDispatcher dispatcher(&altTab, &spotlight);
    dispatcher.setAltTabEnabled(true);
    dispatcher.setSpotlightEnabled(false);
    QSignalSpy openSpy(&altTab, &AltTabController::openChanged);

    dispatcher.dispatch(QStringLiteral("astrea-shell"), QStringLiteral("alt_tab_next"),
                         TyphonShortcutPhase::Pressed);
    QVERIFY(altTab.isOpen());
    QCOMPARE(openSpy.count(), 1);
    QVERIFY(!spotlight.isOpen());

    dispatcher.dispatch(QStringLiteral("astrea-shell"), QStringLiteral("spotlight_toggle"),
                         TyphonShortcutPhase::Pressed);
    QVERIFY(!spotlight.isOpen());
}

void ShellShortcutDispatcherTest::screenshotActionsAreShellWideAndPressOnly()
{
    const QList<QPair<QString, ShellShortcutAction>> actions = {
        {QStringLiteral("screenshot_quick"), ShellShortcutAction::ScreenshotQuick},
        {QStringLiteral("screenshot_region_frozen"), ShellShortcutAction::ScreenshotRegionFrozen},
        {QStringLiteral("screenshot_region_live"), ShellShortcutAction::ScreenshotRegionLive},
    };
    for (const auto &[name, action] : actions) {
        QCOMPARE(mapTyphonShellShortcut(QStringLiteral("astrea-shell"), name,
                                         TyphonShortcutPhase::Pressed), action);
        QCOMPARE(mapTyphonShellShortcut(QStringLiteral("astrea-shell"), name,
                                         TyphonShortcutPhase::Repeated),
                 ShellShortcutAction::Ignore);
        QCOMPARE(mapTyphonShellShortcut(QStringLiteral("astrea-shell"), name,
                                         TyphonShortcutPhase::Released),
                 ShellShortcutAction::Ignore);
        QCOMPARE(mapTyphonShellShortcut(QStringLiteral("other"), name,
                                         TyphonShortcutPhase::Pressed),
                 ShellShortcutAction::Ignore);
    }
}

QTEST_GUILESS_MAIN(ShellShortcutDispatcherTest)
#include "ShellShortcutDispatcherTest.moc"
