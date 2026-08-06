#include <QPointer>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTest>

#include <functional>
#include <memory>
#include <optional>

#include "platform/compositor/AutoCompositorBackend.hpp"
#include "platform/compositor/CompositorBackendFactory.hpp"
#include "platform/typhon/TyphonWindowSource.hpp"

using namespace Astrea::Typhon;

class AutoFakeTyphonAdapter final : public TyphonProtocolAdapter {
    Q_OBJECT

public:
    explicit AutoFakeTyphonAdapter(QObject *parent = nullptr)
        : TyphonProtocolAdapter(parent)
    {
    }

    void start() override { ++starts; }
    void stop() override { ++stops; }
    bool isAvailable() const override { return true; }

    void advertiseManager() { emit registryDiscovered(true); }
    void create(quint64 token) { emit handleCreated(token); }
    void id(quint64 token, const QString &value) { emit identifierChanged(token, value); }
    void app(quint64 token, const QString &value) { emit appIdChanged(token, value); }
    void title(quint64 token, const QString &value) { emit titleChanged(token, value); }
    void pid(quint64 token, quint32 value) { emit pidChanged(token, value); }
    void kind(quint64 token, ToplevelKind value) { emit kindChanged(token, value); }
    void state(quint64 token, ToplevelStates value) { emit stateChanged(token, value, 0); }
    void focus(quint64 token, FocusSerial value) { emit focusSerialChanged(token, value); }
    void handleDone(quint64 token, Revision value) { emit handleCompleted(token, value); }
    void done(Revision revision, quint32 total) { emit managerCompleted(revision, total, false); }

    int starts = 0;
    int stops = 0;
};

static void completeAutoTyphonSnapshot(AutoFakeTyphonAdapter &adapter, quint64 token,
                                       const QString &id, Revision revision)
{
    adapter.create(token);
    adapter.id(token, id);
    adapter.app(token, QStringLiteral("org.example.App"));
    adapter.title(token, QStringLiteral("Example"));
    adapter.pid(token, 100);
    adapter.kind(token, ToplevelKind::XdgToplevel);
    adapter.state(token, {});
    adapter.focus(token, 1);
    adapter.handleDone(token, revision);
    adapter.done(revision, 1);
}

class SelectionFakeBackend final : public CompositorBackend {
    Q_OBJECT
public:
    enum class Kind { Typhon, Hyprland };

    explicit SelectionFakeBackend(Kind kind, QObject *parent = nullptr)
        : CompositorBackend(parent), m_kind(kind)
    {
    }

    void start() override
    {
        ++startCount;
        active = true;
        setState(BackendState::Starting);
    }

    void stop() override
    {
        ++stopCount;
        ++totalStopCalls;
        emit snapshotChanged(staleSnapshot);
        active = false;
        setState(BackendState::Stopped);
    }

    BackendDescriptor descriptor() const override
    {
        return {m_kind == Kind::Typhon ? QStringLiteral("typhon") : QStringLiteral("hyprland"), 1};
    }

    BackendState state() const override { return m_state; }

    QVector<BackendCapability> capabilities() const override
    {
        if (m_kind == Kind::Typhon)
            return {BackendCapability::WindowList, BackendCapability::EventStream};
        return {BackendCapability::WindowList, BackendCapability::EventStream,
                BackendCapability::WindowActivation};
    }

    std::optional<WindowSnapshot> cachedSnapshot() const override
    {
        return m_cachedSnapshot;
    }

    void requestSnapshot(RequestToken token) override
    {
        requestedSnapshotTokens.append(token);
    }

    void activateWindow(ActivationRequest request) override
    {
        activationRequests.append(request);
    }

    void completeActivation(ActivationToken token)
    {
        emit activationFinished(token, ActivationResult{true, {}});
    }

    void emitError(BackendError error)
    {
        emit backendError(error);
    }

    void setState(BackendState state)
    {
        if (m_state == state)
            return;
        m_state = state;
        emit stateChanged(m_state);
    }

    void publishSnapshot(const WindowSnapshot &snapshot)
    {
        m_cachedSnapshot = snapshot;
        emit snapshotChanged(snapshot);
    }

    void completeSnapshot(RequestToken token, const WindowSnapshot &snapshot)
    {
        m_cachedSnapshot = snapshot;
        emit snapshotReady(token, snapshot);
    }

