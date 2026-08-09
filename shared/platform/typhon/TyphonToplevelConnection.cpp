#include "platform/typhon/TyphonToplevelConnection.hpp"

#include "platform/typhon/TyphonSharedConnection.hpp"

#include <QDebug>

using namespace Astrea::Typhon;

TyphonToplevelConnection::TyphonToplevelConnection(TyphonProtocolAdapter *adapter, QObject *parent)
    : QObject(parent), m_model(this)
{
    m_adapter = adapter ? adapter : createDefaultTyphonProtocolAdapter(this);
    if (!m_adapter->parent())
        m_adapter->setParent(this);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this] {
        if (m_started)
            beginConnection();
    });
    connect(&m_model, &TyphonToplevelModel::snapshotCommitted, this,
            [this](const Snapshot &snapshot) {
                if (!m_started)
                    return;
                if (m_state == TyphonConnectionState::WaitingForInitialSnapshot) {
                    m_backoffIndex = 0;
                    setState(TyphonConnectionState::Ready);
                    qInfo("Typhon initial snapshot committed: revision %llu, windows %u",
                          static_cast<unsigned long long>(snapshot.revision), snapshot.total);
                }
                m_publicSnapshotPublished = true;
                emit snapshotChanged(snapshot);
            });
}

TyphonToplevelConnection::TyphonToplevelConnection(TyphonSharedConnection *sharedConnection,
                                                   QObject *parent)
    : TyphonToplevelConnection(
          createDefaultTyphonProtocolAdapter(sharedConnection), parent)
{
    m_sharedConnection = sharedConnection;
    if (!m_sharedConnection)
        return;

    connect(m_sharedConnection, &TyphonSharedConnection::ready, this,
            [this](quint64 generation) { beginSharedGeneration(generation); });
    connect(m_sharedConnection, &TyphonSharedConnection::disconnected, this,
            [this](quint64 generation) { handleSharedDisconnected(generation); });
}

TyphonToplevelConnection::~TyphonToplevelConnection()
{
    stop();
}

void TyphonToplevelConnection::start()
{
    if (m_started)
        return;
    m_started = true;
    m_reconnectTimer.stop();

    if (m_sharedConnection) {
        setState(TyphonConnectionState::Connecting);
        if (m_sharedConnection->isReady())
            beginSharedGeneration(m_sharedConnection->connectionGeneration());
        else if (m_sharedConnection->state() == TyphonSharedConnection::State::Stopped)
            m_sharedConnection->start();
        return;
    }

    beginConnection();
}

void TyphonToplevelConnection::stop()
{
    if (!m_started && m_state == TyphonConnectionState::Stopped)
        return;
    m_started = false;
    m_reconnectTimer.stop();
    settlePendingActions(ToplevelActionError::Disconnected);
    disconnectAdapterSignals();
    if (m_adapter)
        m_adapter->stop();
    clearPublicSnapshot();
    setActionCapability(TyphonActionCapabilityState::Disconnected);
    setState(TyphonConnectionState::Stopped);
}

void TyphonToplevelConnection::beginConnection()
{
    if (!m_started)
        return;
    ++m_generation;
    m_actionState.clearGeneration(m_generation - 1);
    m_model.startGeneration(m_generation);
    setState(TyphonConnectionState::Connecting);
    setState(TyphonConnectionState::WaitingForRegistry);
    bindAdapter(m_generation);
    qInfo("Typhon connection attempt generation %llu",
          static_cast<unsigned long long>(m_generation));
    if (m_adapter)
        m_adapter->start();
}

void TyphonToplevelConnection::beginSharedGeneration(quint64 generation)
{
    if (!m_started || !m_sharedConnection || generation == 0)
        return;

    if (m_generation != 0 && m_generation != generation)
        m_actionState.clearGeneration(m_generation);
    m_generation = generation;
    m_model.startGeneration(generation);
    m_publicSnapshotPublished = false;
    disconnectAdapterSignals();
    setState(TyphonConnectionState::Connecting);
    setState(TyphonConnectionState::WaitingForRegistry);
    bindAdapter(generation);
    if (m_adapter)
        m_adapter->start();
}

void TyphonToplevelConnection::handleSharedDisconnected(quint64 generation)
{
    if (!m_started || !m_sharedConnection || generation != m_generation)
        return;

    settlePendingActions(ToplevelActionError::Disconnected);
    if (m_adapter)
        m_adapter->stop();
    disconnectAdapterSignals();
    clearPublicSnapshot();
    setActionCapability(TyphonActionCapabilityState::Disconnected);
    setState(TyphonConnectionState::Disconnected);
}

