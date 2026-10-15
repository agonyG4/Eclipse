#include "core/DockController.hpp"

#include "apps/appidentity/AppIdentityResolver.hpp"
#include "services/DockConfigValidation.hpp"

#include <QDebug>

#include <algorithm>

using Astrea::Dock::DockRuntimeActionError;

namespace {

bool sameConfig(const DockConfig &left, const DockConfig &right)
{
    return left.iconSize == right.iconSize
        && left.panelPadding == right.panelPadding
        && left.itemSpacing == right.itemSpacing
        && left.hoverEffect == right.hoverEffect
        && qFuzzyCompare(left.magnificationScale, right.magnificationScale)
        && qFuzzyCompare(left.magnificationRadius, right.magnificationRadius)
        && left.edgeMargin == right.edgeMargin
        && left.position == right.position
        && left.floating == right.floating
        && left.cornerRadius == right.cornerRadius
        && left.autoHide == right.autoHide
        && left.indicatorStyle == right.indicatorStyle
        && left.indicatorSize == right.indicatorSize
        && left.animationsEnabled == right.animationsEnabled
        && qFuzzyCompare(left.animationSpeed, right.animationSpeed)
        && left.pins == right.pins;
}

DockConfig normalizedConfig(const DockConfig &config)
{
    DockConfig normalized = config;
    const DockConfig defaults = DockConfig::defaults();
    // Keep direct C++ callers of the old struct source-compatible while making the
    // canonical edgeMargin field authoritative for parsed configurations.
    if (config.edgeMargin == defaults.edgeMargin && config.bottomMargin != defaults.bottomMargin)
        normalized.edgeMargin = config.bottomMargin;
    normalized.bottomMargin = normalized.edgeMargin;
    return normalized;
}

QString identityFingerprint(const Astrea::Dock::DockRuntimeWindow &window)
{
    return window.appId + QLatin1Char('|') + window.title;
}

} // namespace

DockController::DockController(ApplicationLauncher *launcher, DesktopEntryCatalog *catalog,
                               DockConfigPersistence *persistence,
                               QObject *parent)
    : QObject(parent), m_launcher(launcher), m_persistence(persistence), m_catalog(catalog),
      m_catalogSnapshot(std::make_shared<const DesktopEntrySnapshot>())
{
    m_identityResolver = new AppIdentityResolver(this);
    m_identityResolver->initialize(m_catalog);
    connect(m_identityResolver, &AppIdentityResolver::identityResolved,
            this, &DockController::onIdentityResolved);

    m_autoHideTimer.setSingleShot(true);
    m_autoHideTimer.setInterval(180);
    connect(&m_autoHideTimer, &QTimer::timeout, this, [this] {
        if (!m_pointerInside && autoHideActive())
            setRevealed(false);
    });

    if (m_launcher) {
        connect(m_launcher, &ApplicationLauncher::launchSucceeded,
                this, &DockController::onLaunchSucceeded);
        connect(m_launcher, &ApplicationLauncher::launchFailed,
                this, &DockController::onLaunchFailed);
        connect(m_launcher, &ApplicationLauncher::launchTimedOut,
                this, &DockController::onLaunchTimedOut);
    }
    if (m_catalog) {
        connect(m_catalog, &DesktopEntryCatalog::indexUpdated, this, [this] {
            setCatalogSnapshot(m_catalog->snapshot());
        });
        setCatalogSnapshot(m_catalog->snapshot());
    }
    updateAutoHidePolicy();
    updateVisibility();
}

bool DockController::visible() const
{
    return m_enabled && m_requestedVisible && m_model.rowCount() > 0;
}

int DockController::resolvedPinCount() const
{
    int count = 0;
    for (const QString &pin : m_config.pins) {
        const DockAppInfo *item = m_model.itemAt(m_model.rowForDesktopFileName(pin));
        if (item && item->pinned && item->resolved)
            ++count;
    }
    return count;
}

int DockController::launchingCount() const
{
    int count = 0;
    for (int row = 0; row < m_model.rowCount(); ++row) {
        if (m_model.data(m_model.index(row, 0), DockAppModel::LaunchingRole).toBool())
            ++count;
    }
    return count;
}

int DockController::exclusiveZone() const
{
    if (!m_enabled || !m_requestedVisible || m_model.rowCount() == 0 || autoHideActive())
        return 0;
    return restingHeight();
}

bool DockController::autoHideActive() const
{
    if (m_config.autoHide == QStringLiteral("always"))
        return true;
    return m_config.autoHide == QStringLiteral("intelligent") && m_obstructed;
}

void DockController::setRevealed(bool revealed)
{
    if (m_revealed == revealed)
        return;
    m_revealed = revealed;
    emit revealedChanged();
}

void DockController::updateAutoHidePolicy()
{
    bool obstructed = false;
    if (m_runtimeSnapshot.has_value()) {
        obstructed = std::any_of(m_runtimeSnapshot->windows.cbegin(),
                                 m_runtimeSnapshot->windows.cend(), [](const auto &window) {
            return window.active && (window.maximized || window.fullscreen);
        });
    }
    m_obstructed = obstructed;
    if (!autoHideActive()) {
        m_autoHideTimer.stop();
        setRevealed(true);
    } else if (!m_pointerInside) {
        setRevealed(false);
    }
}