    Kind kind() const { return m_kind; }
    bool active = false;
    int startCount = 0;
    int stopCount = 0;
    static inline int totalStopCalls = 0;
    QVector<RequestToken> requestedSnapshotTokens;
    QVector<ActivationRequest> activationRequests;
    WindowSnapshot staleSnapshot;

private:
    Kind m_kind;
    BackendState m_state = BackendState::Stopped;
    std::optional<WindowSnapshot> m_cachedSnapshot;
};

class CompositorBackendFactoryTest final : public QObject {
    Q_OBJECT

private slots:
    void explicitTyphonCreatesReadOnlyBackend();
    void explicitHyprlandCreatesHyprlandBackend();
    void autoFactoryCreatesAsynchronousSelector();
    void autoSelectsTyphonAfterRuntimeDiscovery();
    void autoFallsBackWhenTyphonGlobalIsAbsent();
    void autoFallsBackWhenTyphonDisconnectsBeforeDiscovery();
    void autoFallsBackWhenTyphonIsNotCompiled();
    void autoReportsUnsupportedWhenNeitherIsAvailable();
    void explicitBackendsNeverFallbackThroughAuto();
    void selectedTyphonDoesNotSwitchAfterDisconnect();
    void discardedCandidateSignalsAreIgnored();
    void queuedSnapshotRequestFollowsSelectedBackend();
    void activationBeforeSelectionFailsDeterministically();
    void selectedBackendSignalsAndRequestsAreDelegated();
    void hyprlandCapabilitiesAppearAfterSelection();
    void stopStopsCandidateAndRestartStartsFreshGeneration();
    void autoBackendRetainsPendingTyphonRequestUntilSnapshot();
    void stress100AutoBackendPendingTyphonHandoffs();
};

namespace {

AutoCompositorBackend::Dependencies dependencies(
    bool typhonCompiled, bool hyprlandAvailable,
    const std::shared_ptr<QVector<QPointer<SelectionFakeBackend>>> &candidates)
{
    AutoCompositorBackend::Dependencies result;
    result.typhonCompiled = [typhonCompiled] { return typhonCompiled; };
    result.hyprlandAvailable = [hyprlandAvailable] { return hyprlandAvailable; };
    result.createCandidate = [candidates](AutoBackendCandidate candidate, QObject *parent) {
        const auto kind = candidate == AutoBackendCandidate::Typhon
            ? SelectionFakeBackend::Kind::Typhon : SelectionFakeBackend::Kind::Hyprland;
        auto *backend = new SelectionFakeBackend(kind, parent);
        candidates->append(backend);
        return static_cast<CompositorBackend *>(backend);
    };
    return result;
}

SelectionFakeBackend *candidateAt(
    const std::shared_ptr<QVector<QPointer<SelectionFakeBackend>>> &candidates, int index)
{
    return candidates->at(index).data();
}

} // namespace

void CompositorBackendFactoryTest::explicitTyphonCreatesReadOnlyBackend()
{
    std::unique_ptr<CompositorBackend> backend(
        CompositorBackendFactory::createBackend(QStringLiteral("typhon")));
    QVERIFY(backend);
    QCOMPARE(backend->descriptor().name, QStringLiteral("typhon"));
    QVERIFY(!backend->capabilities().contains(BackendCapability::WindowActivation));
}

void CompositorBackendFactoryTest::explicitHyprlandCreatesHyprlandBackend()
{
    std::unique_ptr<CompositorBackend> backend(
        CompositorBackendFactory::createBackend(QStringLiteral("hyprland")));
    QVERIFY(backend);
    QCOMPARE(backend->descriptor().name, QStringLiteral("hyprland"));
}

void CompositorBackendFactoryTest::autoFactoryCreatesAsynchronousSelector()
{
    std::unique_ptr<CompositorBackend> backend(
        CompositorBackendFactory::createBackend(QStringLiteral("auto")));
    QVERIFY(backend);
    QVERIFY(dynamic_cast<AutoCompositorBackend *>(backend.get()));
    QCOMPARE(backend->descriptor().name, QStringLiteral("auto"));
}

