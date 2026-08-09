#pragma once

#include "AltTab/core/AltTabShortcutRouter.hpp"

#include <QString>

class AltTabController;
class SpotlightController;

class ShellShortcutDispatcher final {
public:
    ShellShortcutDispatcher(AltTabController *altTabController,
                            SpotlightController *spotlightController);

    void setAltTabEnabled(bool enabled) { m_altTabEnabled = enabled; }
    void setSpotlightEnabled(bool enabled) { m_spotlightEnabled = enabled; }
    bool altTabEnabled() const { return m_altTabEnabled; }
    bool spotlightEnabled() const { return m_spotlightEnabled; }

    void dispatch(const QString &namespaceName, const QString &name,
                  TyphonShortcutPhase phase);

private:
    AltTabController *m_altTabController = nullptr;
    SpotlightController *m_spotlightController = nullptr;
    bool m_altTabEnabled = false;
    bool m_spotlightEnabled = false;
};
