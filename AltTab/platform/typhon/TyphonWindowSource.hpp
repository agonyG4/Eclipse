#pragma once

#include "platform/compositor/CompositorBackend.hpp"
#include "platform/typhon/TyphonToplevelConnection.hpp"

#include <optional>

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

    static bool protocolAvailableOnCurrentDisplay();
    WindowSnapshot mapSnapshotForTest(const Astrea::Typhon::Snapshot &snapshot) const;

private:
    void onConnectionStateChanged(TyphonConnectionState state);
    void onConnectionSnapshot(const Astrea::Typhon::Snapshot &snapshot);
    void setBackendState(BackendState state);
    static BackendState mapState(TyphonConnectionState state);

    TyphonToplevelConnection *m_connection = nullptr;
    WindowSnapshot m_cachedSnapshot;
    BackendState m_state = BackendState::Stopped;
    bool m_started = false;
    bool m_hasSnapshot = false;
};