void CompositorBackendFactoryTest::autoSelectsTyphonAfterRuntimeDiscovery()
{
    auto candidates = std::make_shared<QVector<QPointer<SelectionFakeBackend>>>();
    AutoCompositorBackend backend(dependencies(true, true, candidates));
    backend.start();
    QCOMPARE(candidates->size(), 1);
    QCOMPARE(candidateAt(candidates, 0)->kind(), SelectionFakeBackend::Kind::Typhon);
    candidateAt(candidates, 0)->setState(BackendState::LoadingInitialSnapshot);
    QCOMPARE(backend.descriptor().name, QStringLiteral("typhon"));
    QCOMPARE(backend.state(), BackendState::LoadingInitialSnapshot);
    QVERIFY(!backend.capabilities().contains(BackendCapability::WindowActivation));
}

void CompositorBackendFactoryTest::autoFallsBackWhenTyphonGlobalIsAbsent()
{
    auto candidates = std::make_shared<QVector<QPointer<SelectionFakeBackend>>>();
    AutoCompositorBackend backend(dependencies(true, true, candidates));
    backend.start();
    QCOMPARE(candidates->size(), 1);
    candidateAt(candidates, 0)->setState(BackendState::Unsupported);
    QCOMPARE(candidates->size(), 2);
    QVERIFY(!candidates->at(0));
    QCOMPARE(candidateAt(candidates, 1)->kind(), SelectionFakeBackend::Kind::Hyprland);
    candidateAt(candidates, 1)->setState(BackendState::Ready);
    QCOMPARE(backend.descriptor().name, QStringLiteral("hyprland"));
    QCOMPARE(backend.state(), BackendState::Ready);
}

void CompositorBackendFactoryTest::autoFallsBackWhenTyphonIsNotCompiled()
{
    auto candidates = std::make_shared<QVector<QPointer<SelectionFakeBackend>>>();
    AutoCompositorBackend backend(dependencies(false, true, candidates));
    backend.start();
    QCOMPARE(candidates->size(), 1);
    QCOMPARE(candidateAt(candidates, 0)->kind(), SelectionFakeBackend::Kind::Hyprland);
}

void CompositorBackendFactoryTest::autoFallsBackWhenTyphonDisconnectsBeforeDiscovery()
{
    auto candidates = std::make_shared<QVector<QPointer<SelectionFakeBackend>>>();
    AutoCompositorBackend backend(dependencies(true, true, candidates));
    backend.start();
    candidateAt(candidates, 0)->setState(BackendState::Disconnected);
    QCOMPARE(candidates->size(), 2);
    QVERIFY(!candidates->at(0));
    QCOMPARE(candidateAt(candidates, 1)->kind(), SelectionFakeBackend::Kind::Hyprland);
}

void CompositorBackendFactoryTest::autoReportsUnsupportedWhenNeitherIsAvailable()
{
    auto candidates = std::make_shared<QVector<QPointer<SelectionFakeBackend>>>();
    AutoCompositorBackend backend(dependencies(false, false, candidates));
    QSignalSpy stateSpy(&backend, &CompositorBackend::stateChanged);
    backend.start();
    QVERIFY(candidates->isEmpty());
    QCOMPARE(backend.state(), BackendState::Unsupported);
    QVERIFY(!stateSpy.isEmpty());
}

void CompositorBackendFactoryTest::explicitBackendsNeverFallbackThroughAuto()
{
    std::unique_ptr<CompositorBackend> typhon(
        CompositorBackendFactory::createBackend(QStringLiteral("typhon")));
    std::unique_ptr<CompositorBackend> hyprland(
        CompositorBackendFactory::createBackend(QStringLiteral("hyprland")));
    QVERIFY(typhon);
    QVERIFY(hyprland);
    QVERIFY(!dynamic_cast<AutoCompositorBackend *>(typhon.get()));
    QVERIFY(!dynamic_cast<AutoCompositorBackend *>(hyprland.get()));
    QCOMPARE(typhon->descriptor().name, QStringLiteral("typhon"));
    QCOMPARE(hyprland->descriptor().name, QStringLiteral("hyprland"));
}

void CompositorBackendFactoryTest::selectedTyphonDoesNotSwitchAfterDisconnect()
{
    auto candidates = std::make_shared<QVector<QPointer<SelectionFakeBackend>>>();
    AutoCompositorBackend backend(dependencies(true, true, candidates));
    backend.start();
    candidateAt(candidates, 0)->setState(BackendState::Ready);
    candidateAt(candidates, 0)->setState(BackendState::Disconnected);
    QCOMPARE(candidates->size(), 1);
    QCOMPARE(backend.descriptor().name, QStringLiteral("typhon"));
    QCOMPARE(backend.state(), BackendState::Disconnected);
}