void DockController::applyConfig(const DockConfig &config)
{
    const int previousReservation = exclusiveZone();
    const DockSurfacePlacement previousPlacement = surfacePlacement();
    const DockConfig next = normalizedConfig(config);
    const bool changed = !sameConfig(m_config, next);
    if (!changed)
        return;

    m_config = next;
    m_model.setPins(m_config.pins);
    projectRuntime();
    updateAutoHidePolicy();
    emit configChanged();
    if (previousPlacement != surfacePlacement())
        emit surfacePlacementChanged();
    emit modelChanged();
    updateVisibility();
    if (previousReservation != exclusiveZone())
        emit reservationChanged();
}

void DockController::setComponentEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    const int previousReservation = exclusiveZone();
    m_enabled = enabled;
    emit enabledChanged();
    updateVisibility();
    if (previousReservation != exclusiveZone())
        emit reservationChanged();
}

void DockController::setCatalogSnapshot(std::shared_ptr<const DesktopEntrySnapshot> snapshot)
{
    const int previousReservation = exclusiveZone();
    if (!snapshot)
        snapshot = std::make_shared<const DesktopEntrySnapshot>();
    m_catalogSnapshot = std::move(snapshot);
    m_model.setCatalogSnapshot(m_catalogSnapshot);
    projectRuntime();
    emit modelChanged();
    updateVisibility();
    if (previousReservation != exclusiveZone())
        emit reservationChanged();
}

void DockController::attachRuntimeBackend(Astrea::Dock::DockRuntimeBackend *backend)
{
    Astrea::Dock::DockRuntimeBackend *previousBackend = m_runtimeBackend;
    if (previousBackend && previousBackend != backend)
        clearPublishedMinimizeAnchors({});
    for (const QMetaObject::Connection &binding : std::as_const(m_runtimeConnections))
        disconnect(binding);
    m_runtimeConnections.clear();

    if (previousBackend != backend) {
        m_pendingActivations.clear();
        m_pendingWindowActions.clear();
    }

    if (!backend) {
        m_runtimeBackend = nullptr;
        m_publishedMinimizeAnchors.clear();
        m_pendingActivations.clear();
        m_pendingWindowActions.clear();
        clearRuntimeState();
        return;
    }

    m_runtimeBackend = backend;
    if (previousBackend != backend) {
        m_publishedMinimizeAnchors.clear();
        m_minimizeAnchorCapabilityEpoch = 0;
    }

    m_runtimeConnections.append(connect(
        backend, &Astrea::Dock::DockRuntimeBackend::snapshotChanged, this,
        [this, backend](const Astrea::Dock::DockRuntimeSnapshot &snapshot) {
            if (m_runtimeBackend != backend)
                return;
            if (backend->runtimeReady() && snapshot.authoritative)
                applyRuntimeSnapshot(snapshot);
            else
                clearRuntimeState();
        }));
    m_runtimeConnections.append(connect(
        backend, &Astrea::Dock::DockRuntimeBackend::runtimeReadyChanged, this,
        [this, backend](bool ready) {
            if (m_runtimeBackend != backend)
                return;
            if (!ready) {
                clearRuntimeState();
                return;
            }
            const Astrea::Dock::DockRuntimeSnapshot snapshot = backend->snapshot();
            if (snapshot.authoritative)
                applyRuntimeSnapshot(snapshot);
        }));
    m_runtimeConnections.append(connect(
        backend, &Astrea::Dock::DockRuntimeBackend::actionReadyChanged, this,
        [this, backend](bool ready) {
            if (m_runtimeBackend != backend || !ready)
                return;
            ++m_minimizeAnchorCapabilityEpoch;
            replayMinimizeAnchors(backend);
        }));
    m_runtimeConnections.append(connect(
        backend, &Astrea::Dock::DockRuntimeBackend::actionFinished,
        this, &DockController::onRuntimeActionFinished));
    m_runtimeConnections.append(connect(
        backend, &Astrea::Dock::DockRuntimeBackend::actionFailed,
        this, &DockController::onRuntimeActionFailed));

    if (backend->runtimeReady() && backend->snapshot().authoritative)
        applyRuntimeSnapshot(backend->snapshot());
    else
        clearRuntimeState();
}

void DockController::applyRuntimeSnapshot(const Astrea::Dock::DockRuntimeSnapshot &snapshot)
{
    if (!snapshot.authoritative) {
        clearRuntimeState();
        return;
    }
    const int previousReservation = exclusiveZone();
    const DockSurfacePlacement previousPlacement = surfacePlacement();
    m_runtimeSnapshot = snapshot;
    m_runtimeKnown = true;
    updateAutoHidePolicy();
    projectRuntime();
    emit modelChanged();
    if (previousPlacement != surfacePlacement())
        emit surfacePlacementChanged();
    if (previousReservation != exclusiveZone())
        emit reservationChanged();
}

void DockController::clearRuntimeState()
{
    const bool changed = m_runtimeKnown || m_runtimeSnapshot.has_value() || !m_runtimeStates.isEmpty();
    const int previousReservation = exclusiveZone();
    const DockSurfacePlacement previousPlacement = surfacePlacement();
    m_runtimeKnown = false;
    m_runtimeSnapshot.reset();
    m_runtimeStates.clear();
    m_runtimeTaskIdentityTracker.reset();
    m_model.clearRuntimeProjection();
    updateAutoHidePolicy();
    if (changed)
        emit modelChanged();
    if (previousPlacement != surfacePlacement())
        emit surfacePlacementChanged();
    if (previousReservation != exclusiveZone())
        emit reservationChanged();
}

