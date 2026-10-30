#include "platform/typhon/AltTabShortcutAdapter.hpp"

AltTabShortcutPhase toAltTabShortcutPhase(TyphonShortcutPhase phase)
{
    switch (phase) {
    case TyphonShortcutPhase::Pressed: return AltTabShortcutPhase::Pressed;
    case TyphonShortcutPhase::Repeated: return AltTabShortcutPhase::Repeated;
    case TyphonShortcutPhase::Released: return AltTabShortcutPhase::Released;
    case TyphonShortcutPhase::Cancelled: return AltTabShortcutPhase::Cancelled;
    }
    return AltTabShortcutPhase::Cancelled;
}

AltTabShortcutAction mapTyphonShortcut(const QString &namespaceName, const QString &name,
                                       TyphonShortcutPhase phase)
{
    return mapAltTabShortcut(namespaceName, name, toAltTabShortcutPhase(phase));
}