void CompositorBackendFactoryTest::discardedCandidateSignalsAreIgnored()
{
    auto candidates = std::make_shared<QVector<QPointer<SelectionFakeBackend>>>();
    AutoCompositorBackend backend(dependencies(true, true, candidates));
    QSignalSpy snapshots(&backend, &CompositorBackend::snapshotChanged);
    backend.start();
    candidateAt(candidates, 0)->staleSnapshot.revision = 99;
    candidateAt(candidates, 0)->setState(BackendState::Unsupported);
    QCOMPARE(snapshots.count(), 0);
}

void CompositorBackendFactoryTest::queuedSnapshotRequestFollowsSelectedBackend()
{
    auto candidates = std::make_shared<QVector<QPointer<SelectionFakeBackend>>>();
    AutoCompositorBackend backend(dependencies(true, true, candidates));
    backend.requestSnapshot(42);
    backend.start();
    candidateAt(candidates, 0)->setState(BackendState::LoadingInitialSnapshot);
    QCOMPARE(candidateAt(candidates, 0)->requestedSnapshotTokens, QVector<RequestToken>{42});
}

void CompositorBackendFactoryTest::activationBeforeSelectionFailsDeterministically()
{
    auto candidates = std::make_shared<QVector<QPointer<SelectionFakeBackend>>>();
    AutoCompositorBackend backend(dependencies(true, true, candidates));
    QSignalSpy activationSpy(&backend, &CompositorBackend::activationFinished);
    backend.start();
    backend.activateWindow({WindowId{QStringLiteral("1")}, 7});
    QCOMPARE(activationSpy.count(), 1);
    const ActivationResult result = activationSpy.at(0).at(1).value<ActivationResult>();
    QVERIFY(!result.success);
    QCOMPARE(result.error, QStringLiteral("auto backend selection is pending"));
}

void CompositorBackendFactoryTest::selectedBackendSignalsAndRequestsAreDelegated()
{
    auto candidates = std::make_shared<QVector<QPointer<SelectionFakeBackend>>>();
    AutoCompositorBackend backend(dependencies(false, true, candidates));
    QSignalSpy readySpy(&backend, &CompositorBackend::snapshotReady);
    QSignalSpy changedSpy(&backend, &CompositorBackend::snapshotChanged);
    QSignalSpy activationSpy(&backend, &CompositorBackend::activationFinished);
    QSignalSpy errorSpy(&backend, &CompositorBackend::backendError);
    backend.start();
    auto *hyprland = candidateAt(candidates, 0);
    hyprland->setState(BackendState::Ready);

    WindowSnapshot snapshot;
    snapshot.revision = 5;
    hyprland->publishSnapshot(snapshot);
    QCOMPARE(changedSpy.count(), 1);
    QVERIFY(backend.cachedSnapshot().has_value());
    QCOMPARE(backend.cachedSnapshot()->revision, quint64(5));

    backend.requestSnapshot(9);
    QCOMPARE(hyprland->requestedSnapshotTokens, QVector<RequestToken>{9});
    hyprland->completeSnapshot(9, snapshot);
    QCOMPARE(readySpy.count(), 1);

    backend.activateWindow({WindowId{QStringLiteral("1")}, 11});
    QCOMPARE(hyprland->activationRequests.size(), 1);
    hyprland->completeActivation(11);
    QCOMPARE(activationSpy.count(), 1);
    hyprland->emitError(BackendError::CompositorRejected);
    QCOMPARE(errorSpy.count(), 1);
}

void CompositorBackendFactoryTest::hyprlandCapabilitiesAppearAfterSelection()
{
    auto candidates = std::make_shared<QVector<QPointer<SelectionFakeBackend>>>();
    AutoCompositorBackend backend(dependencies(false, true, candidates));
    backend.start();
    QVERIFY(!backend.capabilities().contains(BackendCapability::WindowActivation));
    candidateAt(candidates, 0)->setState(BackendState::Ready);
    QVERIFY(backend.capabilities().contains(BackendCapability::WindowActivation));
}

