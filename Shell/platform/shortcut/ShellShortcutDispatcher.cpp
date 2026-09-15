#include "platform/shortcut/ShellShortcutDispatcher.hpp"

#include "AltTab/core/AltTabController.hpp"
#include "screenshot/ScreenshotController.hpp"
#include "Spotlight/core/SpotlightController.hpp"

ShellShortcutDispatcher::ShellShortcutDispatcher(AltTabController *altTabController,
                                                 SpotlightController *spotlightController,
                                                 ScreenshotController *screenshotController)
    : m_altTabController(altTabController), m_spotlightController(spotlightController),
      m_screenshotController(screenshotController)
{
}

ShellShortcutAction mapTyphonShellShortcut(const QString &namespaceName, const QString &name,
                                           TyphonShortcutPhase phase)
{
    if (namespaceName != QStringLiteral("astrea-shell")
        || phase == TyphonShortcutPhase::Released || phase == TyphonShortcutPhase::Cancelled)
        return ShellShortcutAction::Ignore;
    if (name == QStringLiteral("alt_tab_next")
        && (phase == TyphonShortcutPhase::Pressed || phase == TyphonShortcutPhase::Repeated))
        return ShellShortcutAction::AltTabNext;
    if (name == QStringLiteral("alt_tab_previous")
        && (phase == TyphonShortcutPhase::Pressed || phase == TyphonShortcutPhase::Repeated))
        return ShellShortcutAction::AltTabPrevious;
    if (name == QStringLiteral("alt_tab_commit") && phase == TyphonShortcutPhase::Pressed)
        return ShellShortcutAction::AltTabCommit;
    if (name == QStringLiteral("spotlight_toggle") && phase == TyphonShortcutPhase::Pressed)
        return ShellShortcutAction::SpotlightToggle;
    if (name == QStringLiteral("screenshot_quick") && phase == TyphonShortcutPhase::Pressed)
        return ShellShortcutAction::ScreenshotQuick;
    if (name == QStringLiteral("screenshot_region_frozen")
        && phase == TyphonShortcutPhase::Pressed)
        return ShellShortcutAction::ScreenshotRegionFrozen;
    if (name == QStringLiteral("screenshot_region_live")
        && phase == TyphonShortcutPhase::Pressed)
        return ShellShortcutAction::ScreenshotRegionLive;
    return ShellShortcutAction::Ignore;
}

void ShellShortcutDispatcher::dispatch(const QString &namespaceName, const QString &name,
                                       TyphonShortcutPhase phase)
{
    switch (mapTyphonShellShortcut(namespaceName, name, phase)) {
    case ShellShortcutAction::AltTabNext:
        if (m_altTabEnabled && m_altTabController)
            m_altTabController->step(1);
        break;
    case ShellShortcutAction::AltTabPrevious:
        if (m_altTabEnabled && m_altTabController)
            m_altTabController->step(-1);
        break;
    case ShellShortcutAction::AltTabCommit:
        if (m_altTabEnabled && m_altTabController)
            m_altTabController->commit();
        break;
    case ShellShortcutAction::SpotlightToggle:
        if (m_spotlightEnabled && m_spotlightController)
            m_spotlightController->toggle();
        break;
    case ShellShortcutAction::ScreenshotQuick:
        if (m_screenshotController)
            m_screenshotController->startQuickCapture();
        break;
    case ShellShortcutAction::ScreenshotRegionFrozen:
        if (m_screenshotController)
            m_screenshotController->startFrozenRegionCapture();
        break;
    case ShellShortcutAction::ScreenshotRegionLive:
        if (m_screenshotController)
            m_screenshotController->startLiveRegionCapture();
        break;
    case ShellShortcutAction::Ignore:
        break;
    }
}
