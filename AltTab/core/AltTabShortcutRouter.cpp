#include "core/AltTabShortcutRouter.hpp"

AltTabShortcutAction mapTyphonShortcut(const QString &namespaceName, const QString &name,
                                       TyphonShortcutPhase phase)
{
    if (namespaceName != QStringLiteral("astrea-shell")
        || phase == TyphonShortcutPhase::Released || phase == TyphonShortcutPhase::Cancelled) {
        return AltTabShortcutAction::Ignore;
    }

    if (name == QStringLiteral("alt_tab_next")
        && (phase == TyphonShortcutPhase::Pressed || phase == TyphonShortcutPhase::Repeated)) {
        return AltTabShortcutAction::Next;
    }
    if (name == QStringLiteral("alt_tab_previous")
        && (phase == TyphonShortcutPhase::Pressed || phase == TyphonShortcutPhase::Repeated)) {
        return AltTabShortcutAction::Previous;
    }
    if (name == QStringLiteral("alt_tab_commit") && phase == TyphonShortcutPhase::Pressed)
        return AltTabShortcutAction::Commit;
    if (name == QStringLiteral("spotlight_toggle") && phase == TyphonShortcutPhase::Pressed)
        return AltTabShortcutAction::SpotlightToggle;
    return AltTabShortcutAction::Ignore;
}