void CompositorBackendFactoryTest::stopStopsCandidateAndRestartStartsFreshGeneration()
{
    auto candidates = std::make_shared<QVector<QPointer<SelectionFakeBackend>>>();
    AutoCompositorBackend backend(dependencies(true, true, candidates));
    SelectionFakeBackend::totalStopCalls = 0;
    backend.start();
    QPointer<SelectionFakeBackend> first = candidateAt(candidates, 0);
    QCOMPARE(first->startCount, 1);
    backend.stop();
    QCOMPARE(SelectionFakeBackend::totalStopCalls, 1);
    QVERIFY(first.isNull());
    backend.start();
    QCOMPARE(candidates->size(), 2);
    QVERIFY(candidates->at(0).isNull());
    QVERIFY(candidates->at(1));
    QCOMPARE(candidateAt(candidates, 1)->startCount, 1);
}

void CompositorBackendFactoryTest::autoBackendRetainsPendingTyphonRequestUntilSnapshot()
{
    auto adapter = std::make_shared<QPointer<AutoFakeTyphonAdapter>>();
    auto source = std::make_shared<QPointer<TyphonWindowSource>>();
    AutoCompositorBackend::Dependencies deps;
    deps.typhonCompiled = [] { return true; };
    deps.hyprlandAvailable = [] { return false; };
    deps.createCandidate = [adapter, source](AutoBackendCandidate candidate, QObject *parent) {
        if (candidate != AutoBackendCandidate::Typhon)
            return static_cast<CompositorBackend *>(nullptr);
        auto *fakeAdapter = new AutoFakeTyphonAdapter;
        *adapter = fakeAdapter;
        auto *connection = new TyphonToplevelConnection(fakeAdapter);
        auto *backend = new TyphonWindowSource(connection, parent);
        *source = backend;
        return static_cast<CompositorBackend *>(backend);
    };

    AutoCompositorBackend backend(deps);
    QSignalSpy readySpy(&backend, &CompositorBackend::snapshotReady);
    QSignalSpy changedSpy(&backend, &CompositorBackend::snapshotChanged);

    backend.requestSnapshot(42);
    backend.start();
    QVERIFY(adapter->data());
    adapter->data()->advertiseManager();
    QVERIFY(source->data());
    QCOMPARE(backend.descriptor().name, QStringLiteral("typhon"));
    QCOMPARE(backend.state(), BackendState::LoadingInitialSnapshot);
    QCOMPARE(readySpy.count(), 0);

    completeAutoTyphonSnapshot(*adapter->data(), 1, QStringLiteral("1"), 1);

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(readySpy.count(), 1);
    QCOMPARE(readySpy.at(0).at(0).value<RequestToken>(), RequestToken(42));
    QCOMPARE(readySpy.at(0).at(1).value<WindowSnapshot>().windows.size(), 1);
    QCOMPARE(backend.state(), BackendState::Ready);
}

void CompositorBackendFactoryTest::stress100AutoBackendPendingTyphonHandoffs()
{
    for (RequestToken cycle = 1; cycle <= 100; ++cycle) {
        auto adapter = std::make_shared<QPointer<AutoFakeTyphonAdapter>>();
        AutoCompositorBackend::Dependencies deps;
        deps.typhonCompiled = [] { return true; };
        deps.hyprlandAvailable = [] { return false; };
        deps.createCandidate = [adapter](AutoBackendCandidate candidate, QObject *parent) {
            if (candidate != AutoBackendCandidate::Typhon)
                return static_cast<CompositorBackend *>(nullptr);
            auto *fakeAdapter = new AutoFakeTyphonAdapter;
            *adapter = fakeAdapter;
            auto *connection = new TyphonToplevelConnection(fakeAdapter);
            return static_cast<CompositorBackend *>(new TyphonWindowSource(connection, parent));
        };

        AutoCompositorBackend backend(deps);
        QSignalSpy readySpy(&backend, &CompositorBackend::snapshotReady);
        backend.requestSnapshot(cycle);
        backend.start();
        QVERIFY(adapter->data());
        adapter->data()->advertiseManager();
        QCOMPARE(readySpy.count(), 0);
        completeAutoTyphonSnapshot(*adapter->data(), cycle, QString::number(cycle), cycle);
        QCOMPARE(readySpy.count(), 1);
        QCOMPARE(readySpy.at(0).at(0).value<RequestToken>(), cycle);
        QCOMPARE(readySpy.at(0).at(1).value<WindowSnapshot>().windows.size(), 1);
    }
}

QTEST_MAIN(CompositorBackendFactoryTest)
#include "CompositorBackendFactoryTest.moc"
