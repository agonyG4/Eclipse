#pragma once

#include "apps/DesktopEntryCatalog.hpp"
#include "core/runtime/DockRuntimeTypes.hpp"

#include <QHash>

#include <memory>
#include <optional>

namespace Astrea::Dock {

class DockRuntimeTaskIdentityTracker final {
public:
    QHash<QString, QString> update(
        const DockRuntimeSnapshot &snapshot,
        const std::shared_ptr<const DesktopEntrySnapshot> &desktopEntries);
    void reset();

private:
    std::optional<quint64> m_generation;
    QHash<QString, QString> m_windowTaskKeys;
    QHash<QString, QString> m_appTaskKeys;
};

} // namespace Astrea::Dock
