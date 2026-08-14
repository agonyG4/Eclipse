#include "platform/typhon/DockApplicationStateProjector.hpp"

#include "platform/typhon/TyphonAppMatcher.hpp"

#include <limits>

using namespace Astrea::Typhon;

DockApplicationRuntimeProjection DockApplicationStateProjector::project(
    const Snapshot &snapshot,
    const std::shared_ptr<const DesktopEntrySnapshot> &desktopEntries) const
{
    DockApplicationRuntimeProjection result;
    TyphonAppMatcher matcher(desktopEntries);
    for (const Toplevel &window : snapshot.windows) {
        const TyphonAppMatch app = matcher.match({window.appId, window.title, window.pid, window.kind});
        if (app.desktopFileName.isEmpty())
            continue;

        if (!result.states.contains(app.desktopFileName))
            result.encounterOrder.append(app.desktopFileName);
        DockApplicationRuntimeState &state = result.states[app.desktopFileName];
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
