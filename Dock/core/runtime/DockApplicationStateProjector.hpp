#pragma once

#include "apps/DesktopEntryCatalog.hpp"
#include "core/runtime/DockRuntimeTypes.hpp"

#include <QHash>
#include <QStringList>

namespace Astrea::Dock {

class DockApplicationStateProjector final {
public:
    DockApplicationRuntimeProjection project(
        const DockRuntimeSnapshot &snapshot,
        const std::shared_ptr<const DesktopEntrySnapshot> &desktopEntries,
        const QHash<QString, QString> &taskKeysByWindowId = {}) const;
};

} // namespace Astrea::Dock
