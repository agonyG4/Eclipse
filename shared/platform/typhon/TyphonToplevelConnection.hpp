#pragma once

#include "platform/typhon/TyphonProtocolAdapter.hpp"
#include "platform/typhon/TyphonActionState.hpp"
#include "platform/typhon/TyphonToplevelModel.hpp"

#include <QObject>
#include <QTimer>

class TyphonSharedConnection;

enum class TyphonConnectionState {
    Stopped,
    Connecting,
    WaitingForRegistry,
    WaitingForInitialSnapshot,
    Ready,
    Degraded,
    Disconnected,
    Unsupported
};
Q_DECLARE_METATYPE(TyphonConnectionState)

class TyphonToplevelConnection final : public QObject {
    Q_OBJECT

public:
    explicit TyphonToplevelConnection(TyphonProtocolAdapter *adapter = nullptr,
                                      QObject *parent = nullptr);
    explicit TyphonToplevelConnection(TyphonSharedConnection *sharedConnection,
                                      QObject *parent = nullptr);
    ~TyphonToplevelConnection() override;

    void start();
    void stop();

    TyphonConnectionState state() const { return m_state; }
    bool hasSnapshot() const { return m_model.hasCommittedSnapshot(); }
    const Astrea::Typhon::Snapshot &snapshot() const { return m_model.snapshot(); }
    quint64 connectionGeneration() const { return m_generation; }
    bool reconnectPending() const { return m_reconnectTimer.isActive(); }
    Astrea::Typhon::TyphonActionCapabilityState actionCapability() const
    { return m_actionCapability; }
    quint32 managerVersion() const
    { return m_adapter ? m_adapter->managerVersion() : 0; }
    std::optional<Astrea::Typhon::ToplevelActionError> requestAction(
        const QString &windowId, Astrea::Typhon::ToplevelAction action, quint64 consumerToken);

signals:
    void stateChanged(TyphonConnectionState state);
    void snapshotChanged(Astrea::Typhon::Snapshot snapshot);
    void diagnostic(QString message);
    void actionCapabilityChanged(Astrea::Typhon::TyphonActionCapabilityState state);
    void actionFinished(quint64 consumerToken, Astrea::Typhon::ToplevelAction action,
                        Astrea::Typhon::ToplevelActionResult result);
    void actionFailed(quint64 consumerToken, Astrea::Typhon::ToplevelAction action,
                      Astrea::Typhon::ToplevelActionError error);

private:
    void beginConnection();
    void bindAdapter(quint64 generation);
    void disconnectAdapterSignals();
    void setState(TyphonConnectionState state);
    void handleResult(TyphonToplevelModel::EventResult result);
    void clearPublicSnapshot();
    void enterDegraded(const QString &message, bool reconnect);
    void scheduleReconnect();
    int reconnectDelay() const;
    void settlePendingActions(Astrea::Typhon::ToplevelActionError error);
    void setActionCapability(Astrea::Typhon::TyphonActionCapabilityState state);
    void beginSharedGeneration(quint64 generation);
    void handleSharedDisconnected(quint64 generation);

    TyphonProtocolAdapter *m_adapter = nullptr;
    TyphonSharedConnection *m_sharedConnection = nullptr;
    TyphonToplevelModel m_model;
    QTimer m_reconnectTimer;
    QVector<QMetaObject::Connection> m_adapterConnections;
    TyphonConnectionState m_state = TyphonConnectionState::Stopped;
    quint64 m_generation = 0;
    int m_backoffIndex = 0;
    bool m_started = false;
    bool m_publicSnapshotPublished = false;
    Astrea::Typhon::TyphonActionCapabilityState m_actionCapability =
        Astrea::Typhon::TyphonActionCapabilityState::Disconnected;
    Astrea::Typhon::TyphonActionState m_actionState;
};
