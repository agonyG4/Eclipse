#pragma once

#include "platform/compositor/CompositorBackend.hpp"
#include "platform/hyprland/HyprlandSocket.hpp"
#include <QTimer>
#include <optional>

class HyprlandWindowSource : public CompositorBackend {
    Q_OBJECT
public:
    explicit HyprlandWindowSource(QObject *parent = nullptr);
    ~HyprlandWindowSource() override;

    void start() override;
    void stop() override;

    BackendDescriptor descriptor() const override;
    BackendState state() const override;
    QVector<BackendCapability> capabilities() const override;

    std::optional<WindowSnapshot> cachedSnapshot() const override;
    void requestSnapshot(RequestToken token) override;
    void activateWindow(ActivationRequest request) override;

private slots:
    void onEvent(const QString &eventLine);
    void onConnectionLost();
    void triggerDebouncedRefresh();

private:
    void connectSockets();
    void requestInitialSnapshot();
    QVector<WindowInfo> parseClientsFromJson(const QByteArray &data);
    void updateSnapshotStateFromWindows(const QVector<WindowInfo> &windows);
    void setBackendState(BackendState state);

    HyprlandSocket *m_socket = nullptr;
    QTimer *m_debounceTimer = nullptr;
    WindowSnapshot m_cachedSnapshot;
    BackendState m_state = BackendState::Stopped;
    bool m_started = false;
    bool m_activationInFlight = false;
    quint64 m_lifecycleGeneration = 0;
    quint64 m_activationGeneration = 0;
};
