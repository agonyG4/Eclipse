#include "platform/typhon/DockApplicationStateProjector.hpp"

#include "platform/typhon/TyphonAppMatcher.hpp"

#include <limits>

using namespace Astrea::Typhon;

namespace {

QString taskKeyFor(const Toplevel &window, const TyphonAppMatch &match)
{
    if (!match.desktopFileName.isEmpty())
        return QStringLiteral("desktop:") + match.desktopFileName;

    const QString appId = window.appId.trimmed();
    if (!appId.isEmpty())
        return QStringLiteral("app:") + appId.toCaseFolded();

    return QStringLiteral("window:") + window.id;
}

QString fallbackDisplayName(const Toplevel &window)
{
    const QString title = window.title.trimmed();
    if (!title.isEmpty())
        return title;
    const QString appId = window.appId.trimmed();
    if (!appId.isEmpty())
        return appId;
    return QStringLiteral("Application");
}

} // namespace

DockApplicationRuntimeProjection DockApplicationStateProjector::project(
    const Snapshot &snapshot,
    const std::shared_ptr<const DesktopEntrySnapshot> &desktopEntries) const
{
    DockApplicationRuntimeProjection result;
    TyphonAppMatcher matcher(desktopEntries);
    for (const Toplevel &window : snapshot.windows) {
        const TyphonAppMatch app = matcher.match({window.appId, window.title, window.pid, window.kind});
        const QString taskKey = taskKeyFor(window, app);

        if (!result.states.contains(taskKey))
            result.encounterOrder.append(taskKey);
        DockApplicationRuntimeState &state = result.states[taskKey];
        if (state.taskKey.isEmpty())
            state.taskKey = taskKey;
        if (state.appId.isEmpty())
            state.appId = window.appId.trimmed();
        if (state.desktopFileName.isEmpty())
            state.desktopFileName = app.desktopFileName;
        if (state.desktopId.isEmpty())
            state.desktopId = app.desktopId;
        if (state.displayName.isEmpty())
            state.displayName = app.displayName.isEmpty() ? fallbackDisplayName(window) : app.displayName;
        if (state.iconName.isEmpty())
            state.iconName = app.iconName;
        if (state.iconPath.isEmpty())
            state.iconPath = app.iconPath;
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
