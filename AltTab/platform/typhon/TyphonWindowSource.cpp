#include "platform/typhon/TyphonWindowSource.hpp"

#include <algorithm>
#include <limits>
#include <utility>

using namespace Astrea::Typhon;

TyphonWindowSource::TyphonWindowSource(TyphonToplevelConnection *connection, QObject *parent)
    : CompositorBackend(parent)
{
    m_connection = connection ? connection : new TyphonToplevelConnection(static_cast<TyphonProtocolAdapter *>(nullptr), this);
    if (!m_connection->parent())
        m_connection->setParent(this);
    connect(m_connection, &TyphonToplevelConnection::stateChanged,
            this, &TyphonWindowSource::onConnectionStateChanged);
    connect(m_connection, &TyphonToplevelConnection::snapshotChanged,
            this, &TyphonWindowSource::onConnectionSnapshot);
    connect(m_connection, &TyphonToplevelConnection::actionFinished, this,
            [this](quint64 token, ToplevelAction action, ToplevelActionResult result) {
        if (!m_started || action != ToplevelAction::Activate)
            return;
        ActivationResult activation;
        activation.success = result != ToplevelActionResult::Unavailable;
        if (!activation.success)
            activation.error = QStringLiteral("Typhon activation unavailable");
        emit activationFinished(token, activation);
    });
    connect(m_connection, &TyphonToplevelConnection::actionFailed, this,
            [this](quint64 token, ToplevelAction action, ToplevelActionError error) {
        if (!m_started || action != ToplevelAction::Activate)
            return;
        QString message;
        switch (error) {
        case ToplevelActionError::UnsupportedProtocol:
            message = QStringLiteral("Typhon toplevel actions are unsupported");
            break;
        case ToplevelActionError::NotAuthenticated:
            message = QStringLiteral("Typhon toplevel actions are not authenticated");
            break;
        case ToplevelActionError::Disconnected:
            message = QStringLiteral("Typhon connection disconnected");
            break;
        case ToplevelActionError::LocalCapacityExceeded:
            message = QStringLiteral("Typhon action capacity is exhausted");
            break;
        case ToplevelActionError::ToplevelNotLive:
            message = QStringLiteral("selected Typhon window is no longer live");
            break;
        case ToplevelActionError::InvalidRequest:
            message = QStringLiteral("invalid Typhon action request");
            break;
        }
        emit activationFinished(token, ActivationResult{false, message});
    });
}

TyphonWindowSource::~TyphonWindowSource()
{
    stop();
}

void TyphonWindowSource::start()
{
    if (m_started)
        return;
    m_started = true;
    m_observedConnectionGeneration = 0;
    if (m_connection)
        m_connection->start();
}

void TyphonWindowSource::stop()
{
    if (!m_started && m_state == BackendState::Stopped && m_pendingSnapshotRequests.isEmpty())
        return;
    m_started = false;
    if (m_connection)
        m_connection->stop();
    failPendingSnapshotRequests();
    m_hasSnapshot = false;
    m_cachedSnapshot = {};
    m_observedConnectionGeneration = 0;
    setBackendState(BackendState::Stopped);
}

BackendDescriptor TyphonWindowSource::descriptor() const
{
    return {QStringLiteral("typhon"), 2};
}

BackendState TyphonWindowSource::state() const
{
    return m_state;
}

QVector<BackendCapability> TyphonWindowSource::capabilities() const
{
    QVector<BackendCapability> capabilities = {BackendCapability::WindowList,
                                               BackendCapability::EventStream,
                                               BackendCapability::ActiveWindow};
    if (m_connection
        && m_connection->actionCapability() == TyphonActionCapabilityState::ActionReadyV2) {
        capabilities.append(BackendCapability::WindowActivation);
    }
    return capabilities;
}

std::optional<WindowSnapshot> TyphonWindowSource::cachedSnapshot() const
{
    return m_hasSnapshot ? std::optional<WindowSnapshot>(m_cachedSnapshot) : std::nullopt;
}

void TyphonWindowSource::requestSnapshot(RequestToken token)
{
    if (std::any_of(m_pendingSnapshotRequests.cbegin(), m_pendingSnapshotRequests.cend(),
                    [token](const PendingSnapshotRequest &pending) {
                        return pending.token == token;
                    })) {
        return;
    }

    if (m_hasSnapshot) {
        emit snapshotReady(token, m_cachedSnapshot);
        return;
    }

    if (m_pendingSnapshotRequests.size() >= kMaxPendingSnapshotRequests) {
        emit snapshotReady(token, {});
        emit backendError(BackendError::ConnectionFailed);
        return;
    }

    m_pendingSnapshotRequests.append({token, m_connection ? m_connection->connectionGeneration() : 0});
}

void TyphonWindowSource::activateWindow(ActivationRequest request)
{
    if (!m_connection) {
        emit activationFinished(request.token,
                                 ActivationResult{false, QStringLiteral("Typhon connection is unavailable")});
        return;
    }

    const auto error = m_connection->requestAction(
        request.windowId.value, ToplevelAction::Activate, request.token);
    if (!error.has_value())
        return;

    QString message;
    switch (error.value()) {
    case ToplevelActionError::UnsupportedProtocol:
        message = QStringLiteral("Typhon toplevel actions are unsupported");
        break;
    case ToplevelActionError::NotAuthenticated:
        message = QStringLiteral("Typhon toplevel actions are not authenticated");
        break;
    case ToplevelActionError::Disconnected:
        message = QStringLiteral("Typhon connection disconnected");
        break;
    case ToplevelActionError::LocalCapacityExceeded:
        message = QStringLiteral("Typhon action capacity is exhausted");
        break;
    case ToplevelActionError::ToplevelNotLive:
        message = QStringLiteral("selected Typhon window is no longer live");
        break;
    case ToplevelActionError::InvalidRequest:
        message = QStringLiteral("invalid Typhon action request");
        break;
    }
    emit activationFinished(request.token, ActivationResult{false, message});
}

