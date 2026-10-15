#include "platform/typhon/TyphonDockRuntimeBackend.hpp"

#include "platform/typhon/TyphonToplevelConnection.hpp"

#include <QStringList>

using namespace Astrea::Dock;

namespace {

DockRuntimeWindow dockWindow(const Astrea::Typhon::Toplevel &window)
{
    DockRuntimeWindow result;
    result.id = window.id;
    result.appId = window.appId;
    result.title = window.title;
    result.pid = window.pid;
    result.active = Astrea::Typhon::hasState(
        window.states, Astrea::Typhon::ToplevelStateFlag::Active);
    result.minimized = Astrea::Typhon::hasState(
        window.states, Astrea::Typhon::ToplevelStateFlag::Minimized);
    result.maximized = Astrea::Typhon::hasState(
        window.states, Astrea::Typhon::ToplevelStateFlag::Maximized);
    result.fullscreen = Astrea::Typhon::hasState(
        window.states, Astrea::Typhon::ToplevelStateFlag::Fullscreen);
    result.focusSerial = window.focusSerial;
    result.revision = window.revision;
    return result;
}

DockRuntimeSnapshot dockSnapshot(const Astrea::Typhon::Snapshot &snapshot)
{
    DockRuntimeSnapshot result;
    result.revision = snapshot.revision;
    result.total = snapshot.total;
    result.truncated = snapshot.truncated;
    result.generation = snapshot.connectionGeneration;
    result.authoritative = true;
    result.windows.reserve(snapshot.windows.size());
    for (const Astrea::Typhon::Toplevel &window : snapshot.windows)
        result.windows.append(dockWindow(window));
    return result;
}

std::optional<DockRuntimeAction> dockAction(Astrea::Typhon::ToplevelAction action)
{
    switch (action) {
    case Astrea::Typhon::ToplevelAction::Activate:
        return DockRuntimeAction::Activate;
    case Astrea::Typhon::ToplevelAction::Close:
        return DockRuntimeAction::Close;
    case Astrea::Typhon::ToplevelAction::Minimize:
    case Astrea::Typhon::ToplevelAction::Restore:
        return std::nullopt;
    }
    return std::nullopt;
}

Astrea::Typhon::ToplevelAction typhonAction(DockRuntimeAction action)
{
    switch (action) {
    case DockRuntimeAction::Activate: return Astrea::Typhon::ToplevelAction::Activate;
    case DockRuntimeAction::Close: return Astrea::Typhon::ToplevelAction::Close;
    }
    return Astrea::Typhon::ToplevelAction::Activate;
}

DockRuntimeActionResult dockResult(Astrea::Typhon::ToplevelActionResult result)
{
    switch (result) {
    case Astrea::Typhon::ToplevelActionResult::Accepted:
        return DockRuntimeActionResult::Accepted;
    case Astrea::Typhon::ToplevelActionResult::NoChange:
        return DockRuntimeActionResult::NoChange;
    case Astrea::Typhon::ToplevelActionResult::Unavailable:
        return DockRuntimeActionResult::Unavailable;
    }
    return DockRuntimeActionResult::Unavailable;
}

DockRuntimeActionError dockError(Astrea::Typhon::ToplevelActionError error)
{
    switch (error) {
    case Astrea::Typhon::ToplevelActionError::UnsupportedProtocol:
        return DockRuntimeActionError::UnsupportedProtocol;
    case Astrea::Typhon::ToplevelActionError::NotAuthenticated:
        return DockRuntimeActionError::NotActionReady;
    case Astrea::Typhon::ToplevelActionError::Disconnected:
        return DockRuntimeActionError::Disconnected;
    case Astrea::Typhon::ToplevelActionError::LocalCapacityExceeded:
        return DockRuntimeActionError::LocalCapacityExceeded;
    case Astrea::Typhon::ToplevelActionError::ToplevelNotLive:
        return DockRuntimeActionError::WindowNotLive;
    case Astrea::Typhon::ToplevelActionError::InvalidRequest:
        return DockRuntimeActionError::InvalidRequest;
    }
    return DockRuntimeActionError::InvalidRequest;
}

QString connectionStateToken(TyphonConnectionState state)
{
    switch (state) {
    case TyphonConnectionState::Stopped: return QStringLiteral("stopped");
    case TyphonConnectionState::Connecting: return QStringLiteral("connecting");
    case TyphonConnectionState::WaitingForRegistry:
        return QStringLiteral("waiting_for_registry");
    case TyphonConnectionState::WaitingForInitialSnapshot:
        return QStringLiteral("waiting_for_initial_snapshot");
    case TyphonConnectionState::Ready: return QStringLiteral("ready");
    case TyphonConnectionState::Degraded: return QStringLiteral("degraded");
    case TyphonConnectionState::Disconnected: return QStringLiteral("disconnected");
    case TyphonConnectionState::Unsupported: return QStringLiteral("unsupported");
    }
    return QStringLiteral("disconnected");
}

QString capabilityToken(Astrea::Typhon::TyphonActionCapabilityState capability)
{
    switch (capability) {
    case Astrea::Typhon::TyphonActionCapabilityState::Disconnected:
        return QStringLiteral("disconnected");
    case Astrea::Typhon::TyphonActionCapabilityState::ReadOnlyV1:
        return QStringLiteral("read_only_v1");
    case Astrea::Typhon::TyphonActionCapabilityState::ReadOnlyV2:
        return QStringLiteral("read_only_v2");
    case Astrea::Typhon::TyphonActionCapabilityState::AuthenticatingV2:
        return QStringLiteral("authenticating_v2");
    case Astrea::Typhon::TyphonActionCapabilityState::ActionReadyV2:
        return QStringLiteral("action_ready_v2");
    case Astrea::Typhon::TyphonActionCapabilityState::Degraded:
        return QStringLiteral("degraded");
    }
    return QStringLiteral("disconnected");
}

} // namespace