bool DockController::setMinimizeAnchor(const QString &taskKey, const QRect &rect)
{
    if (taskKey.isEmpty() || rect.width() <= 0 || rect.height() <= 0)
        return false;
    const auto existing = m_minimizeAnchors.constFind(taskKey);
    if (existing != m_minimizeAnchors.constEnd() && existing.value() == rect) {
        diagnoseMinimizeAnchor(MinimizeAnchorDiagnosticEvent::AnchorCached,
                               MinimizeAnchorDiagnosticReason::LocalAcceptanceOnly,
                               taskKey, {});
        publishMinimizeAnchor(taskKey);
        return true;
    }
    m_minimizeAnchors.insert(taskKey, rect);
    diagnoseMinimizeAnchor(MinimizeAnchorDiagnosticEvent::AnchorCached,
                           MinimizeAnchorDiagnosticReason::LocalAcceptanceOnly,
                           taskKey, {});
    publishMinimizeAnchor(taskKey);
    return true;
}

void DockController::clearMinimizeAnchor(const QString &taskKey)
{
    m_minimizeAnchors.remove(taskKey);
    diagnoseMinimizeAnchor(MinimizeAnchorDiagnosticEvent::AnchorRemoved,
                           MinimizeAnchorDiagnosticReason::LocalCacheRemoved, taskKey, {});
    const auto state = m_runtimeStates.constFind(taskKey);
    if (state != m_runtimeStates.constEnd()) {
        for (const QString &windowId : state->windowIds) {
            if (!m_runtimeBackend)
                continue;
            const auto error = m_runtimeBackend->clearMinimizeAnchor(windowId);
            if (!error.has_value()) {
                const auto published = m_publishedMinimizeAnchors.constFind(windowId);
                if (published != m_publishedMinimizeAnchors.constEnd()
                    && published->taskKey == taskKey)
                    m_publishedMinimizeAnchors.remove(windowId);
                diagnoseMinimizeAnchor(
                    MinimizeAnchorDiagnosticEvent::AnchorRemoved,
                    MinimizeAnchorDiagnosticReason::TransportQueuedNoRemoteAck,
                    taskKey, windowId);
            } else if (error == DockRuntimeActionError::NotActionReady) {
                diagnoseMinimizeAnchor(
                    MinimizeAnchorDiagnosticEvent::DeferredNotAuthenticated,
                    MinimizeAnchorDiagnosticReason::NotAuthenticated,
                    taskKey, windowId);
            } else if (error == DockRuntimeActionError::WindowNotLive) {
                diagnoseMinimizeAnchor(
                    MinimizeAnchorDiagnosticEvent::MissingHandle,
                    MinimizeAnchorDiagnosticReason::ToplevelNotLive,
                    taskKey, windowId);
            } else {
                diagnoseMinimizeAnchor(
                    MinimizeAnchorDiagnosticEvent::SendFailed,
                    *error == DockRuntimeActionError::UnsupportedProtocol
                        ? MinimizeAnchorDiagnosticReason::UnsupportedProtocol
                        : *error == DockRuntimeActionError::Disconnected
                            ? MinimizeAnchorDiagnosticReason::Disconnected
                            : *error == DockRuntimeActionError::LocalCapacityExceeded
                                ? MinimizeAnchorDiagnosticReason::LocalCapacityExceeded
                                : MinimizeAnchorDiagnosticReason::InvalidRequest,
                    taskKey, windowId);
            }
        }
    }
}

void DockController::setPointerInside(bool inside)
{
    m_pointerInside = inside;
    if (!autoHideActive()) {
        m_autoHideTimer.stop();
        setRevealed(true);
        return;
    }
    if (inside) {
        m_autoHideTimer.stop();
        setRevealed(true);
    } else if (m_revealed) {
        m_autoHideTimer.start();
    }
}

void DockController::launch(int row)
{
    const DockAppInfo *item = m_model.itemAt(row);
    if (!item || !m_enabled)
        return;

    launchItem(*item);
}

void DockController::launchByDesktopFileName(const QString &desktopFileName)
{
    const int row = m_model.rowForDesktopFileName(desktopFileName);
    const DockAppInfo *item = m_model.itemAt(row);
    if (!item || !m_enabled)
        return;
    launchItem(*item);
}

bool DockController::launchNewWindow(const QString &desktopFileName)
{
    const int row = m_model.rowForDesktopFileName(desktopFileName);
    const DockAppInfo *item = m_model.itemAt(row);
    if (!item || !m_enabled || !m_launcher)
        return false;
    launchItem(*item, false);
    return true;
}

void DockController::activateOrLaunchTask(const QString &taskKey)
{
    const int row = m_model.rowForTaskKey(taskKey);
    const DockAppInfo *item = m_model.itemAt(row);
    if (!item || !m_enabled)
        return;
    launchItem(*item);
}

