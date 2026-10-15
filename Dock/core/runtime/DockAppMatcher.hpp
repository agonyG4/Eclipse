#pragma once

#include "apps/DesktopEntryCatalog.hpp"
#include "core/runtime/DockRuntimeTypes.hpp"

#include <memory>

namespace Astrea::Dock {

enum class DockAppMatchConfidence {
    Unresolved,
    ExactDesktopFileName,
    ExactDesktopId,
    CaseInsensitiveDesktopId,
    ExactStartupWmClass,
    CaseInsensitiveStartupWmClass,
    NormalizedReverseDnsDesktopId
};

struct DockAppMatch {
    QString desktopFileName;
    QString desktopId;
    QString displayName;
    QString iconName;
    QString iconPath;
    DockAppMatchConfidence confidence = DockAppMatchConfidence::Unresolved;
};

class DockAppMatcher final {
public:
    explicit DockAppMatcher(std::shared_ptr<const DesktopEntrySnapshot> snapshot = {});

    void setSnapshot(std::shared_ptr<const DesktopEntrySnapshot> snapshot);
    DockAppMatch match(const DockRuntimeWindow &window) const;

private:
    std::shared_ptr<const DesktopEntrySnapshot> m_snapshot;
};

} // namespace Astrea::Dock

Q_DECLARE_METATYPE(Astrea::Dock::DockAppMatchConfidence)
Q_DECLARE_METATYPE(Astrea::Dock::DockAppMatch)
