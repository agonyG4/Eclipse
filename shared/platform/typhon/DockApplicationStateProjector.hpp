#pragma once

#include "apps/DesktopEntryCatalog.hpp"
#include "platform/typhon/TyphonProtocolTypes.hpp"

#include <QHash>
#include <QStringList>

namespace Astrea::Typhon {

struct DockApplicationRuntimeState {
    QString desktopFileName;
    bool running = false;
    bool active = false;
    int windowCount = 0;
    QVector<QString> windowIds;
};

class DockApplicationStateProjector final {
public:
    QHash<QString, DockApplicationRuntimeState> project(
        const Snapshot &snapshot,
        const std::shared_ptr<const DesktopEntrySnapshot> &desktopEntries,
        const QStringList &currentPins) const;
};

} // namespace Astrea::Typhon

Q_DECLARE_METATYPE(Astrea::Typhon::DockApplicationRuntimeState)
