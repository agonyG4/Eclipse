#include "platform/typhon/TyphonWindowSource.hpp"

#include <limits>

using namespace Astrea::Typhon;

TyphonWindowSource::TyphonWindowSource(TyphonToplevelConnection *connection, QObject *parent)
    : CompositorBackend(parent)
{
    m_connection = connection ? connection : new TyphonToplevelConnection(nullptr, this);
    if (!m_connection->parent())
        m_connection->setParent(this);
    connect(m_connection, &TyphonToplevelConnection::stateChanged,
            this, &TyphonWindowSource::onConnectionStateChanged);
    connect(m_connection, &TyphonToplevelConnection::snapshotChanged,
            this, &TyphonWindowSource::onConnectionSnapshot);
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
    if (m_connection)
        m_connection->start();
}

void TyphonWindowSource::stop()
{
    if (!m_started && m_state == BackendState::Stopped)
        return;
    m_started = false;
    if (m_connection)
        m_connection->stop();
    m_hasSnapshot = false;
    m_cachedSnapshot = {};
    setBackendState(BackendState::Stopped);
}

BackendDescriptor TyphonWindowSource::descriptor() const
{
    return {QStringLiteral("typhon"), 1};
}

BackendState TyphonWindowSource::state() const
{
    return m_state;
}

QVector<BackendCapability> TyphonWindowSource::capabilities() const
{
    return {BackendCapability::WindowList,
            BackendCapability::EventStream,
            BackendCapability::ActiveWindow};
}

std::optional<WindowSnapshot> TyphonWindowSource::cachedSnapshot() const
{
    return m_hasSnapshot ? std::optional<WindowSnapshot>(m_cachedSnapshot) : std::nullopt;
}

void TyphonWindowSource::requestSnapshot(RequestToken token)
{
    if (m_hasSnapshot)
        emit snapshotReady(token, m_cachedSnapshot);
}

void TyphonWindowSource::activateWindow(ActivationRequest request)
{
    emit activationFinished(request.token,
                            ActivationResult{false, QStringLiteral("Typhon window activation is unsupported")});
}

bool TyphonWindowSource::protocolAvailableOnCurrentDisplay()
{
#if ASTREA_HAVE_TYPHON_PROTOCOL
    return true;
#else
    return false;
#endif
}

void TyphonWindowSource::onConnectionStateChanged(TyphonConnectionState state)
{
    if (!m_started)
        return;
    setBackendState(mapState(state));
}

void TyphonWindowSource::onConnectionSnapshot(const Snapshot &snapshot)
{
    if (!m_started)
        return;
    if (snapshot.connectionGeneration == 0) {
        m_hasSnapshot = false;
        m_cachedSnapshot = {};
        emit snapshotChanged(m_cachedSnapshot);
        return;
    }
    m_cachedSnapshot = mapSnapshotForTest(snapshot);
    m_hasSnapshot = true;
    emit snapshotChanged(m_cachedSnapshot);
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
        window.isHidden = window.isMinimized;
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
