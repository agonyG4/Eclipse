#include "platform/compositor/AutoCompositorBackend.hpp"

#include "platform/hyprland/HyprlandWindowSource.hpp"
#include "platform/typhon/TyphonWindowSource.hpp"

#include <QProcessEnvironment>

#include <utility>

namespace {

bool typhonBackendCompiled()
{
#if ASTREA_HAVE_TYPHON_PROTOCOL
    return true;
#else
    return false;
#endif
}

bool hyprlandEnvironmentAvailable()
{
    return !QProcessEnvironment::systemEnvironment()
                .value(QStringLiteral("HYPRLAND_INSTANCE_SIGNATURE"))
                .trimmed()
                .isEmpty();
}

CompositorBackend *createDefaultCandidate(AutoBackendCandidate candidate, QObject *parent)
{
    switch (candidate) {
    case AutoBackendCandidate::Typhon:
        return new TyphonWindowSource(nullptr, parent);
    case AutoBackendCandidate::Hyprland:
        return new HyprlandWindowSource(parent);
    }
    return nullptr;
}

} // namespace

AutoCompositorBackend::AutoCompositorBackend(Dependencies dependencies, QObject *parent)
    : CompositorBackend(parent), m_dependencies(std::move(dependencies))
{
    if (!m_dependencies.typhonCompiled)
        m_dependencies.typhonCompiled = typhonBackendCompiled;
    if (!m_dependencies.hyprlandAvailable)
        m_dependencies.hyprlandAvailable = hyprlandEnvironmentAvailable;
    if (!m_dependencies.createCandidate)
        m_dependencies.createCandidate = createDefaultCandidate;
}

AutoCompositorBackend::~AutoCompositorBackend()
{
    stop();
}

void AutoCompositorBackend::start()
{
    if (m_started)
        return;

    m_started = true;
    ++m_selectionGeneration;
    m_selectedCandidate.reset();
    setBackendState(BackendState::Starting);

    if (m_dependencies.typhonCompiled())
        startTyphonCandidate();
    else
        startHyprlandCandidate();
}

void AutoCompositorBackend::stop()
{
    if (!m_started && m_state == BackendState::Stopped && !m_candidate)
        return;

    m_started = false;
    ++m_selectionGeneration;
    m_selectedCandidate.reset();
    disconnectCandidateSignals();
    if (m_candidate)
        m_candidate->stop();
    m_candidate.reset();
    m_pendingSnapshotRequests.clear();
    setBackendState(BackendState::Stopped);
}

BackendDescriptor AutoCompositorBackend::descriptor() const
{
    if (m_selectedCandidate.has_value() && m_candidate)
        return m_candidate->descriptor();
    return {QStringLiteral("auto"), 1};
}

BackendState AutoCompositorBackend::state() const
{
    return m_state;
}

QVector<BackendCapability> AutoCompositorBackend::capabilities() const
{
    if (m_selectedCandidate.has_value() && m_candidate)
        return m_candidate->capabilities();
    return {};
}

std::optional<WindowSnapshot> AutoCompositorBackend::cachedSnapshot() const
{
    if (m_selectedCandidate.has_value() && m_candidate)
        return m_candidate->cachedSnapshot();
    return std::nullopt;
}

void AutoCompositorBackend::requestSnapshot(RequestToken token)
{
    if (m_selectedCandidate.has_value() && m_candidate) {
        m_candidate->requestSnapshot(token);
        return;
    }

    static constexpr int kMaxPendingSnapshotRequests = 16;
    if (m_pendingSnapshotRequests.size() >= kMaxPendingSnapshotRequests) {
        emit snapshotReady(token, {});
        emit backendError(BackendError::ConnectionFailed);
        return;
    }
    m_pendingSnapshotRequests.append(token);
}

void AutoCompositorBackend::activateWindow(ActivationRequest request)
{
    if (m_selectedCandidate.has_value() && m_candidate) {
        m_candidate->activateWindow(request);
        return;
    }
    failActivation(request, QStringLiteral("auto backend selection is pending"));
}

void AutoCompositorBackend::startTyphonCandidate()
{
    if (!m_started)
        return;
    m_candidate.reset(m_dependencies.createCandidate(AutoBackendCandidate::Typhon, nullptr));
    if (!m_candidate) {
        startHyprlandCandidate();
        return;
    }
    connectCandidate(AutoBackendCandidate::Typhon);
    m_candidate->start();
}

