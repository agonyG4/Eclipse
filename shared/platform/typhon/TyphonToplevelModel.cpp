#include "platform/typhon/TyphonToplevelModel.hpp"

#include <QDebug>
#include <QSet>

#include <algorithm>

using namespace Astrea::Typhon;

namespace {

bool validIdentifier(const QString &id)
{
    if (id.isEmpty())
        return false;
    for (const QChar character : id) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9'))
            return false;
    }
    bool ok = false;
    const quint64 numericId = id.toULongLong(&ok, 10);
    return ok && numericId != 0;
}

} // namespace

TyphonToplevelModel::TyphonToplevelModel(QObject *parent)
    : QObject(parent)
{
}

TyphonToplevelModel::EventResult TyphonToplevelModel::startGeneration(quint64 generation)
{
    if (generation == 0)
        return reject(QStringLiteral("invalid zero connection generation"));

    m_generation = generation;
    m_generationStarted = true;
    m_degraded = false;
    m_lastError.clear();
    m_hasSnapshot = false;
    m_snapshot = {};
    m_committed.clear();
    m_pending.clear();
    m_lastRevision = 0;
    return EventResult::Accepted;
}

TyphonToplevelModel::EventResult TyphonToplevelModel::checkGeneration(quint64 generation) const
{
    if (!m_generationStarted || generation != m_generation)
        return EventResult::IgnoredStaleGeneration;
    return EventResult::Accepted;
}

TyphonToplevelModel::EventResult TyphonToplevelModel::reject(const QString &diagnostic)
{
    m_degraded = true;
    m_lastError = diagnostic;
    m_committed.clear();
    m_pending.clear();
    m_snapshot = {};
    m_hasSnapshot = false;
    emit degraded(diagnostic);
    qWarning("Typhon toplevel protocol contradiction: %s", qPrintable(diagnostic));
    return EventResult::Rejected;
}

TyphonToplevelModel::PendingHandleState *TyphonToplevelModel::pendingForUpdate(quint64 token)
{
    auto existing = m_pending.find(token);
    if (existing != m_pending.end())
        return &existing.value();

    const auto committed = m_committed.constFind(token);
    if (committed == m_committed.constEnd())
        return nullptr;

    PendingHandleState pending;
    pending.id = committed->id;
    pending.appId = committed->appId;
    pending.title = committed->title;
    pending.pid = committed->pid;
    pending.kind = committed->kind;
    pending.states = committed->states;
    pending.rawStateBits = committed->rawStateBits;
    pending.focusSerial = committed->focusSerial;
    pending.complete = true;
    existing = m_pending.insert(token, pending);
    return &existing.value();
}

const TyphonToplevelModel::PendingHandleState *TyphonToplevelModel::pendingFor(quint64 token) const
{
    const auto it = m_pending.constFind(token);
    return it == m_pending.constEnd() ? nullptr : &it.value();
}

TyphonToplevelModel::EventResult TyphonToplevelModel::handleCreated(quint64 generation, quint64 token)
{
    const EventResult generationResult = checkGeneration(generation);
    if (generationResult != EventResult::Accepted)
        return generationResult;
    if (token == 0 || m_pending.contains(token) || m_committed.contains(token))
        return reject(QStringLiteral("duplicate or invalid local handle token"));
    m_pending.insert(token, PendingHandleState{});
    return EventResult::Accepted;
}

TyphonToplevelModel::EventResult TyphonToplevelModel::identifierChanged(
    quint64 generation, quint64 token, const QString &id)
{
    const EventResult generationResult = checkGeneration(generation);
    if (generationResult != EventResult::Accepted)
        return generationResult;
    PendingHandleState *pending = pendingForUpdate(token);
    if (!pending || pending->closed || pending->doneRevision.has_value())
        return reject(QStringLiteral("identifier event after handle terminal state"));
    if (!validIdentifier(id))
        return reject(QStringLiteral("invalid toplevel identifier"));
    if (pending->id.has_value() && pending->id.value() != id)
        return reject(QStringLiteral("toplevel identifier changed after assignment"));
    pending->id = id;
    return EventResult::Accepted;
}

