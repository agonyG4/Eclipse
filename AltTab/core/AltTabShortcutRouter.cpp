#include "core/AltTabShortcutRouter.hpp"

AltTabShortcutAction mapAltTabShortcut(const QString &namespaceName, const QString &name,
                                       AltTabShortcutPhase phase)
{
    if (namespaceName != QStringLiteral("astrea-shell")
        || phase == AltTabShortcutPhase::Released || phase == AltTabShortcutPhase::Cancelled) {
        return AltTabShortcutAction::Ignore;
    }

    if (name == QStringLiteral("alt_tab_next")
        && (phase == AltTabShortcutPhase::Pressed || phase == AltTabShortcutPhase::Repeated)) {
        return AltTabShortcutAction::Next;
    }
    if (name == QStringLiteral("alt_tab_previous")
        && (phase == AltTabShortcutPhase::Pressed || phase == AltTabShortcutPhase::Repeated)) {
        return AltTabShortcutAction::Previous;
    }
    if (name == QStringLiteral("alt_tab_commit") && phase == AltTabShortcutPhase::Pressed)
        return AltTabShortcutAction::Commit;
    return AltTabShortcutAction::Ignore;
}
