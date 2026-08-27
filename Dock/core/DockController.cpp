#include "core/DockController.hpp"

#include "platform/typhon/TyphonToplevelConnection.hpp"
#include "services/DockConfigValidation.hpp"

#include <QDebug>

#include <algorithm>

namespace {

bool sameConfig(const DockConfig &left, const DockConfig &right)
{
    return left.iconSize == right.iconSize
        && left.bottomMargin == right.bottomMargin
        && left.panelPadding == right.panelPadding
        && left.itemSpacing == right.itemSpacing
        && left.hoverEffect == right.hoverEffect
        && qFuzzyCompare(left.magnificationScale, right.magnificationScale)
        && qFuzzyCompare(left.magnificationRadius, right.magnificationRadius)
        && left.pins == right.pins;
}

} // namespace

DockController::DockController(ApplicationLauncher *launcher, DesktopEntryCatalog *catalog,
                               DockConfigPersistence *persistence,
                               QObject *parent)
    : QObject(parent), m_launcher(launcher), m_persistence(persistence), m_catalog(catalog),
      m_catalogSnapshot(std::make_shared<const DesktopEntrySnapshot>())
{
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

void DockController::applyConfig(const DockConfig &config)
{
    const bool changed = !sameConfig(m_config, config);
    m_config = config;
    m_model.setPins(m_config.pins);
    projectRuntime();
    if (changed)
        emit configChanged();
    emit modelChanged();
    updateVisibility();
}

void DockController::setComponentEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
    updateVisibility();
}

void DockController::setCatalogSnapshot(std::shared_ptr<const DesktopEntrySnapshot> snapshot)
{
    if (!snapshot)
        snapshot = std::make_shared<const DesktopEntrySnapshot>();
    m_catalogSnapshot = std::move(snapshot);
    m_model.setCatalogSnapshot(m_catalogSnapshot);
    projectRuntime();
    emit modelChanged();
    updateVisibility();
}

void DockController::attachTyphonConnection(TyphonToplevelConnection *connection)
{
    for (const QMetaObject::Connection &binding : std::as_const(m_typhonConnections))
        disconnect(binding);
    m_typhonConnections.clear();

    if (!connection) {
        m_typhonConnection = nullptr;
        m_pendingActivations.clear();
        m_pendingWindowActions.clear();
        clearTyphonRuntime();
        return;
    }

    m_typhonConnection = connection;

    m_typhonConnections.append(connect(connection, &TyphonToplevelConnection::snapshotChanged,
                                      this, [this](const Astrea::Typhon::Snapshot &snapshot) {
        if (snapshot.connectionGeneration == 0 && snapshot.revision == 0
            && snapshot.windows.isEmpty()) {
            clearTyphonRuntime();
        } else {
            applyTyphonSnapshot(snapshot);
        }
    }));
    m_typhonConnections.append(connect(connection, &TyphonToplevelConnection::stateChanged,
                                      this, [this](TyphonConnectionState state) {
        if (state != TyphonConnectionState::Ready)
            clearTyphonRuntime();
    }));
    m_typhonConnections.append(connect(connection, &TyphonToplevelConnection::actionFinished,
                                      this, &DockController::onTyphonActionFinished));
    m_typhonConnections.append(connect(connection, &TyphonToplevelConnection::actionFailed,
                                      this, &DockController::onTyphonActionFailed));

    if (connection->state() == TyphonConnectionState::Ready && connection->hasSnapshot())
        applyTyphonSnapshot(connection->snapshot());
    else
        clearTyphonRuntime();
}

void DockController::applyTyphonSnapshot(const Astrea::Typhon::Snapshot &snapshot)
{
    m_runtimeSnapshot = snapshot;
    m_runtimeKnown = true;
    projectRuntime();
    emit modelChanged();
}

