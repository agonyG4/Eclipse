#pragma once

#include "platform/compositor/CompositorBackend.hpp"
#include "platform/typhon/TyphonToplevelConnection.hpp"

#include <optional>
#include <QSet>
#include <QVector>

class TyphonWindowSource final : public CompositorBackend {
    Q_OBJECT

public:
    explicit TyphonWindowSource(TyphonToplevelConnection *connection = nullptr,
                                QObject *parent = nullptr);
    ~TyphonWindowSource() override;

    void start() override;
    void stop() override;

    BackendDescriptor descriptor() const override;
    BackendState state() const override;
    QVector<BackendCapability> capabilities() const override;

    std::optional<WindowSnapshot> cachedSnapshot() const override;
    void requestSnapshot(RequestToken token) override;
    void activateWindow(ActivationRequest request) override;

    WindowSnapshot mapSnapshotForTest(const Astrea::Typhon::Snapshot &snapshot) const;

private:
    static constexpr qsizetype kMaxPendingSnapshotRequests = 16;

    struct PendingSnapshotRequest {
        RequestToken token = 0;
        quint64 connectionGeneration = 0;
    };

    void onConnectionStateChanged(TyphonConnectionState state);
    void onConnectionSnapshot(const Astrea::Typhon::Snapshot &snapshot);
    void setBackendState(BackendState state);
    void failPendingSnapshotRequests();
    void failStalePendingSnapshotRequests(quint64 connectionGeneration);
    static BackendState mapState(TyphonConnectionState state);

    TyphonToplevelConnection *m_connection = nullptr;
    WindowSnapshot m_cachedSnapshot;
    BackendState m_state = BackendState::Stopped;
    bool m_started = false;
    bool m_hasSnapshot = false;
    quint64 m_observedConnectionGeneration = 0;
    QVector<PendingSnapshotRequest> m_pendingSnapshotRequests;
    QSet<RequestToken> m_resolvedSnapshotTokens;
};
