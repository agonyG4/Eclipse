#pragma once

#include "platform/typhon/TyphonShortcutClient.hpp"

enum class AltTabShortcutAction {
    Ignore,
    Next,
    Previous,
    Commit,
};
Q_DECLARE_METATYPE(AltTabShortcutAction)

AltTabShortcutAction mapTyphonShortcut(const QString &namespaceName, const QString &name,
                                       TyphonShortcutPhase phase);
