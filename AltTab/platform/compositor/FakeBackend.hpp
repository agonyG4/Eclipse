#pragma once

#include "platform/compositor/CompositorBackend.hpp"

class FakeWindowSource : public CompositorBackend {
    Q_OBJECT
public:
    explicit FakeWindowSource(const QVector<WindowInfo> &windows, QObject *parent = nullptr)
        : CompositorBackend(parent), m_windows(windows) {}

    void start() override {
        m_state = BackendState::Ready;
        emit stateChanged(m_state);
    }
    void stop() override {
        m_state = BackendState::Stopped;
        emit stateChanged(m_state);
    }

    BackendDescriptor descriptor() const override {
        return { QStringLiteral("fake"), 1 };
    }
    BackendState state() const override {
        return m_state;
    }
    QVector<BackendCapability> capabilities() const override {
        return { BackendCapability::WindowList, BackendCapability::WindowActivation,
                 BackendCapability::ActiveWindow, BackendCapability::ActiveOutput };
    }

    std::optional<WindowSnapshot> cachedSnapshot() const override {
        if (m_delaySnapshot || m_windows.isEmpty())
            return std::nullopt;
        WindowSnapshot snap;
        snap.windows = m_windows;
        snap.revision = m_revision;
        return snap;
    }

    void requestSnapshot(RequestToken token) override {
        if (m_delaySnapshot) {
            m_pendingToken = token;
            return;
        }
        deliverSnapshot(token);
    }

    void activateWindow(ActivationRequest request) override {
        m_activationRequests.append(request);
        if (m_autoCompleteActivation) {
            m_lastActivated = request.windowId;
            ActivationResult res;
            res.success = m_activationSuccess;
            res.error = m_activationError;
            emit activationFinished(request.token, res);
        }
    }

    void setWindows(const QVector<WindowInfo> &windows) {
        m_windows = windows;
        WindowSnapshot snap;
        snap.windows = m_windows;
        snap.revision = ++m_revision;
        emit snapshotChanged(snap);
    }

    WindowId lastActivatedWindowId() const {
        return m_activationRequests.isEmpty() ? WindowId{} : m_lastActivated;
    }

    // Test control
    void setDelaySnapshot(bool delay) { m_delaySnapshot = delay; }
    void deliverSnapshot(RequestToken token) {
        WindowSnapshot snap;
        snap.windows = m_windows;
        snap.revision = ++m_revision;
        snap.activeWindowId = m_activeWindowId;
        snap.activeWorkspaceId = WorkspaceId{QString::number(m_activeWorkspace)};
        snap.activeOutputId = m_activeOutput;
        emit snapshotReady(token, snap);
    }
    void deliverPendingSnapshot() {
        if (m_pendingToken > 0)
            deliverSnapshot(m_pendingToken);
        m_pendingToken = 0;
    }
    void setAutoCompleteActivation(bool autoComplete) { m_autoCompleteActivation = autoComplete; }
    void setActivationResult(bool success, const QString &error = {}) {
        m_activationSuccess = success;
        m_activationError = error;
    }
    void completeActivation(ActivationToken token, bool success, const QString &error = {}) {
        m_lastActivated = m_activationRequests.isEmpty() ? WindowId{} : m_activationRequests.last().windowId;
        ActivationResult res;
        res.success = success;
        res.error = error;
        emit activationFinished(token, res);
    }
    void disconnectManually() {
        m_state = BackendState::Disconnected;
        emit stateChanged(m_state);
    }
    void emitBackendError(BackendError error) {
        emit backendError(error);
    }
    int activationCount() const { return m_activationRequests.size(); }
    void setActiveWindow(const WindowId &id) { m_activeWindowId = id; }
    void setActiveWorkspace(int ws) { m_activeWorkspace = ws; }
    void setActiveOutput(const OutputId &output) { m_activeOutput = output; }

private:
    QVector<WindowInfo> m_windows;
    BackendState m_state = BackendState::Ready;
    int m_revision = 0;
    WindowId m_lastActivated;
    WindowId m_activeWindowId;
    int m_activeWorkspace = 1;
    OutputId m_activeOutput{QStringLiteral("eDP-1")};
    bool m_delaySnapshot = false;
    RequestToken m_pendingToken = 0;
    bool m_autoCompleteActivation = true;
    bool m_activationSuccess = true;
    QString m_activationError;
    QVector<ActivationRequest> m_activationRequests;
};
