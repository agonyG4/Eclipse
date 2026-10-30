#pragma once

#include "core/AltTabShortcutRouter.hpp"
#include "platform/typhon/TyphonShortcutClient.hpp"

AltTabShortcutPhase toAltTabShortcutPhase(TyphonShortcutPhase phase);
AltTabShortcutAction mapTyphonShortcut(const QString &namespaceName, const QString &name,
                                       TyphonShortcutPhase phase);
