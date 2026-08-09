#pragma once

#include "platform/typhon/TyphonShortcutClient.hpp"

enum class AltTabShortcutAction {
    Ignore,
    Next,
    Previous,
    Commit,
    // Shared shell shortcut routed to Spotlight by ShellShortcutDispatcher.
    SpotlightToggle,
};
Q_DECLARE_METATYPE(AltTabShortcutAction)

AltTabShortcutAction mapTyphonShortcut(const QString &namespaceName, const QString &name,
                                       TyphonShortcutPhase phase);
