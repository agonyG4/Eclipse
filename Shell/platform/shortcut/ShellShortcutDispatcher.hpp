#pragma once

#include "platform/typhon/TyphonShortcutClient.hpp"

#include <QString>

class AltTabController;
class SpotlightController;
class ScreenshotController;

enum class ShellShortcutAction {
    Ignore,
    AltTabNext,
    AltTabPrevious,
    AltTabCommit,
    SpotlightToggle,
    ScreenshotQuick,
    ScreenshotRegionFrozen,
    ScreenshotRegionLive,
};
Q_DECLARE_METATYPE(ShellShortcutAction)

ShellShortcutAction mapTyphonShellShortcut(const QString &namespaceName, const QString &name,
                                           TyphonShortcutPhase phase);

class ShellShortcutDispatcher final {
public:
    ShellShortcutDispatcher(AltTabController *altTabController,
                            SpotlightController *spotlightController,
                            ScreenshotController *screenshotController = nullptr);

    void setAltTabEnabled(bool enabled) { m_altTabEnabled = enabled; }
    void setSpotlightEnabled(bool enabled) { m_spotlightEnabled = enabled; }
    bool altTabEnabled() const { return m_altTabEnabled; }
    bool spotlightEnabled() const { return m_spotlightEnabled; }

    void dispatch(const QString &namespaceName, const QString &name,
                  TyphonShortcutPhase phase);

private:
    AltTabController *m_altTabController = nullptr;
    SpotlightController *m_spotlightController = nullptr;
    ScreenshotController *m_screenshotController = nullptr;
    bool m_altTabEnabled = false;
    bool m_spotlightEnabled = false;
};