void TyphonWindowSource::onConnectionStateChanged(TyphonConnectionState state)
{
    if (!m_started)
        return;

    const quint64 connectionGeneration = m_connection ? m_connection->connectionGeneration() : 0;
    if (connectionGeneration != 0 && connectionGeneration != m_observedConnectionGeneration) {
        m_observedConnectionGeneration = connectionGeneration;
        failStalePendingSnapshotRequests(connectionGeneration);
    }

    if (state == TyphonConnectionState::Degraded
        || state == TyphonConnectionState::Disconnected
        || state == TyphonConnectionState::Unsupported
        || state == TyphonConnectionState::Stopped) {
        failPendingSnapshotRequests();
    }
    setBackendState(mapState(state));
}

void TyphonWindowSource::onConnectionSnapshot(const Snapshot &snapshot)
{
    if (!m_started)
        return;
    if (snapshot.connectionGeneration == 0) {
        failPendingSnapshotRequests();
        m_hasSnapshot = false;
        m_cachedSnapshot = {};
        emit snapshotChanged(m_cachedSnapshot);
        return;
    }

    if (snapshot.connectionGeneration != m_observedConnectionGeneration) {
        m_observedConnectionGeneration = snapshot.connectionGeneration;
        failStalePendingSnapshotRequests(snapshot.connectionGeneration);
    }

    m_cachedSnapshot = mapSnapshotForTest(snapshot);
    m_hasSnapshot = true;
    emit snapshotChanged(m_cachedSnapshot);

    const QVector<PendingSnapshotRequest> pending = std::exchange(m_pendingSnapshotRequests, {});
    for (const PendingSnapshotRequest &request : pending) {
        if (request.connectionGeneration != snapshot.connectionGeneration) {
            emit snapshotReady(request.token, {});
            emit backendError(BackendError::ConnectionFailed);
            continue;
        }
        emit snapshotReady(request.token, m_cachedSnapshot);
    }
}

void TyphonWindowSource::failPendingSnapshotRequests()
{
    const QVector<PendingSnapshotRequest> pending = std::exchange(m_pendingSnapshotRequests, {});
    for (const PendingSnapshotRequest &request : pending) {
        emit snapshotReady(request.token, {});
        emit backendError(BackendError::ConnectionFailed);
    }
}

void TyphonWindowSource::failStalePendingSnapshotRequests(quint64 connectionGeneration)
{
    QVector<PendingSnapshotRequest> current;
    current.reserve(m_pendingSnapshotRequests.size());
    for (const PendingSnapshotRequest &request : std::as_const(m_pendingSnapshotRequests)) {
        if (request.connectionGeneration == connectionGeneration) {
            current.append(request);
            continue;
        }
        emit snapshotReady(request.token, {});
        emit backendError(BackendError::ConnectionFailed);
    }
    m_pendingSnapshotRequests = std::move(current);
}

void TyphonWindowSource::setBackendState(BackendState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(m_state);
}

BackendState TyphonWindowSource::mapState(TyphonConnectionState state)
{
    switch (state) {
    case TyphonConnectionState::Stopped: return BackendState::Stopped;
    case TyphonConnectionState::Connecting: return BackendState::ConnectingEvents;
    case TyphonConnectionState::WaitingForRegistry: return BackendState::ConnectingEvents;
    case TyphonConnectionState::WaitingForInitialSnapshot: return BackendState::LoadingInitialSnapshot;
    case TyphonConnectionState::Ready: return BackendState::Ready;
    case TyphonConnectionState::Degraded: return BackendState::Degraded;
    case TyphonConnectionState::Disconnected: return BackendState::Disconnected;
    case TyphonConnectionState::Unsupported: return BackendState::Unsupported;
    }
    return BackendState::Degraded;
}

WindowSnapshot TyphonWindowSource::mapSnapshotForTest(const Snapshot &snapshot) const
{
    WindowSnapshot result;
    result.revision = snapshot.revision;
    result.total = snapshot.total;
    result.truncated = snapshot.truncated;
    result.backendGeneration = snapshot.connectionGeneration;

    int focusedCount = 0;
    int neverFocusedCount = 0;
    result.windows.reserve(snapshot.windows.size());
    for (const Toplevel &toplevel : snapshot.windows) {
        WindowInfo window;
        window.windowId = WindowId{toplevel.id};
        window.pid = static_cast<qint64>(toplevel.pid);
        window.appId = toplevel.appId;
        window.className = toplevel.appId;
        window.title = toplevel.title;
        window.initialTitle = toplevel.title;
        window.displayName = WindowInfo::displayNameFromMetadata(window.className, window.title);
        window.isActive = hasState(toplevel.states, ToplevelStateFlag::Active);
        window.isMinimized = hasState(toplevel.states, ToplevelStateFlag::Minimized);
        window.isHidden = false;
        window.skipSwitcher = false;
        window.backendGeneration = snapshot.connectionGeneration;
        if (toplevel.focusSerial > 0) {
            window.focusHistoryId = focusedCount++;
        } else {
            window.focusHistoryId = 1000000 + neverFocusedCount++;
        }
        if (window.isActive)
            result.activeWindowId = window.windowId;
        result.windows.append(window);
    }
    return result;
}
