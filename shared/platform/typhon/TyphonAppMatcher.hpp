#pragma once

#include "apps/DesktopEntryCatalog.hpp"
#include "platform/typhon/TyphonProtocolTypes.hpp"

#include <memory>

namespace Astrea::Typhon {

enum class MatchConfidence {
    Unresolved,
    ExactDesktopFileName,
    ExactDesktopId,
    CaseInsensitiveDesktopId,
    ExactStartupWmClass,
    CaseInsensitiveStartupWmClass,
    NormalizedReverseDnsDesktopId
};

struct TyphonAppMatchInput {
    QString appId;
    QString title;
    quint32 pid = 0;
    ToplevelKind kind = ToplevelKind::XdgToplevel;
};

struct TyphonAppMatch {
    QString desktopFileName;
    QString desktopId;
    QString displayName;
    QString iconName;
    QString iconPath;
    MatchConfidence confidence = MatchConfidence::Unresolved;
};

class TyphonAppMatcher final {
public:
    explicit TyphonAppMatcher(std::shared_ptr<const DesktopEntrySnapshot> snapshot = {});

    void setSnapshot(std::shared_ptr<const DesktopEntrySnapshot> snapshot);
    TyphonAppMatch match(const TyphonAppMatchInput &input) const;

private:
    std::shared_ptr<const DesktopEntrySnapshot> m_snapshot;
};

} // namespace Astrea::Typhon

Q_DECLARE_METATYPE(Astrea::Typhon::MatchConfidence)
Q_DECLARE_METATYPE(Astrea::Typhon::TyphonAppMatchInput)
Q_DECLARE_METATYPE(Astrea::Typhon::TyphonAppMatch)
