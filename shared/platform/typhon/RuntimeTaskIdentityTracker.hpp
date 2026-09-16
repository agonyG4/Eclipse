#pragma once

#include "apps/DesktopEntryCatalog.hpp"
#include "platform/typhon/TyphonProtocolTypes.hpp"

#include <QHash>

#include <memory>
#include <optional>

namespace Astrea::Typhon {

class RuntimeTaskIdentityTracker final {
public:
    QHash<QString, QString> update(
        const Snapshot &snapshot,
        const std::shared_ptr<const DesktopEntrySnapshot> &desktopEntries);
    void reset();

private:
    std::optional<quint64> m_generation;
    QHash<QString, QString> m_windowTaskKeys;
    QHash<QString, QString> m_appTaskKeys;
};

} // namespace Astrea::Typhon