void TyphonToplevelConnection::bindAdapter(quint64 generation)
{
    disconnectAdapterSignals();
    if (!m_adapter)
        return;

    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::registryDiscovered,
                                        this, [this, generation](bool available) {
        if (generation != m_generation || !m_started)
            return;
        if (m_adapter)
            setActionCapability(m_adapter->actionCapability());
        if (!available) {
            clearPublicSnapshot();
            setState(TyphonConnectionState::Unsupported);
            if (m_adapter)
                m_adapter->stop();
            return;
        }
        setState(TyphonConnectionState::WaitingForInitialSnapshot);
    }));
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::handleCreated,
                                        this, [this, generation](quint64 token) {
        if (generation == m_generation && m_started)
            handleResult(m_model.handleCreated(generation, token));
    }));
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::identifierChanged,
                                        this, [this, generation](quint64 token, const QString &id) {
        if (generation == m_generation && m_started)
            handleResult(m_model.identifierChanged(generation, token, id));
    }));
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::appIdChanged,
                                        this, [this, generation](quint64 token, const QString &appId) {
        if (generation == m_generation && m_started)
            handleResult(m_model.appIdChanged(generation, token, appId));
    }));
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::titleChanged,
                                        this, [this, generation](quint64 token, const QString &title) {
        if (generation == m_generation && m_started)
            handleResult(m_model.titleChanged(generation, token, title));
    }));
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::pidChanged,
                                        this, [this, generation](quint64 token, quint32 pid) {
        if (generation == m_generation && m_started)
            handleResult(m_model.pidChanged(generation, token, pid));
    }));
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::kindChanged,
                                        this, [this, generation](quint64 token, ToplevelKind kind) {
        if (generation == m_generation && m_started)
            handleResult(m_model.kindChanged(generation, token, kind));
    }));
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::stateChanged,
                                        this, [this, generation](quint64 token, ToplevelStates states,
                                                                  quint32 rawStateBits) {
        if (generation == m_generation && m_started)
            handleResult(m_model.stateChanged(generation, token, states, rawStateBits));
    }));
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::focusSerialChanged,
                                        this, [this, generation](quint64 token, FocusSerial serial) {
        if (generation == m_generation && m_started)
            handleResult(m_model.focusSerialChanged(generation, token, serial));
    }));
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::handleCompleted,
                                        this, [this, generation](quint64 token, Revision revision) {
        if (generation == m_generation && m_started)
            handleResult(m_model.handleDone(generation, token, revision));
    }));
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::handleClosed,
                                        this, [this, generation](quint64 token) {
        if (generation == m_generation && m_started)
            handleResult(m_model.handleClosed(generation, token));
    }));
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::managerCompleted,
                                        this, [this, generation](Revision revision, quint32 total,
                                                                 bool truncated) {
        if (generation == m_generation && m_started)
            handleResult(m_model.managerDone(generation, revision, total, truncated));
    }));
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::managerFailed,
                                        this, [this, generation](const QString &diagnostic) {
        if (generation == m_generation && m_started) {
            const auto result = m_model.managerFailed(generation);
            if (result == TyphonToplevelModel::EventResult::Rejected)
                enterDegraded(diagnostic.isEmpty() ? QStringLiteral("Typhon manager failed") : diagnostic,
                              true);
        }
    }));
    m_adapterConnections.append(connect(
        m_adapter, &TyphonProtocolAdapter::actionCapabilityChanged, this,
        [this, generation](TyphonActionCapabilityState capability) {
            if (generation == m_generation && m_started)
                setActionCapability(capability);
        }));
    m_adapterConnections.append(connect(
        m_adapter, &TyphonProtocolAdapter::actionCompleted, this,
        [this, generation](quint32 tokenHi, quint32 tokenLo, ToplevelAction action,
                           ToplevelActionResult result) {
            if (generation != m_generation || !m_started)
                return;
            const auto pending = m_actionState.complete(
                generation, TyphonActionToken{tokenHi, tokenLo}, action, result);
            if (!pending.has_value()) {
                qWarning("Ignoring unknown or stale Typhon action completion");
                return;
            }
            emit actionFinished(pending->consumerToken, action, result);
        }));
    m_adapterConnections.append(connect(
        m_adapter, &TyphonProtocolAdapter::capabilityDiagnostic, this,
        [this, generation](const QString &diagnostic) {
            if (generation == m_generation && m_started)
                emit this->diagnostic(diagnostic);
        }));
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::displayDisconnected,
                                        this, [this, generation] {
        if (generation != m_generation || !m_started)
            return;
        if (m_sharedConnection) {
            settlePendingActions(ToplevelActionError::Disconnected);
            setActionCapability(TyphonActionCapabilityState::Disconnected);
            clearPublicSnapshot();
            setState(TyphonConnectionState::Disconnected);
            return;
        }
        settlePendingActions(ToplevelActionError::Disconnected);
        setActionCapability(TyphonActionCapabilityState::Disconnected);
        setState(TyphonConnectionState::Disconnected);
        disconnectAdapterSignals();
        if (m_adapter)
            m_adapter->stop();
        clearPublicSnapshot();
        scheduleReconnect();
    }));
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::protocolError,
                                        this, [this, generation](const QString &diagnostic) {
        if (generation == m_generation && m_started) {
            settlePendingActions(ToplevelActionError::Disconnected);
            setActionCapability(TyphonActionCapabilityState::Degraded);
            enterDegraded(diagnostic, true);
        }
    }));
}