void DockController::clearTyphonRuntime()
{
    const bool changed = m_runtimeKnown || m_runtimeSnapshot.has_value() || !m_runtimeStates.isEmpty();
    m_runtimeKnown = false;
    m_runtimeSnapshot.reset();
    m_runtimeStates.clear();
    m_model.clearRuntimeProjection();
    if (changed)
        emit modelChanged();
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

QVector<Astrea::Typhon::Toplevel> DockController::windowsForDesktopFileName(
    const QString &desktopFileName) const
{
    QVector<Astrea::Typhon::Toplevel> result;
    const auto state = m_runtimeStates.constFind(desktopFileName);
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

bool DockController::requestExactWindowAction(const QString &desktopFileName,
                                              const QString &windowId,
                                              Astrea::Typhon::ToplevelAction action)
{
    const auto state = m_runtimeStates.constFind(desktopFileName);
    const auto windows = windowsForDesktopFileName(desktopFileName);
    const bool exactWindowIsLive = std::any_of(windows.cbegin(), windows.cend(),
                                               [&windowId](const auto &window) {
        return window.id == windowId;
    });
    if (state == m_runtimeStates.constEnd() || !state->windowIds.contains(windowId)
        || !exactWindowIsLive) {
        setLastError(QStringLiteral("The selected window is no longer available"));
        return false;
    }
    if (!m_typhonConnection) {
        setLastError(QStringLiteral("Typhon window actions are unavailable"));
        return false;
    }

    const quint64 token = ++m_nextActivationToken;
    m_pendingWindowActions.insert(token, PendingWindowAction{desktopFileName, windowId, action});
    const auto error = m_typhonConnection->requestAction(windowId, action, token);
    if (error.has_value()) {
        m_pendingWindowActions.remove(token);
        setLastError(QStringLiteral("The selected window action was rejected"));
        return false;
    }
    return true;
}

bool DockController::activateWindow(const QString &desktopFileName, const QString &windowId)
{
    return requestExactWindowAction(desktopFileName, windowId,
                                    Astrea::Typhon::ToplevelAction::Activate);
}

bool DockController::closeWindow(const QString &desktopFileName, const QString &windowId)
{
    return requestExactWindowAction(desktopFileName, windowId,
                                    Astrea::Typhon::ToplevelAction::Close);
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

    const QString key = item.desktopFileName;
    if (activateRunning && item.runtimeKnown && item.running) {
        const auto state = m_runtimeStates.constFind(key);
        if (state == m_runtimeStates.constEnd() || state->windowIds.isEmpty()) {
            qInfo("Dock activation suppressed for running application '%s' without a live target",
                  qPrintable(key));
            return;
        }
        if (!m_typhonConnection) {
            qInfo("Dock activation suppressed for running application '%s' without Typhon actions",
                  qPrintable(key));
            return;
        }

        const quint64 activationToken = ++m_nextActivationToken;
        const QString targetWindowId = state->windowIds.constFirst();
        m_pendingActivations.insert(activationToken, key);
        const auto error = m_typhonConnection->requestAction(
            targetWindowId, Astrea::Typhon::ToplevelAction::Activate, activationToken);
        if (error.has_value()) {
            m_pendingActivations.remove(activationToken);
            reconcileTyphonActionFailure(error.value());
        } else {
            qInfo("Dock activating exact Typhon window '%s' for '%s'",
                  qPrintable(targetWindowId), qPrintable(key));
        }
        return;
    }
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
        return;
    m_requestedVisible = true;
    updateVisibility();
}

void DockController::hide()
{
    if (!m_requestedVisible)
        return;
    m_requestedVisible = false;
    updateVisibility();
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

void DockController::onTyphonActionFinished(
    quint64 token, Astrea::Typhon::ToplevelAction action,
    Astrea::Typhon::ToplevelActionResult result)
{
    const auto pendingWindow = m_pendingWindowActions.find(token);
    if (pendingWindow != m_pendingWindowActions.end()) {
        const PendingWindowAction operation = pendingWindow.value();
        m_pendingWindowActions.erase(pendingWindow);
        if (result == Astrea::Typhon::ToplevelActionResult::Unavailable)
            setLastError(QStringLiteral("The selected window is no longer available"));
        Q_UNUSED(operation);
        return;
    }
    if (action != Astrea::Typhon::ToplevelAction::Activate
        || !m_pendingActivations.contains(token))
        return;
    const QString key = m_pendingActivations.take(token);
    if (result == Astrea::Typhon::ToplevelActionResult::Unavailable) {
        qInfo("Dock activation unavailable for '%s'; reconciling without launch",
              qPrintable(key));
        reconcileTyphonActionFailure(Astrea::Typhon::ToplevelActionError::ToplevelNotLive);
    }
}

void DockController::onTyphonActionFailed(
    quint64 token, Astrea::Typhon::ToplevelAction action,
    Astrea::Typhon::ToplevelActionError error)
{
    if (m_pendingWindowActions.contains(token)) {
        m_pendingWindowActions.remove(token);
        setLastError(QStringLiteral("The selected window action failed"));
        return;
    }
    if (action != Astrea::Typhon::ToplevelAction::Activate
        || !m_pendingActivations.contains(token))
        return;
    m_pendingActivations.remove(token);
    reconcileTyphonActionFailure(error);
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
        m_runtimeStates.clear();
        m_model.clearRuntimeProjection();
        return;
    }

    const Astrea::Typhon::DockApplicationRuntimeProjection projection =
        m_runtimeProjector.project(*m_runtimeSnapshot, m_catalogSnapshot);
    m_runtimeStates = projection.states;
    m_model.applyRuntimeProjection(projection, true);
}

void DockController::reconcileTyphonActionFailure(Astrea::Typhon::ToplevelActionError error)
{
    Q_UNUSED(error);
    if (m_typhonConnection && m_typhonConnection->state() == TyphonConnectionState::Ready
        && m_typhonConnection->hasSnapshot()) {
        applyTyphonSnapshot(m_typhonConnection->snapshot());
    } else {
        clearTyphonRuntime();
    }
}