QVector<Astrea::Dock::DockRuntimeWindow> DockController::windowsForTaskKey(
    const QString &taskKey) const
{
    QVector<Astrea::Dock::DockRuntimeWindow> result;
    const auto state = m_runtimeStates.constFind(taskKey);
    if (state == m_runtimeStates.constEnd() || !m_runtimeSnapshot.has_value())
        return result;

    for (const QString &windowId : state->windowIds) {
        const auto it = std::find_if(m_runtimeSnapshot->windows.cbegin(),
                                     m_runtimeSnapshot->windows.cend(),
                                     [&windowId](const auto &window) {
                                         return window.id == windowId;
                                     });
        if (it != m_runtimeSnapshot->windows.cend())
            result.append(*it);
    }
    return result;
}

QVector<Astrea::Dock::DockRuntimeWindow> DockController::windowsForDesktopFileName(
    const QString &desktopFileName) const
{
    const QString taskKey = m_model.runtimeTaskKeyForDesktopFileName(desktopFileName);
    return taskKey.isEmpty() ? QVector<Astrea::Dock::DockRuntimeWindow>{}
                             : windowsForTaskKey(taskKey);
}

bool DockController::requestExactWindowAction(const QString &taskKey,
                                              const QString &windowId,
                                              Astrea::Dock::DockRuntimeAction action)
{
    const auto state = m_runtimeStates.constFind(taskKey);
    const auto windows = windowsForTaskKey(taskKey);
    const bool exactWindowIsLive = std::any_of(windows.cbegin(), windows.cend(),
                                               [&windowId](const auto &window) {
        return window.id == windowId;
    });
    if (state == m_runtimeStates.constEnd() || !state->windowIds.contains(windowId)
        || !exactWindowIsLive) {
        setLastError(QStringLiteral("The selected window is no longer available"));
        return false;
    }
    if (!m_runtimeBackend) {
        setLastError(QStringLiteral("Dock window actions are unavailable"));
        return false;
    }

    const quint64 token = ++m_nextActivationToken;
    m_pendingWindowActions.insert(token, PendingWindowAction{taskKey, windowId, action});
    const auto error = m_runtimeBackend->requestAction(windowId, action, token);
    if (error.has_value()) {
        m_pendingWindowActions.remove(token);
        setLastError(QStringLiteral("The selected window action was rejected"));
        return false;
    }
    return true;
}

bool DockController::activateWindow(const QString &taskKey, const QString &windowId)
{
    return requestExactWindowAction(taskKey, windowId,
                                    Astrea::Dock::DockRuntimeAction::Activate);
}

bool DockController::closeWindow(const QString &taskKey, const QString &windowId)
{
    return requestExactWindowAction(taskKey, windowId,
                                    Astrea::Dock::DockRuntimeAction::Close);
}

bool DockController::setPinned(const QString &desktopFileName, bool pinned)
{
    if (!DockConfigValidation::validDesktopFileName(desktopFileName)) {
        setLastError(QStringLiteral("Invalid Dock desktop filename"));
        return false;
    }
    if (!m_persistence) {
        setLastError(QStringLiteral("Dock pin persistence is unavailable"));
        return false;
    }

    QStringList pins = m_config.pins;
    if (pinned) {
        if (!pins.contains(desktopFileName))
            pins.append(desktopFileName);
    } else {
        pins.removeAll(desktopFileName);
    }
    if (pins == m_config.pins)
        return true;

    QString error;
    if (!m_persistence->writePins(pins, &error)) {
        setLastError(error.left(512).isEmpty()
                         ? QStringLiteral("Could not persist Dock pins") : error.left(512));
        return false;
    }
    m_config.pins = pins;
    m_model.setPins(m_config.pins);
    emit configChanged();
    emit modelChanged();
    updateVisibility();
    return true;
}

bool DockController::movePinned(const QString &desktopFileName, int targetPinIndex)
{
    const int sourcePinIndex = m_config.pins.indexOf(desktopFileName);
    if (sourcePinIndex < 0 || m_config.pins.isEmpty())
        return false;
    const DockAppInfo *sourceItem =
        m_model.itemAt(m_model.rowForDesktopFileName(desktopFileName));
    if (!sourceItem || !sourceItem->pinned)
        return false;

    const int boundedTarget = qBound(0, targetPinIndex, m_config.pins.size() - 1);
    if (sourcePinIndex == boundedTarget)
        return false;
    if (!m_persistence) {
        setLastError(QStringLiteral("Dock pin persistence is unavailable"));
        return false;
    }

    QStringList reordered = m_config.pins;
    const QString moved = reordered.takeAt(sourcePinIndex);
    reordered.insert(boundedTarget, moved);
    QString error;
    if (!m_persistence->writePins(reordered, &error)) {
        setLastError(error.left(512).isEmpty()
                         ? QStringLiteral("Could not persist Dock pin order") : error.left(512));
        return false;
    }

    m_config.pins = reordered;
    m_model.setPins(m_config.pins);
    emit configChanged();
    emit modelChanged();
    updateVisibility();
    return true;
}

