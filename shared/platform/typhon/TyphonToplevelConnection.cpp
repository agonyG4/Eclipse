#include "platform/typhon/TyphonToplevelConnection.hpp"

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
    beginConnection();
}

void TyphonToplevelConnection::stop()
{
    if (!m_started && m_state == TyphonConnectionState::Stopped)
        return;
    m_started = false;
    m_reconnectTimer.stop();
    disconnectAdapterSignals();
    if (m_adapter)
        m_adapter->stop();
    clearPublicSnapshot();
    setState(TyphonConnectionState::Stopped);
}

void TyphonToplevelConnection::beginConnection()
{
    if (!m_started)
        return;
    ++m_generation;
    m_model.startGeneration(m_generation);
    setState(TyphonConnectionState::Connecting);
    setState(TyphonConnectionState::WaitingForRegistry);
    bindAdapter(m_generation);
    qInfo("Typhon connection attempt generation %llu",
          static_cast<unsigned long long>(m_generation));
    if (m_adapter)
        m_adapter->start();
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
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::displayDisconnected,
                                        this, [this, generation] {
        if (generation != m_generation || !m_started)
            return;
        setState(TyphonConnectionState::Disconnected);
        disconnectAdapterSignals();
        if (m_adapter)
            m_adapter->stop();
        clearPublicSnapshot();
        scheduleReconnect();
    }));
    m_adapterConnections.append(connect(m_adapter, &TyphonProtocolAdapter::protocolError,
                                        this, [this, generation](const QString &diagnostic) {
        if (generation == m_generation && m_started)
            enterDegraded(diagnostic, true);
    }));
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
