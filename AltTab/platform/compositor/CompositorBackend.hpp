#pragma once

#include "core/CompositorTypes.hpp"
#include "core/WindowInfo.hpp"
#include <QObject>
#include <QVector>
#include <QString>
#include <optional>

enum class BackendState {
    Stopped,
    Starting,
    ConnectingEvents,
    LoadingInitialSnapshot,
    Ready,
    Degraded,
    Disconnected,
    Unsupported
};

enum class BackendCapability {
    WindowList,
    EventStream,
    WindowActivation,
    ActiveWindow,
    ActiveOutput
};

struct BackendDescriptor {
    QString name;
    int protocolVersion = 1;
};

struct WindowSnapshot {
    QVector<WindowInfo> windows;
    WindowId activeWindowId;
    WorkspaceId activeWorkspaceId;
    OutputId activeOutputId;
    QString focusedMonitorName;
    int revision = 0;
    quint64 backendGeneration = 0;
};

using RequestToken = quint64;
using ActivationToken = quint64;

struct ActivationRequest {
    WindowId windowId;
    ActivationToken token = 0;
};

struct ActivationResult {
    bool success = false;
    QString error;
};

enum class BackendError {
    ConnectionFailed,
    ProtocolError,
    CompositorRejected,
    Unknown
};

class CompositorBackend : public QObject {
    Q_OBJECT
public:
    explicit CompositorBackend(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~CompositorBackend() = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    virtual BackendDescriptor descriptor() const = 0;
    virtual BackendState state() const = 0;
    virtual QVector<BackendCapability> capabilities() const = 0;

    virtual std::optional<WindowSnapshot> cachedSnapshot() const = 0;
    virtual void requestSnapshot(RequestToken token) = 0;
    virtual void activateWindow(ActivationRequest request) = 0;

signals:
    void stateChanged(BackendState state);
    void snapshotReady(RequestToken token, WindowSnapshot snapshot);
    void snapshotChanged(WindowSnapshot snapshot);
    void activationFinished(ActivationToken token, ActivationResult result);
    void backendError(BackendError error);
};
