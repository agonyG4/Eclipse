#include "platform/shortcut/ShellShortcutDispatcher.hpp"

#include "AltTab/core/AltTabController.hpp"
#include "Spotlight/core/SpotlightController.hpp"

ShellShortcutDispatcher::ShellShortcutDispatcher(AltTabController *altTabController,
                                                 SpotlightController *spotlightController)
    : m_altTabController(altTabController), m_spotlightController(spotlightController)
{
}

void ShellShortcutDispatcher::dispatch(const QString &namespaceName, const QString &name,
                                       TyphonShortcutPhase phase)
{
    switch (mapTyphonShortcut(namespaceName, name, phase)) {
    case AltTabShortcutAction::Next:
        if (m_altTabEnabled && m_altTabController)
            m_altTabController->step(1);
        break;
    case AltTabShortcutAction::Previous:
        if (m_altTabEnabled && m_altTabController)
            m_altTabController->step(-1);
        break;
    case AltTabShortcutAction::Commit:
        if (m_altTabEnabled && m_altTabController)
            m_altTabController->commit();
        break;
    case AltTabShortcutAction::SpotlightToggle:
        if (m_spotlightEnabled && m_spotlightController)
            m_spotlightController->toggle();
        break;
    case AltTabShortcutAction::Ignore:
        break;
    }
}
