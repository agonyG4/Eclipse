#pragma once

#include "platform/typhon/TyphonProtocolTypes.hpp"

#include <QHash>
#include <QVector>

#include <optional>

namespace Astrea::Typhon {

enum class TyphonActionAdmission {
    Accepted,
    InvalidToken,
    DuplicatePending,
    CapacityExceeded,
};

class TyphonActionState final {
public:
    static constexpr qsizetype kMaxPendingActions = 64;

    TyphonActionToken nextToken(quint64 connectionGeneration);

    TyphonActionAdmission reserve(quint64 connectionGeneration,
                                  TyphonActionToken token,
                                  const QString &windowId,
                                  ToplevelAction action,
                                  quint64 consumerToken);

    std::optional<TyphonPendingAction> complete(quint64 connectionGeneration,
                                                TyphonActionToken token,
                                                ToplevelAction action,
                                                ToplevelActionResult result);

    bool discard(quint64 connectionGeneration, TyphonActionToken token,
                 ToplevelAction action);

    QVector<TyphonPendingAction> clearGeneration(quint64 connectionGeneration);
    qsizetype pendingCount() const { return m_pending.size(); }

private:
    quint32 m_sequence = 0;
    QHash<TyphonActionToken, TyphonPendingAction> m_pending;
};

} // namespace Astrea::Typhon
