#pragma once

#include "platform/compositor/CompositorBackend.hpp"

#include <QMetaObject>
#include <QPointer>

#include <functional>
#include <memory>
#include <optional>

enum class AutoBackendCandidate {
    Typhon,
    Hyprland
};
class AutoCompositorBackend final : public CompositorBackend {
    Q_OBJECT

public:
    struct Dependencies {
        std::function<bool()> typhonCompiled;
        std::function<bool()> hyprlandAvailable;
        std::function<CompositorBackend *(AutoBackendCandidate, QObject *)> createCandidate;
    };

    explicit AutoCompositorBackend(Dependencies dependencies = {}, QObject *parent = nullptr);
    ~AutoCompositorBackend() override;

    void start() override;
    void stop() override;

    BackendDescriptor descriptor() const override;
    BackendState state() const override;
    QVector<BackendCapability> capabilities() const override;

    std::optional<WindowSnapshot> cachedSnapshot() const override;
    void requestSnapshot(RequestToken token) override;
    void activateWindow(ActivationRequest request) override;

private:
    void startTyphonCandidate();
    void startHyprlandCandidate();
    void connectCandidate(AutoBackendCandidate candidate);
    void disconnectCandidateSignals();
    void selectCandidate(AutoBackendCandidate candidate);
    void discardCandidate();
    void handleCandidateState(AutoBackendCandidate candidate, BackendState state);
    bool candidateIsCurrent(quint64 generation,
                            const QPointer<CompositorBackend> &candidate) const;
    void setBackendState(BackendState state);
    void failPendingSnapshotRequests();
    void failActivation(ActivationRequest request, const QString &error);

    Dependencies m_dependencies;
    std::unique_ptr<CompositorBackend> m_candidate;
    std::optional<AutoBackendCandidate> m_selectedCandidate;
    QVector<QMetaObject::Connection> m_candidateConnections;
    QVector<RequestToken> m_pendingSnapshotRequests;
    BackendState m_state = BackendState::Stopped;
    quint64 m_selectionGeneration = 0;
    bool m_started = false;
};