void DockController::launchItem(const DockAppInfo &item, bool activateRunning)
{
    if (!m_enabled)
        return;

    const QString taskKey = item.taskKey;
    if (activateRunning && item.runtimeKnown && item.running) {
        const auto state = m_runtimeStates.constFind(taskKey);
        if (state == m_runtimeStates.constEnd() || state->windowIds.isEmpty()) {
            qInfo("Dock activation suppressed for running application '%s' without a live target",
                  qPrintable(taskKey));
            return;
        }
        if (!m_runtimeBackend) {
            qInfo("Dock activation suppressed for running application '%s' without runtime actions",
                  qPrintable(taskKey));
            return;
        }

        const quint64 activationToken = ++m_nextActivationToken;
        const QString targetWindowId = state->windowIds.constFirst();
        m_pendingActivations.insert(activationToken, taskKey);
        const auto error = m_runtimeBackend->requestAction(
            targetWindowId, Astrea::Dock::DockRuntimeAction::Activate, activationToken);
        if (error.has_value()) {
            m_pendingActivations.remove(activationToken);
            reconcileRuntimeActionFailure(error.value());
        } else {
            qInfo("Dock activating exact runtime window '%s' for '%s'",
                  qPrintable(targetWindowId), qPrintable(taskKey));
        }
        return;
    }
    if (item.desktopFileName.isEmpty())
        return;

    const QString key = item.desktopFileName;
    if (m_pendingLaunches.contains(key))
        return;

    ApplicationLaunchRequest request;
    request.desktopFileName = item.desktopFileName;
    request.desktopId = item.desktopId;
    if (m_catalog) {
        const auto record = m_catalog->findByDesktopFileName(item.desktopFileName);
        if (record) {
            request.desktopId = record->id;
            request.exec = record->exec;
            request.appName = record->name;
            request.iconName = record->icon;
            request.desktopFilePath = record->sourceFilePath;
        }
    }
    const QString launchId = !request.desktopFileName.isEmpty()
        ? request.desktopFileName
        : (request.desktopId.isEmpty() ? key : request.desktopId);
    m_pendingLaunches.insert(key, launchId);
    m_model.setLaunchError(key, {});
    m_model.setLaunching(key, true);
    emit modelChanged();

    if (!m_launcher) {
        finishLaunch(launchId, false, QStringLiteral("Application launcher is unavailable"));
        return;
    }
    m_launcher->launchDesktop(request);
}

void DockController::show()
{
    if (m_requestedVisible)
    {
        setRevealed(true);
        return;
    }
    const int previousReservation = exclusiveZone();
    m_requestedVisible = true;
    setRevealed(true);
    updateVisibility();
    if (previousReservation != exclusiveZone())
        emit reservationChanged();
}

void DockController::hide()
{
    if (!m_requestedVisible)
        return;
    const int previousReservation = exclusiveZone();
    m_requestedVisible = false;
    m_autoHideTimer.stop();
    setRevealed(false);
    updateVisibility();
    if (previousReservation != exclusiveZone())
        emit reservationChanged();
}

void DockController::onLaunchSucceeded(const QString &desktopId)
{
    finishLaunch(desktopId, true);
}

void DockController::onLaunchFailed(const QString &desktopId, const QString &error)
{
    finishLaunch(desktopId, false, error);
}

void DockController::onLaunchTimedOut(const QString &desktopId)
{
    finishLaunch(desktopId, false, QStringLiteral("Launch timed out"));
}

void DockController::onRuntimeActionFinished(
    quint64 token, Astrea::Dock::DockRuntimeAction action,
    Astrea::Dock::DockRuntimeActionResult result)
{
    const auto pendingWindow = m_pendingWindowActions.find(token);
    if (pendingWindow != m_pendingWindowActions.end()) {
        const PendingWindowAction operation = pendingWindow.value();
        m_pendingWindowActions.erase(pendingWindow);
        if (result == Astrea::Dock::DockRuntimeActionResult::Unavailable)
            setLastError(QStringLiteral("The selected window is no longer available"));
        Q_UNUSED(operation);
        return;
    }
    if (action != Astrea::Dock::DockRuntimeAction::Activate
        || !m_pendingActivations.contains(token))
        return;
    const QString key = m_pendingActivations.take(token);
    if (result == Astrea::Dock::DockRuntimeActionResult::Unavailable) {
        qInfo("Dock activation unavailable for '%s'; reconciling without launch",
              qPrintable(key));
        reconcileRuntimeActionFailure(Astrea::Dock::DockRuntimeActionError::WindowNotLive);
    }
}

void DockController::onRuntimeActionFailed(
    quint64 token, Astrea::Dock::DockRuntimeAction action,
    Astrea::Dock::DockRuntimeActionError error)
{
    if (m_pendingWindowActions.contains(token)) {
        m_pendingWindowActions.remove(token);
        setLastError(QStringLiteral("The selected window action failed"));
        return;
    }
    if (action != Astrea::Dock::DockRuntimeAction::Activate
        || !m_pendingActivations.contains(token))
        return;
    m_pendingActivations.remove(token);
    reconcileRuntimeActionFailure(error);
}

QString DockController::keyForLaunchId(const QString &desktopId) const
{
    auto direct = m_pendingLaunches.key(desktopId);
    if (!direct.isEmpty())
        return direct;
    for (auto it = m_pendingLaunches.cbegin(); it != m_pendingLaunches.cend(); ++it) {
        if (it.value() == desktopId)
            return it.key();
    }
    return {};
}

void DockController::finishLaunch(const QString &desktopId, bool success, const QString &error)
{
    const QString key = keyForLaunchId(desktopId);
    if (key.isEmpty())
        return;
    m_pendingLaunches.remove(key);
    m_model.setLaunching(key, false);
    if (success) {
        m_model.setLaunchError(key, {});
    } else {
        const QString bounded = error.left(512).isEmpty()
            ? QStringLiteral("Application launch failed") : error.left(512);
        m_model.setLaunchError(key, bounded);
        setLastError(bounded);
        qWarning("Dock launch failed for '%s': %s", qPrintable(key), qPrintable(bounded));
    }
    emit modelChanged();
}

