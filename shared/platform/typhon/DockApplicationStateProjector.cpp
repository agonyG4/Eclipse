#include "platform/typhon/DockApplicationStateProjector.hpp"

#include "platform/typhon/TyphonAppMatcher.hpp"

#include <limits>

using namespace Astrea::Typhon;

QHash<QString, DockApplicationRuntimeState> DockApplicationStateProjector::project(
    const Snapshot &snapshot,
    const std::shared_ptr<const DesktopEntrySnapshot> &desktopEntries,
    const QStringList &currentPins) const
{
    QHash<QString, DockApplicationRuntimeState> result;
    for (const QString &pin : currentPins) {
        if (pin.isEmpty() || result.contains(pin))
            continue;
        DockApplicationRuntimeState state;
        state.desktopFileName = pin;
        result.insert(pin, state);
    }

    TyphonAppMatcher matcher(desktopEntries);
    for (const Toplevel &window : snapshot.windows) {
        const TyphonAppMatch app = matcher.match({window.appId, window.title, window.pid, window.kind});
        if (app.desktopFileName.isEmpty())
            continue;

        DockApplicationRuntimeState &state = result[app.desktopFileName];
        if (state.desktopFileName.isEmpty())
            state.desktopFileName = app.desktopFileName;
        state.running = true;
        state.active = state.active || hasState(window.states, ToplevelStateFlag::Active);
        if (state.windowCount < std::numeric_limits<int>::max())
            ++state.windowCount;
        int insertAt = 0;
        while (insertAt < state.focusSerials.size()
               && state.focusSerials.at(insertAt) >= window.focusSerial) {
            ++insertAt;
        }
        state.windowIds.insert(insertAt, window.id);
        state.focusSerials.insert(insertAt, window.focusSerial);
    }
    return result;
}