TyphonDockRuntimeBackend::TyphonDockRuntimeBackend(TyphonToplevelConnection *connection,
                                                   QObject *parent)
    : DockRuntimeBackend(parent), m_connection(connection)
{
    if (!m_connection)
        return;

    connect(m_connection, &TyphonToplevelConnection::snapshotChanged, this,
            [this](const Astrea::Typhon::Snapshot &) { refreshRuntimeState(true); });
    connect(m_connection, &TyphonToplevelConnection::stateChanged, this,
            [this](TyphonConnectionState) { refreshRuntimeState(true); });
    connect(m_connection, &TyphonToplevelConnection::actionCapabilityChanged, this,
            [this](Astrea::Typhon::TyphonActionCapabilityState) { refreshActionReadiness(); });
    connect(m_connection, &TyphonToplevelConnection::actionFinished, this,
            [this](quint64 token, Astrea::Typhon::ToplevelAction action,
                   Astrea::Typhon::ToplevelActionResult result) {
        const auto translatedAction = dockAction(action);
        if (!translatedAction)
            return;
        emit actionFinished(token, *translatedAction, dockResult(result));
    });
    connect(m_connection, &TyphonToplevelConnection::actionFailed, this,
            [this](quint64 token, Astrea::Typhon::ToplevelAction action,
                   Astrea::Typhon::ToplevelActionError error) {
        const auto translatedAction = dockAction(action);
        if (!translatedAction)
            return;
        emit actionFailed(token, *translatedAction, dockError(error));
    });

    refreshRuntimeState(false);
    refreshActionReadiness();
}

quint64 TyphonDockRuntimeBackend::generation() const
{
    return m_connection ? m_connection->connectionGeneration() : 0;
}

std::optional<quint64> TyphonDockRuntimeBackend::handleTokenForWindowId(
    const QString &windowId) const
{
    return m_connection ? m_connection->handleTokenForWindowId(windowId) : std::nullopt;
}

std::optional<DockRuntimeActionError> TyphonDockRuntimeBackend::requestAction(
    const QString &windowId, DockRuntimeAction action, quint64 consumerToken)
{
    if (!m_connection)
        return DockRuntimeActionError::Disconnected;
    const auto error = m_connection->requestAction(windowId, typhonAction(action), consumerToken);
    return error.has_value() ? std::optional<DockRuntimeActionError>(dockError(*error))
                             : std::nullopt;
}

std::optional<DockRuntimeActionError> TyphonDockRuntimeBackend::setMinimizeAnchor(
    const QString &windowId, const QRect &rect)
{
    if (!m_connection)
        return DockRuntimeActionError::Disconnected;
    const auto error = m_connection->setMinimizeAnchor(windowId, rect);
    return error.has_value() ? std::optional<DockRuntimeActionError>(dockError(*error))
                             : std::nullopt;
}

std::optional<DockRuntimeActionError> TyphonDockRuntimeBackend::clearMinimizeAnchor(
    const QString &windowId)
{
    if (!m_connection)
        return DockRuntimeActionError::Disconnected;
    const auto error = m_connection->clearMinimizeAnchor(windowId);
    return error.has_value() ? std::optional<DockRuntimeActionError>(dockError(*error))
                             : std::nullopt;
}

QString TyphonDockRuntimeBackend::diagnosticContext() const
{
    const QString state = m_connection
        ? connectionStateToken(m_connection->state()) : QStringLiteral("detached");
    const QString capability = m_connection
        ? capabilityToken(m_connection->actionCapability()) : QStringLiteral("disconnected");
    return QStringLiteral("connection_state=%1 capability=%2 generation=%3")
        .arg(state, capability).arg(generation());
}

void TyphonDockRuntimeBackend::refreshRuntimeState(bool publishSnapshot)
{
    const bool wasReady = m_runtimeReady;
    m_runtimeReady = m_connection
        && m_connection->state() == TyphonConnectionState::Ready
        && m_connection->hasSnapshot();
    m_snapshot = m_runtimeReady ? dockSnapshot(m_connection->snapshot()) : DockRuntimeSnapshot{};
    if (wasReady != m_runtimeReady)
        emit runtimeReadyChanged(m_runtimeReady);
    if (publishSnapshot)
        emit snapshotChanged(m_snapshot);
}

void TyphonDockRuntimeBackend::refreshActionReadiness()
{
    const bool ready = m_connection
        && m_connection->actionCapability()
            == Astrea::Typhon::TyphonActionCapabilityState::ActionReadyV2;
    if (ready == m_actionReady)
        return;
    m_actionReady = ready;
    emit actionReadyChanged(m_actionReady);
}