void AutoCompositorBackend::startHyprlandCandidate()
{
    if (!m_started)
        return;
    if (!m_dependencies.hyprlandAvailable()) {
        failPendingSnapshotRequests();
        setBackendState(BackendState::Unsupported);
        return;
    }

    m_candidate.reset(m_dependencies.createCandidate(AutoBackendCandidate::Hyprland, nullptr));
    if (!m_candidate) {
        failPendingSnapshotRequests();
        setBackendState(BackendState::Unsupported);
        return;
    }
    connectCandidate(AutoBackendCandidate::Hyprland);
    m_candidate->start();
}

void AutoCompositorBackend::connectCandidate(AutoBackendCandidate candidate)
{
    disconnectCandidateSignals();
    if (!m_candidate)
        return;

    const quint64 generation = m_selectionGeneration;
    const QPointer<CompositorBackend> observedCandidate(m_candidate.get());
    m_candidateConnections.append(connect(
        m_candidate.get(), &CompositorBackend::stateChanged, this,
        [this, candidate, generation, observedCandidate](BackendState state) {
            if (candidateIsCurrent(generation, observedCandidate))
                handleCandidateState(candidate, state);
        }));
    m_candidateConnections.append(connect(
        m_candidate.get(), &CompositorBackend::snapshotReady, this,
        [this, generation, observedCandidate](RequestToken token, WindowSnapshot snapshot) {
            if (candidateIsCurrent(generation, observedCandidate))
                emit snapshotReady(token, snapshot);
        }));
    m_candidateConnections.append(connect(
        m_candidate.get(), &CompositorBackend::snapshotChanged, this,
        [this, generation, observedCandidate](WindowSnapshot snapshot) {
            if (candidateIsCurrent(generation, observedCandidate))
                emit snapshotChanged(snapshot);
        }));
    m_candidateConnections.append(connect(
        m_candidate.get(), &CompositorBackend::activationFinished, this,
        [this, generation, observedCandidate](ActivationToken token, ActivationResult result) {
            if (candidateIsCurrent(generation, observedCandidate))
                emit activationFinished(token, result);
        }));
    m_candidateConnections.append(connect(
        m_candidate.get(), &CompositorBackend::backendError, this,
        [this, generation, observedCandidate](BackendError error) {
            if (candidateIsCurrent(generation, observedCandidate))
                emit backendError(error);
        }));
}

void AutoCompositorBackend::disconnectCandidateSignals()
{
    for (const QMetaObject::Connection &connection : std::as_const(m_candidateConnections))
        disconnect(connection);
    m_candidateConnections.clear();
}

void AutoCompositorBackend::selectCandidate(AutoBackendCandidate candidate)
{
    if (!m_started || !m_candidate || m_selectedCandidate.has_value())
        return;
    m_selectedCandidate = candidate;
    setBackendState(m_candidate->state());

    const QVector<RequestToken> pending = std::exchange(m_pendingSnapshotRequests, {});
    for (const RequestToken token : pending)
        m_candidate->requestSnapshot(token);
}

void AutoCompositorBackend::discardCandidate()
{
    disconnectCandidateSignals();
    if (m_candidate)
        m_candidate->stop();
    m_candidate.reset();
    m_selectedCandidate.reset();
}

void AutoCompositorBackend::handleCandidateState(AutoBackendCandidate candidate,
                                                 BackendState candidateState)
{
    if (m_selectedCandidate.has_value()) {
        setBackendState(candidateState);
        return;
    }

    if (candidate == AutoBackendCandidate::Typhon) {
        if (candidateState == BackendState::LoadingInitialSnapshot
            || candidateState == BackendState::Ready) {
            selectCandidate(candidate);
            return;
        }
        if (candidateState == BackendState::Unsupported) {
            discardCandidate();
            startHyprlandCandidate();
            return;
        }
        if (candidateState == BackendState::Disconnected
            && m_dependencies.hyprlandAvailable()) {
            discardCandidate();
            startHyprlandCandidate();
            return;
        }
    }

    if (candidate == AutoBackendCandidate::Hyprland && candidateState == BackendState::Ready) {
        selectCandidate(candidate);
        return;
    }

    setBackendState(candidateState);
}

bool AutoCompositorBackend::candidateIsCurrent(
    quint64 generation, const QPointer<CompositorBackend> &candidate) const
{
    return m_started && generation == m_selectionGeneration && m_candidate
        && m_candidate.get() == candidate.data();
}

void AutoCompositorBackend::setBackendState(BackendState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(m_state);
}

void AutoCompositorBackend::failPendingSnapshotRequests()
{
    const QVector<RequestToken> pending = std::exchange(m_pendingSnapshotRequests, {});
    for (const RequestToken token : pending) {
        emit snapshotReady(token, {});
        emit backendError(BackendError::ConnectionFailed);
    }
}

void AutoCompositorBackend::failActivation(ActivationRequest request, const QString &error)
{
    emit activationFinished(request.token, ActivationResult{false, error});
}