#define TYPHON_METADATA_EVENT(methodName, fieldName, valueType, valueExpression, message) \
TyphonToplevelModel::EventResult TyphonToplevelModel::methodName( \
    quint64 generation, quint64 token, valueType valueExpression) \
{ \
    const EventResult generationResult = checkGeneration(generation); \
    if (generationResult != EventResult::Accepted) \
        return generationResult; \
    PendingHandleState *pending = pendingForUpdate(token); \
    if (!pending || pending->closed || pending->doneRevision.has_value()) \
        return reject(QStringLiteral(message)); \
    pending->fieldName = valueExpression; \
    return EventResult::Accepted; \
}

TYPHON_METADATA_EVENT(appIdChanged, appId, const QString &, appId,
                      "metadata event after handle terminal state")
TYPHON_METADATA_EVENT(titleChanged, title, const QString &, title,
                      "metadata event after handle terminal state")
TYPHON_METADATA_EVENT(pidChanged, pid, quint32, pid,
                      "metadata event after handle terminal state")

#undef TYPHON_METADATA_EVENT

TyphonToplevelModel::EventResult TyphonToplevelModel::kindChanged(
    quint64 generation, quint64 token, ToplevelKind kind)
{
    const EventResult generationResult = checkGeneration(generation);
    if (generationResult != EventResult::Accepted)
        return generationResult;
    PendingHandleState *pending = pendingForUpdate(token);
    if (!pending || pending->closed || pending->doneRevision.has_value())
        return reject(QStringLiteral("kind event after handle terminal state"));
    pending->kind = kind;
    return EventResult::Accepted;
}

TyphonToplevelModel::EventResult TyphonToplevelModel::stateChanged(
    quint64 generation, quint64 token, ToplevelStates states, quint32 rawStateBits)
{
    const EventResult generationResult = checkGeneration(generation);
    if (generationResult != EventResult::Accepted)
        return generationResult;
    PendingHandleState *pending = pendingForUpdate(token);
    if (!pending || pending->closed || pending->doneRevision.has_value())
        return reject(QStringLiteral("state event after handle terminal state"));
    pending->states = states;
    pending->rawStateBits = rawStateBits | static_cast<quint32>(states.toInt());
    return EventResult::Accepted;
}

TyphonToplevelModel::EventResult TyphonToplevelModel::focusSerialChanged(
    quint64 generation, quint64 token, FocusSerial serial)
{
    const EventResult generationResult = checkGeneration(generation);
    if (generationResult != EventResult::Accepted)
        return generationResult;
    PendingHandleState *pending = pendingForUpdate(token);
    if (!pending || pending->closed || pending->doneRevision.has_value())
        return reject(QStringLiteral("focus serial event after handle terminal state"));
    pending->focusSerial = serial;
    return EventResult::Accepted;
}

bool TyphonToplevelModel::validateComplete(const PendingHandleState &pending) const
{
    return pending.id.has_value() && !pending.id->isEmpty()
        && pending.appId.has_value() && pending.title.has_value()
        && pending.pid.has_value() && pending.kind.has_value()
        && pending.states.has_value() && pending.rawStateBits.has_value()
        && pending.focusSerial.has_value();
}

std::optional<Toplevel> TyphonToplevelModel::toToplevel(const PendingHandleState &pending) const
{
    if (!validateComplete(pending))
        return std::nullopt;
    Toplevel result;
    result.id = pending.id.value();
    result.appId = pending.appId.value();
    result.title = pending.title.value();
    result.pid = pending.pid.value();
    result.kind = pending.kind.value();
    result.states = pending.states.value();
    result.focusSerial = pending.focusSerial.value();
    result.rawStateBits = pending.rawStateBits.value();
    return result;
}

TyphonToplevelModel::EventResult TyphonToplevelModel::handleDone(
    quint64 generation, quint64 token, Revision revision)
{
    const EventResult generationResult = checkGeneration(generation);
    if (generationResult != EventResult::Accepted)
        return generationResult;
    PendingHandleState *pending = pendingForUpdate(token);
    if (!pending || pending->closed || pending->doneRevision.has_value())
        return reject(QStringLiteral("invalid or duplicate handle done"));
    if (!validateComplete(*pending))
        return reject(QStringLiteral("handle done before all required fields"));
    pending->complete = true;
    pending->doneRevision = revision;
    return EventResult::Accepted;
}

