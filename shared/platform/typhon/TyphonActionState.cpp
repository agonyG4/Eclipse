#include "platform/typhon/TyphonActionState.hpp"

using namespace Astrea::Typhon;

TyphonActionToken TyphonActionState::nextToken(quint64 connectionGeneration)
{
    if (connectionGeneration == 0)
        return {};
    ++m_sequence;
    if (m_sequence == 0)
        ++m_sequence;
    return {static_cast<quint32>(connectionGeneration), m_sequence};
}

TyphonActionAdmission TyphonActionState::reserve(quint64 connectionGeneration,
                                                 TyphonActionToken token,
                                                 const QString &windowId,
                                                 ToplevelAction action,
                                                 quint64 consumerToken)
{
    if (connectionGeneration == 0 || !token.isValid() || windowId.isEmpty())
        return TyphonActionAdmission::InvalidToken;
    if (m_pending.contains(token))
        return TyphonActionAdmission::DuplicatePending;
    if (m_pending.size() >= kMaxPendingActions)
        return TyphonActionAdmission::CapacityExceeded;

    TyphonPendingAction pending;
    pending.connectionGeneration = connectionGeneration;
    pending.token = token;
    pending.windowId = windowId;
    pending.action = action;
    pending.consumerToken = consumerToken;
    m_pending.insert(token, pending);
    return TyphonActionAdmission::Accepted;
}

std::optional<TyphonPendingAction> TyphonActionState::complete(
    quint64 connectionGeneration, TyphonActionToken token, ToplevelAction action,
    ToplevelActionResult)
{
    if (connectionGeneration == 0)
        return std::nullopt;
    const auto it = m_pending.find(token);
    if (it == m_pending.end() || it->connectionGeneration != connectionGeneration
        || it->action != action) {
        return std::nullopt;
    }
    const TyphonPendingAction completed = it.value();
    m_pending.erase(it);
    return completed;
}

bool TyphonActionState::discard(quint64 connectionGeneration, TyphonActionToken token,
                                ToplevelAction action)
{
    const auto it = m_pending.find(token);
    if (it == m_pending.end() || it->connectionGeneration != connectionGeneration
        || it->action != action) {
        return false;
    }
    m_pending.erase(it);
    return true;
}

QVector<TyphonPendingAction> TyphonActionState::clearGeneration(quint64 connectionGeneration)
{
    QVector<TyphonPendingAction> cleared;
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (it->connectionGeneration != connectionGeneration) {
            ++it;
            continue;
        }
        cleared.append(it.value());
        it = m_pending.erase(it);
    }
    return cleared;
}
