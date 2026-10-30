#pragma once

#include <QMetaType>
#include <QString>

// Platform-neutral lifecycle phases used by AltTab shortcut policy.
enum class AltTabShortcutPhase {
    Pressed,
    Repeated,
    Released,
    Cancelled,
};

Q_DECLARE_METATYPE(AltTabShortcutPhase)

enum class AltTabShortcutAction {
    Ignore,
    Next,
    Previous,
    Commit,
};
Q_DECLARE_METATYPE(AltTabShortcutAction)

AltTabShortcutAction mapAltTabShortcut(const QString &namespaceName, const QString &name,
                                       AltTabShortcutPhase phase);