std::optional<ToplevelActionError> TyphonToplevelConnection::requestAction(
    const QString &windowId, ToplevelAction action, quint64 consumerToken)
{
    if (!m_started || !m_adapter)
        return ToplevelActionError::Disconnected;

    switch (m_actionCapability) {
    case TyphonActionCapabilityState::Disconnected:
        return ToplevelActionError::Disconnected;
    case TyphonActionCapabilityState::ReadOnlyV1:
        return ToplevelActionError::UnsupportedProtocol;
    case TyphonActionCapabilityState::ReadOnlyV2:
    case TyphonActionCapabilityState::AuthenticatingV2:
        return ToplevelActionError::NotAuthenticated;
    case TyphonActionCapabilityState::Degraded:
        return ToplevelActionError::Disconnected;
    case TyphonActionCapabilityState::ActionReadyV2:
        break;
    }

    const auto handleToken = m_model.handleTokenForWindowId(windowId);
    if (!handleToken.has_value())
        return ToplevelActionError::ToplevelNotLive;

    const TyphonActionToken token = m_actionState.nextToken(m_generation);
    const TyphonActionAdmission admission = m_actionState.reserve(
        m_generation, token, windowId, action, consumerToken);
    if (admission != TyphonActionAdmission::Accepted)
        return ToplevelActionError::LocalCapacityExceeded;

    const auto localError = m_adapter->requestAction(*handleToken, token, action);
    if (localError.has_value()) {
        m_actionState.discard(m_generation, token, action);
        return localError;
    }
    qInfo("Typhon action submitted: generation %llu, window %s, action %d",
          static_cast<unsigned long long>(m_generation), qPrintable(windowId),
          static_cast<int>(action));
    return std::nullopt;
}

void TyphonToplevelConnection::disconnectAdapterSignals()
{
    for (const QMetaObject::Connection &connection : std::as_const(m_adapterConnections))
        disconnect(connection);
    m_adapterConnections.clear();
}

void TyphonToplevelConnection::setState(TyphonConnectionState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void TyphonToplevelConnection::handleResult(TyphonToplevelModel::EventResult result)
{
    if (result == TyphonToplevelModel::EventResult::Rejected)
        enterDegraded(m_model.lastError(), true);
}

void TyphonToplevelConnection::clearPublicSnapshot()
{
    const bool hadSnapshot = m_publicSnapshotPublished;
    m_model.clearSnapshot(m_generation);
    m_publicSnapshotPublished = false;
    if (hadSnapshot)
        emit snapshotChanged({});
}

void TyphonToplevelConnection::enterDegraded(const QString &message, bool reconnect)
{
    settlePendingActions(ToplevelActionError::Disconnected);
    setActionCapability(TyphonActionCapabilityState::Degraded);
    if (m_state != TyphonConnectionState::Degraded)
        setState(TyphonConnectionState::Degraded);
    emit diagnostic(message);
    qWarning("Typhon connection degraded: %s", qPrintable(message));
    disconnectAdapterSignals();
    if (m_adapter)
        m_adapter->stop();
    clearPublicSnapshot();
    if (reconnect)
        scheduleReconnect();
}

void TyphonToplevelConnection::settlePendingActions(ToplevelActionError error)
{
    const QVector<TyphonPendingAction> pending = m_actionState.clearGeneration(m_generation);
    for (const TyphonPendingAction &action : pending)
        emit actionFailed(action.consumerToken, action.action, error);
}

void TyphonToplevelConnection::setActionCapability(TyphonActionCapabilityState state)
{
    if (m_actionCapability == state)
        return;
    m_actionCapability = state;
    emit actionCapabilityChanged(state);
}

int TyphonToplevelConnection::reconnectDelay() const
{
    static constexpr int delays[] = {250, 500, 1000, 2000, 5000};
    return delays[qBound(0, m_backoffIndex, 4)];
}

void TyphonToplevelConnection::scheduleReconnect()
{
    if (!m_started || m_reconnectTimer.isActive())
        return;
    const int delay = reconnectDelay();
    m_reconnectTimer.start(delay);
    m_backoffIndex = qMin(m_backoffIndex + 1, 4);
    qInfo("Typhon reconnect scheduled in %d ms", delay);
}