void DockController::setLastError(const QString &error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}

void DockController::updateVisibility()
{
    const bool next = visible();
    if (m_visible == next)
        return;
    m_visible = next;
    emit visibleChanged();
}

void DockController::projectRuntime()
{
    if (!m_runtimeKnown || !m_runtimeSnapshot.has_value()) {
        m_publishedMinimizeAnchors.clear();
        m_runtimeStates.clear();
        m_model.clearRuntimeProjection();
        return;
    }

    const QHash<QString, QString> taskKeysByWindowId =
        m_runtimeTaskIdentityTracker.update(*m_runtimeSnapshot, m_catalogSnapshot);
    const Astrea::Dock::DockApplicationRuntimeProjection projection =
        m_runtimeProjector.project(*m_runtimeSnapshot, m_catalogSnapshot, taskKeysByWindowId);
    m_runtimeStates = projection.states;
    m_model.applyRuntimeProjection(projection, true);
    publishAllMinimizeAnchors();
    clearPublishedMinimizeAnchors(liveRuntimeWindowIds());
    resolveTaskIdentities();
}

void DockController::resolveTaskIdentities()
{
    if (!m_identityResolver || !m_runtimeSnapshot.has_value())
        return;

    const Astrea::Dock::DockRuntimeSnapshot &snapshot = m_runtimeSnapshot.value();
    for (auto it = m_runtimeStates.cbegin(); it != m_runtimeStates.cend(); ++it) {
        const Astrea::Dock::DockApplicationRuntimeState &state = it.value();
        if (state.windowIds.isEmpty())
            continue;

        const QString &windowId = state.windowIds.constFirst();
        const auto window = std::find_if(snapshot.windows.cbegin(), snapshot.windows.cend(),
                                         [&windowId](const auto &candidate) {
            return candidate.id == windowId;
        });
        if (window == snapshot.windows.cend())
            continue;

        WindowIdentityInput input;
        input.address = window->id;
        input.pid = window->pid;
        input.className = window->appId;
        input.initialClass = window->appId;
        input.title = window->title;
        input.initialTitle = window->title;
        input.openGeneration = snapshot.generation;
        input.metadataFingerprint = identityFingerprint(*window);
        input.themeRevision = m_identityResolver->themeRevision();
        input.desktopIndexRevision = m_identityResolver->desktopIndexRevision();
        input.steamIndexRevision = m_identityResolver->steamIndexRevision();

        const AppIdentity fast = m_identityResolver->resolveSync(input);
        if (fast.iconPending && m_model.applyIdentityEnrichment(state.taskKey, fast))
            emit modelChanged();
        m_identityResolver->resolveAsync(input, snapshot.generation);
    }
}

void DockController::onIdentityResolved(const QString &address, const AppIdentity &identity)
{
    if (!m_runtimeSnapshot.has_value() || identity.windowId != address)
        return;

    const Astrea::Dock::DockRuntimeWindow *currentWindow = nullptr;
    for (const auto &window : m_runtimeSnapshot->windows) {
        if (window.id == address) {
            currentWindow = &window;
            break;
        }
    }
    if (!currentWindow || identity.pid != currentWindow->pid
        || identity.openGeneration != m_runtimeSnapshot->generation
        || identity.metadataFingerprint != identityFingerprint(*currentWindow))
        return;

    for (auto it = m_runtimeStates.cbegin(); it != m_runtimeStates.cend(); ++it) {
        if (!it->windowIds.contains(address))
            continue;
        if (m_model.applyIdentityEnrichment(it.key(), identity))
            emit modelChanged();
        return;
    }
}

