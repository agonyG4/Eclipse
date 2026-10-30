#include "platform/shortcut/ShellShortcutDispatcher.hpp"

#include "AltTab/core/AltTabController.hpp"
#include "AltTab/platform/typhon/AltTabShortcutAdapter.hpp"
#include "screenshot/ScreenshotController.hpp"
#include "Spotlight/core/SpotlightController.hpp"

#include <QDebug>

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
    const auto altTabAction = mapTyphonShortcut(namespaceName, name, phase);
    if (altTabAction == AltTabShortcutAction::Next)
        return ShellShortcutAction::AltTabNext;
    if (altTabAction == AltTabShortcutAction::Previous)
        return ShellShortcutAction::AltTabPrevious;
    if (altTabAction == AltTabShortcutAction::Commit)
        return ShellShortcutAction::AltTabCommit;
    if (namespaceName != QStringLiteral("astrea-shell")
        || phase == TyphonShortcutPhase::Released || phase == TyphonShortcutPhase::Cancelled)
        return ShellShortcutAction::Ignore;
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
    const ShellShortcutAction action = mapTyphonShellShortcut(namespaceName, name, phase);
    switch (action) {
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
    case ShellShortcutAction::ScreenshotRegionFrozen:
    case ShellShortcutAction::ScreenshotRegionLive: {
        QString shortcutName;
        bool accepted = false;
        if (action == ShellShortcutAction::ScreenshotQuick) {
            shortcutName = QStringLiteral("screenshot_quick");
            if (m_screenshotController)
                accepted = m_screenshotController->startQuickCapture();
        } else if (action == ShellShortcutAction::ScreenshotRegionFrozen) {
            shortcutName = QStringLiteral("screenshot_region_frozen");
            if (m_screenshotController)
                accepted = m_screenshotController->startFrozenRegionCapture();
        } else {
            shortcutName = QStringLiteral("screenshot_region_live");
            if (m_screenshotController)
                accepted = m_screenshotController->startLiveRegionCapture();
        }
        qInfo().noquote() << "Screenshot shortcut received:" << shortcutName;
        if (accepted)
            qInfo().noquote() << "Screenshot shortcut accepted:" << shortcutName;
        else
            qWarning().noquote() << "Screenshot shortcut rejected:" << shortcutName;
        break;
    }
    case ShellShortcutAction::Ignore:
        break;
    }
}
