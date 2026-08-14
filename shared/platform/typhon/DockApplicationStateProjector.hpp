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
    QVector<FocusSerial> focusSerials;
};

struct DockApplicationRuntimeProjection {
    QHash<QString, DockApplicationRuntimeState> states;
    QStringList encounterOrder;
};

class DockApplicationStateProjector final {
public:
    DockApplicationRuntimeProjection project(
        const Snapshot &snapshot,
        const std::shared_ptr<const DesktopEntrySnapshot> &desktopEntries) const;
};

} // namespace Astrea::Typhon

Q_DECLARE_METATYPE(Astrea::Typhon::DockApplicationRuntimeState)
Q_DECLARE_METATYPE(Astrea::Typhon::DockApplicationRuntimeProjection)