void DockController::publishMinimizeAnchor(const QString &taskKey)
{
    if (!m_runtimeBackend)
        return;
    const auto anchor = m_minimizeAnchors.constFind(taskKey);
    const auto state = m_runtimeStates.constFind(taskKey);
    if (anchor == m_minimizeAnchors.constEnd() || state == m_runtimeStates.constEnd())
        return;
    for (const QString &windowId : state->windowIds) {
        const auto handleToken = m_runtimeBackend->handleTokenForWindowId(windowId);
        if (!handleToken.has_value()) {
            diagnoseMinimizeAnchor(MinimizeAnchorDiagnosticEvent::MissingHandle,
                                   MinimizeAnchorDiagnosticReason::ToplevelNotLive,
                                   taskKey, windowId);
            continue;
        }
        const quint64 generation = m_runtimeBackend->generation();
        const auto published = m_publishedMinimizeAnchors.constFind(windowId);
        if (published != m_publishedMinimizeAnchors.constEnd()
            && published->taskKey == taskKey && published->rect == anchor.value()
            && published->generation == generation
            && published->handleToken == handleToken.value()
            && published->capabilityEpoch == m_minimizeAnchorCapabilityEpoch)
            continue;

        diagnoseMinimizeAnchor(MinimizeAnchorDiagnosticEvent::PublishAttempted,
                               MinimizeAnchorDiagnosticReason::None, taskKey, windowId);
        const auto error = m_runtimeBackend->setMinimizeAnchor(windowId, anchor.value());
        if (!error.has_value()) {
            m_publishedMinimizeAnchors.insert(
                windowId,
                PublishedMinimizeAnchor{taskKey, anchor.value(), generation,
                                        handleToken.value(), m_minimizeAnchorCapabilityEpoch});
            diagnoseMinimizeAnchor(MinimizeAnchorDiagnosticEvent::SendQueued,
                                   MinimizeAnchorDiagnosticReason::TransportQueuedNoRemoteAck,
                                   taskKey, windowId);
        } else if (*error == DockRuntimeActionError::NotActionReady) {
            diagnoseMinimizeAnchor(MinimizeAnchorDiagnosticEvent::DeferredNotAuthenticated,
                                   MinimizeAnchorDiagnosticReason::NotAuthenticated,
                                   taskKey, windowId);
        } else if (*error == DockRuntimeActionError::WindowNotLive) {
            diagnoseMinimizeAnchor(MinimizeAnchorDiagnosticEvent::MissingHandle,
                                   MinimizeAnchorDiagnosticReason::ToplevelNotLive,
                                   taskKey, windowId);
        } else {
            const auto reason = *error == DockRuntimeActionError::UnsupportedProtocol
                ? MinimizeAnchorDiagnosticReason::UnsupportedProtocol
                : *error == DockRuntimeActionError::Disconnected
                    ? MinimizeAnchorDiagnosticReason::Disconnected
                    : *error == DockRuntimeActionError::LocalCapacityExceeded
                        ? MinimizeAnchorDiagnosticReason::LocalCapacityExceeded
                        : MinimizeAnchorDiagnosticReason::InvalidRequest;
            diagnoseMinimizeAnchor(MinimizeAnchorDiagnosticEvent::SendFailed,
                                   reason, taskKey, windowId);
        }
    }
}

void DockController::publishAllMinimizeAnchors()
{
    for (auto it = m_minimizeAnchors.cbegin(); it != m_minimizeAnchors.cend(); ++it)
        publishMinimizeAnchor(it.key());
}

void DockController::clearPublishedMinimizeAnchors(const QSet<QString> &liveWindowIds)
{
    const auto published = m_publishedMinimizeAnchors;
    for (auto it = published.cbegin(); it != published.cend(); ++it) {
        if (!liveWindowIds.contains(it.key())) {
            m_publishedMinimizeAnchors.remove(it.key());
            continue;
        }
        if (!m_runtimeBackend)
            continue;

        QString currentTaskKey;
        for (auto state = m_runtimeStates.cbegin(); state != m_runtimeStates.cend(); ++state) {
            if (state->windowIds.contains(it.key())) {
                currentTaskKey = state.key();
                break;
            }
        }
        const auto currentAnchor = m_minimizeAnchors.constFind(currentTaskKey);
        const auto currentHandle = m_runtimeBackend->handleTokenForWindowId(it.key());
        const bool isCurrentPublication = !currentTaskKey.isEmpty()
            && currentAnchor != m_minimizeAnchors.constEnd()
            && currentAnchor.value() == it->rect && currentTaskKey == it->taskKey
            && currentHandle.has_value() && currentHandle.value() == it->handleToken
            && it->generation == m_runtimeBackend->generation()
            && it->capabilityEpoch == m_minimizeAnchorCapabilityEpoch;
        if (isCurrentPublication)
            continue;

        if (!currentHandle.has_value()) {
            diagnoseMinimizeAnchor(MinimizeAnchorDiagnosticEvent::MissingHandle,
                                   MinimizeAnchorDiagnosticReason::ToplevelNotLive,
                                   it->taskKey, it.key());
            continue;
        }
        const auto error = m_runtimeBackend->clearMinimizeAnchor(it.key());
        if (!error.has_value()) {
            m_publishedMinimizeAnchors.remove(it.key());
            diagnoseMinimizeAnchor(MinimizeAnchorDiagnosticEvent::AnchorRemoved,
                                   MinimizeAnchorDiagnosticReason::TransportQueuedNoRemoteAck,
                                   it->taskKey, it.key());
        } else if (*error == DockRuntimeActionError::NotActionReady) {
            diagnoseMinimizeAnchor(MinimizeAnchorDiagnosticEvent::DeferredNotAuthenticated,
                                   MinimizeAnchorDiagnosticReason::NotAuthenticated,
                                   it->taskKey, it.key());
        } else if (*error == DockRuntimeActionError::WindowNotLive) {
            m_publishedMinimizeAnchors.remove(it.key());
            diagnoseMinimizeAnchor(MinimizeAnchorDiagnosticEvent::MissingHandle,
                                   MinimizeAnchorDiagnosticReason::ToplevelNotLive,
                                   it->taskKey, it.key());
        } else {
            const auto reason = *error == DockRuntimeActionError::UnsupportedProtocol
                ? MinimizeAnchorDiagnosticReason::UnsupportedProtocol
                : *error == DockRuntimeActionError::Disconnected
                    ? MinimizeAnchorDiagnosticReason::Disconnected
                    : *error == DockRuntimeActionError::LocalCapacityExceeded
                        ? MinimizeAnchorDiagnosticReason::LocalCapacityExceeded
                        : MinimizeAnchorDiagnosticReason::InvalidRequest;
            diagnoseMinimizeAnchor(MinimizeAnchorDiagnosticEvent::SendFailed,
                                   reason, it->taskKey, it.key());
        }
    }
}