TyphonToplevelModel::EventResult TyphonToplevelModel::handleClosed(quint64 generation, quint64 token)
{
    const EventResult generationResult = checkGeneration(generation);
    if (generationResult != EventResult::Accepted)
        return generationResult;
    PendingHandleState *pending = pendingForUpdate(token);
    if (!pending)
        return reject(QStringLiteral("closed event for unknown handle"));
    if (pending->closed)
        return reject(QStringLiteral("duplicate closed event"));
    pending->closed = true;
    return EventResult::Accepted;
}

void TyphonToplevelModel::sortWindows(QVector<Toplevel> &windows) const
{
    std::sort(windows.begin(), windows.end(), [](const Toplevel &left, const Toplevel &right) {
        if (left.focusSerial != right.focusSerial)
            return left.focusSerial > right.focusSerial;

        const bool leftActive = hasState(left.states, ToplevelStateFlag::Active);
        const bool rightActive = hasState(right.states, ToplevelStateFlag::Active);
        if (leftActive != rightActive)
            return leftActive;

        bool leftOk = false;
        bool rightOk = false;
        const quint64 leftId = left.id.toULongLong(&leftOk, 10);
        const quint64 rightId = right.id.toULongLong(&rightOk, 10);
        if (leftOk && rightOk && leftId != rightId)
            return leftId < rightId;
        if (leftOk != rightOk)
            return leftOk;
        return left.id < right.id;
    });
}

TyphonToplevelModel::EventResult TyphonToplevelModel::managerDone(
    quint64 generation, Revision revision, quint32 total, bool truncated)
{
    const EventResult generationResult = checkGeneration(generation);
    if (generationResult != EventResult::Accepted)
        return generationResult;
    if (m_degraded)
        return reject(QStringLiteral("manager done after manager failure"));
    if (m_hasSnapshot && revision == m_lastRevision)
        return reject(QStringLiteral("duplicate manager revision"));

    QHash<quint64, Toplevel> next = m_committed;
    for (auto it = m_pending.cbegin(); it != m_pending.cend(); ++it) {
        const PendingHandleState &pending = it.value();
        if (pending.closed) {
            next.remove(it.key());
            continue;
        }
        if (!pending.doneRevision.has_value() || pending.doneRevision.value() != revision)
            return reject(QStringLiteral("changed handle has no matching handle revision"));
        const auto toplevel = toToplevel(pending);
        if (!toplevel.has_value())
            return reject(QStringLiteral("manager done before required fields"));
        Toplevel committed = toplevel.value();
        committed.revision = revision;
        next.insert(it.key(), committed);
    }

    QSet<QString> identifiers;
    QVector<Toplevel> windows;
    windows.reserve(next.size());
    for (auto it = next.cbegin(); it != next.cend(); ++it) {
        if (identifiers.contains(it.value().id))
            return reject(QStringLiteral("duplicate live toplevel identifier"));
        identifiers.insert(it.value().id);
        windows.append(it.value());
    }
    const quint32 visibleHandles = static_cast<quint32>(windows.size());
    if ((!truncated && total != visibleHandles) || (truncated && visibleHandles > total))
        return reject(QStringLiteral("manager total does not match live handles"));

    sortWindows(windows);
    m_committed = std::move(next);
    m_pending.clear();
    m_lastRevision = revision;
    m_hasSnapshot = true;
    m_snapshot.windows = std::move(windows);
    m_snapshot.revision = revision;
    m_snapshot.total = total;
    m_snapshot.truncated = truncated;
    m_snapshot.connectionGeneration = generation;
    emit snapshotCommitted(m_snapshot);
    return EventResult::Accepted;
}

TyphonToplevelModel::EventResult TyphonToplevelModel::managerFailed(quint64 generation)
{
    const EventResult generationResult = checkGeneration(generation);
    if (generationResult != EventResult::Accepted)
        return generationResult;
    m_committed.clear();
    m_pending.clear();
    m_snapshot = {};
    m_hasSnapshot = false;
    return reject(QStringLiteral("manager reported terminal failure"));
}

void TyphonToplevelModel::clearSnapshot(quint64 generation)
{
    if (checkGeneration(generation) != EventResult::Accepted)
        return;
    m_committed.clear();
    m_pending.clear();
    m_snapshot = {};
    m_hasSnapshot = false;
}