QSet<QString> DockController::liveRuntimeWindowIds() const
{
    QSet<QString> liveWindowIds;
    for (auto state = m_runtimeStates.cbegin(); state != m_runtimeStates.cend(); ++state) {
        for (const QString &windowId : state->windowIds)
            liveWindowIds.insert(windowId);
    }
    return liveWindowIds;
}

void DockController::replayMinimizeAnchors(Astrea::Dock::DockRuntimeBackend *backend)
{
    if (m_runtimeBackend != backend || !backend || !backend->runtimeReady()
        || !backend->actionReady() || !backend->snapshot().authoritative)
        return;
    publishAllMinimizeAnchors();
    clearPublishedMinimizeAnchors(liveRuntimeWindowIds());
}

void DockController::diagnoseMinimizeAnchor(MinimizeAnchorDiagnosticEvent event,
                                            MinimizeAnchorDiagnosticReason reason,
                                            const QString &taskKey,
                                            const QString &windowId)
{
    if (!qEnvironmentVariableIsSet("ASTREA_TYPHON_ANCHOR_DIAGNOSTICS"))
        return;
    if (!m_minimizeAnchorDiagnosticWindow.isValid()
        || m_minimizeAnchorDiagnosticWindow.elapsed() >= 1000) {
        m_minimizeAnchorDiagnosticWindow.start();
        m_minimizeAnchorDiagnosticCount = 0;
    }
    if (m_minimizeAnchorDiagnosticCount >= 8)
        return;
    ++m_minimizeAnchorDiagnosticCount;

    const auto eventName = [event] {
        switch (event) {
        case MinimizeAnchorDiagnosticEvent::AnchorCached: return QStringLiteral("anchor_cached");
        case MinimizeAnchorDiagnosticEvent::PublishAttempted: return QStringLiteral("publish_attempted");
        case MinimizeAnchorDiagnosticEvent::SendQueued: return QStringLiteral("send_queued");
        case MinimizeAnchorDiagnosticEvent::DeferredNotAuthenticated:
            return QStringLiteral("deferred_not_authenticated");
        case MinimizeAnchorDiagnosticEvent::MissingHandle: return QStringLiteral("missing_handle");
        case MinimizeAnchorDiagnosticEvent::SendFailed: return QStringLiteral("send_failed");
        case MinimizeAnchorDiagnosticEvent::AnchorRemoved: return QStringLiteral("anchor_removed");
        }
        return QStringLiteral("unknown");
    }();
    const auto reasonName = [reason] {
        switch (reason) {
        case MinimizeAnchorDiagnosticReason::None: return QStringLiteral("none");
        case MinimizeAnchorDiagnosticReason::LocalAcceptanceOnly:
            return QStringLiteral("local_acceptance_only");
        case MinimizeAnchorDiagnosticReason::TransportQueuedNoRemoteAck:
            return QStringLiteral("transport_queued_no_remote_ack");
        case MinimizeAnchorDiagnosticReason::LocalCacheRemoved:
            return QStringLiteral("local_cache_removed");
        case MinimizeAnchorDiagnosticReason::NotAuthenticated:
            return QStringLiteral("not_authenticated");
        case MinimizeAnchorDiagnosticReason::UnsupportedProtocol:
            return QStringLiteral("unsupported_protocol");
        case MinimizeAnchorDiagnosticReason::Disconnected:
            return QStringLiteral("disconnected");
        case MinimizeAnchorDiagnosticReason::LocalCapacityExceeded:
            return QStringLiteral("local_capacity_exceeded");
        case MinimizeAnchorDiagnosticReason::ToplevelNotLive:
            return QStringLiteral("toplevel_not_live");
        case MinimizeAnchorDiagnosticReason::InvalidRequest:
            return QStringLiteral("invalid_request");
        }
        return QStringLiteral("unknown");
    }();
    const auto safeToken = [](QString value) {
        value.truncate(96);
        for (qsizetype index = 0; index < value.size(); ++index) {
            const ushort code = value.at(index).unicode();
            const bool safe = (code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z')
                || (code >= '0' && code <= '9') || code == '.' || code == '_' || code == ':'
                || code == '-';
            if (!safe)
                value[index] = QLatin1Char('_');
        }
        return value.isEmpty() ? QStringLiteral("-") : value;
    };
    const QString safeTaskKey = safeToken(taskKey);
    const QString safeWindowId = safeToken(windowId);
    const QString diagnosticContext = m_runtimeBackend
        ? m_runtimeBackend->diagnosticContext()
        : QStringLiteral("connection_state=detached capability=disconnected generation=0");
    qInfo().noquote()
        << QStringLiteral("typhon_minimize_anchor event=%1 task_key=%2 window_id=%3 reason=%4 %5")
               .arg(eventName, safeTaskKey, safeWindowId, reasonName, diagnosticContext);
}

void DockController::reconcileRuntimeActionFailure(Astrea::Dock::DockRuntimeActionError error)
{
    Q_UNUSED(error);
    if (m_runtimeBackend && m_runtimeBackend->runtimeReady()) {
        const Astrea::Dock::DockRuntimeSnapshot snapshot = m_runtimeBackend->snapshot();
        if (snapshot.authoritative) {
            applyRuntimeSnapshot(snapshot);
            return;
        }
        clearRuntimeState();
    } else {
        clearRuntimeState();
    }
}
