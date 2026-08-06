#include "core/AltTabController.hpp"
#include "core/AltTabWindowModel.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QIcon>
#include <QTimerEvent>
#include <QDebug>

AltTabController::AltTabController(CompositorBackend *backend, AppIdentityResolver *identityResolver,
                                   QObject *parent)
    : QObject(parent), m_backend(backend), m_identityResolver(identityResolver)
{
    if (m_backend) {
        connect(m_backend, &CompositorBackend::snapshotChanged, this, [this](const WindowSnapshot &snapshot) {
            if (m_state == State::Open) {
                onSnapshotReady(0, snapshot);
            }
        });

        connect(m_backend, &CompositorBackend::snapshotReady, this, &AltTabController::onSnapshotReady);

        connect(m_backend, &CompositorBackend::stateChanged, this, &AltTabController::onBackendStateChanged);

        connect(m_backend, &CompositorBackend::activationFinished,
                this, &AltTabController::onActivationFinished);
    }

    connect(m_identityResolver, &AppIdentityResolver::identityResolved,
            this, &AltTabController::onIdentityResolved);

    connect(&m_model, &AltTabWindowModel::countChanged,
            this, &AltTabController::windowCountChanged);
}

void AltTabController::onBackendStateChanged(BackendState state)
{
    if (state == BackendState::Ready && m_state == State::Opening) {
        requestOpeningSnapshot();
        return;
    }

    if ((state == BackendState::Disconnected || state == BackendState::Unsupported
         || state == BackendState::Stopped) && m_state != State::Hidden) {
        cancel();
    }
}

QString AltTabController::stateName() const {
    switch (m_state) {
    case State::Hidden: return QStringLiteral("hidden");
    case State::Opening: return QStringLiteral("opening");
    case State::Open: return QStringLiteral("open");
    case State::Committing: return QStringLiteral("committing");
    case State::Closing: return QStringLiteral("closing");
    }
    return QStringLiteral("unknown");
}

void AltTabController::step(int direction) {
    switch (m_state) {
    case State::Hidden:
    case State::Closing:
        cleanup();
        m_pendingOpenOffset = direction;
        setState(State::Opening);
        m_openingGeneration++;
        m_timerGeneration = m_openingGeneration;
        m_snapshotTimeout.start(3000, this);
        if (m_backend) {
            if (m_backend->state() == BackendState::Ready) {
                requestOpeningSnapshot();
            } else {
                m_backend->start();
                if (m_backend->state() == BackendState::Ready)
                    requestOpeningSnapshot();
            }
        }
        break;

    case State::Opening:
        m_pendingOpenOffset += direction;
        break;

    case State::Open: {
        if (m_model.count() == 0)
            return;
        const int next = (m_selectedIndex + direction + m_model.count()) % m_model.count();
        setSelectedIndex(next);
        break;
    }

    case State::Committing:
        break;
    }
}

void AltTabController::preview(int index) {
    if (m_state != State::Open)
        return;
    if (index < 0 || index >= m_model.count())
        return;
    setSelectedIndex(index);
}

void AltTabController::commit() {
    switch (m_state) {
    case State::Hidden:
    case State::Closing:
        break;

    case State::Opening:
        m_commitAfterLoad = true;
        break;

    case State::Open:
        if (!m_commitInFlight)
            doCommit();
        break;

    case State::Committing:
        break;
    }
}

void AltTabController::commitIndex(int index) {
    if (m_state != State::Open)
        return;
    if (index < 0 || index >= m_model.count())
        return;
    m_selectedIndex = index;
    emit selectedIndexChanged();
    doCommit();
}

void AltTabController::cancel() {
    if (m_state == State::Hidden)
        return;

    setState(State::Closing);
    cleanup();
    setState(State::Hidden);
    emit closed();
}

void AltTabController::show() {
    if (m_state != State::Hidden && m_state != State::Closing)
        return;
    cleanup();
    setState(State::Opening);
    m_openingGeneration++;
    m_timerGeneration = m_openingGeneration;
    m_snapshotTimeout.start(3000, this);
    if (m_backend) {
        if (m_backend->state() == BackendState::Ready) {
            requestOpeningSnapshot();
        } else {
            m_backend->start();
            if (m_backend->state() == BackendState::Ready)
                requestOpeningSnapshot();
        }
    }
}

void AltTabController::hide() {
    cancel();
}

void AltTabController::reloadWindows() {
    if (m_backend) {
        m_backend->requestSnapshot(m_openingGeneration);
    }
}

void AltTabController::setSelectedIndex(int index) {
    const int bounded = (index < 0 || m_model.count() == 0)
        ? -1
        : qBound(0, index, m_model.count() - 1);
    if (m_selectedIndex == bounded)
        return;
    m_selectedIndex = bounded;
    m_model.setSelectedIndex(bounded);
    emit selectedIndexChanged();
}

void AltTabController::setState(State s) {
    if (m_state == s)
        return;
    const bool oldOpen = isOpen();
    m_state = s;
    emit stateChanged();
    if (oldOpen != isOpen()) {
        emit openChanged();
    }
}

void AltTabController::requestOpeningSnapshot()
{
    if (!m_backend || m_state != State::Opening
        || m_lastOpeningSnapshotRequestGeneration == m_openingGeneration) {
        return;
    }

    m_lastOpeningSnapshotRequestGeneration = m_openingGeneration;
    m_backend->requestSnapshot(m_openingGeneration);
}

void AltTabController::onSnapshotReady(RequestToken token, const WindowSnapshot &snapshot) {
    if (m_state != State::Opening && m_state != State::Open)
        return;
    if (m_state == State::Opening && token != m_openingGeneration)
        return;

    QVector<WindowInfo> filtered;
    filtered.reserve(snapshot.windows.size());
    for (const auto &w : snapshot.windows) {
        if (w.windowId.isEmpty())
            continue;
        if (w.skipSwitcher || w.isSpecial || w.isHidden)
            continue;
        const QString workspaceValue = w.workspaceId.value.trimmed();
        if (!workspaceValue.isEmpty()) {
            bool validWorkspace = false;
            const int workspaceId = workspaceValue.toInt(&validWorkspace);
            if (!validWorkspace || workspaceId <= 0)
                continue;
        }
        filtered.append(w);
    }

    std::sort(filtered.begin(), filtered.end(), [](const WindowInfo &a, const WindowInfo &b) {
        if (a.focusHistoryId != b.focusHistoryId)
            return a.focusHistoryId < b.focusHistoryId;
        return a.displayName < b.displayName;
    });

    if (filtered.isEmpty()) {
        cancel();
        return;
    }

    m_snapshotTimeout.stop();

    m_model.setWindows(filtered);

    if (m_state == State::Opening) {
        finishOpening();
    } else {
        m_selectedIndex = m_model.selectedIndex();
    }

    resolveIconsForWindows(filtered);
}

void AltTabController::finishOpening() {
    if (m_state != State::Opening)
        return;

    const int count = m_model.count();
    if (count == 0) {
        cancel();
        return;
    }

    int activeIndex = -1;
    for (int i = 0; i < count; ++i) {
        if (m_model.at(i).focusHistoryId == 0) {
            activeIndex = i;
            break;
        }
    }

    if (activeIndex < 0)
        activeIndex = 0;

    if (count > 1) {
        m_selectedIndex = (activeIndex + m_pendingOpenOffset + count) % count;
    } else {
        m_selectedIndex = 0;
    }
    m_pendingOpenOffset = 0;
    m_model.setSelectedIndex(m_selectedIndex);
    emit selectedIndexChanged();

    if (!m_surfaceVisible) {
        m_surfaceVisible = true;
        emit surfaceVisibleChanged();
    }
    setState(State::Open);
    emit focusRequested();

    if (m_commitAfterLoad) {
        m_commitAfterLoad = false;
        QTimer::singleShot(0, this, &AltTabController::doCommit);
    }
}

void AltTabController::doCommit() {
    if (m_state != State::Open)
        return;

    const int idx = m_selectedIndex;
    if (idx < 0 || idx >= m_model.count()) {
        cancel();
        return;
    }

    const WindowInfo target = m_model.at(idx);
    m_commitInFlight = true;
    setState(State::Committing);

    m_model.clear();
    setSelectedIndex(-1);
    m_surfaceVisible = false;
    emit surfaceVisibleChanged();

    if (m_backend) {
        ActivationRequest request;
        request.windowId = target.windowId;
        request.token = ++m_activationRequestGen;
        m_backend->activateWindow(request);
    } else {
        onCommitComplete(true, {});
    }
}

void AltTabController::onActivationFinished(ActivationToken token, ActivationResult result) {
    if (token != m_activationRequestGen)
        return;
    m_lastActivationResult = result;
    onCommitComplete(result.success, result.error);
}

void AltTabController::onCommitComplete(bool success, const QString &error) {
    if (m_state != State::Committing)
        return;
    m_commitInFlight = false;
    setState(State::Hidden);
    emit closed();
    if (!success)
        qWarning("Activation failed for token %llu: %s",
                 static_cast<unsigned long long>(m_activationRequestGen), qPrintable(error));
}

void AltTabController::resolveIconsForWindows(const QVector<WindowInfo> &windows) {
    for (const auto &w : windows) {
        WindowIdentityInput input;
        input.address = w.windowId.value;
        input.pid = w.pid;
        input.className = w.className;
        input.initialClass = w.initialClass;
        input.title = w.title;
        input.initialTitle = w.initialTitle;
        input.workspaceId = w.workspaceId.value.trimmed().isEmpty() ? -1 : w.workspaceIdInt();
        input.openGeneration = w.backendGeneration != 0 ? w.backendGeneration : m_openingGeneration;
        input.metadataFingerprint = w.backendGeneration != 0
            ? (w.appId + QLatin1Char('|') + w.title) : w.metaKey();
        input.themeRevision = m_identityResolver->themeRevision();
        input.desktopIndexRevision = m_identityResolver->desktopIndexRevision();
        input.steamIndexRevision = m_identityResolver->steamIndexRevision();

        m_identityResolver->resolveAsync(input, input.openGeneration);
    }
}

void AltTabController::onIdentityResolved(const QString &address, const AppIdentity &identity) {
    if (m_state != State::Open && m_state != State::Opening)
        return;

    const int row = m_model.indexOf(address);
    if (row < 0)
        return;

    const WindowInfo current = m_model.at(row);
    const quint64 expectedGeneration = current.backendGeneration != 0
        ? current.backendGeneration : m_openingGeneration;
    if (identity.openGeneration != expectedGeneration)
        return;
    if (current.pid != identity.pid)
        return;
    const QString expectedFingerprint = current.backendGeneration != 0
        ? (current.appId + QLatin1Char('|') + current.title) : current.metaKey();
    if (expectedFingerprint != identity.metadataFingerprint)
        return;
    if (identity.themeRevision != m_identityResolver->themeRevision())
        return;
    if (identity.desktopIndexRevision != m_identityResolver->desktopIndexRevision())
        return;
    if (identity.steamIndexRevision != m_identityResolver->steamIndexRevision())
        return;

    WindowInfo updated = current;
    updated.iconName = identity.iconName;
    updated.iconPath = identity.iconPath;
    updated.iconPending = identity.iconPending;
    updated.showFallbackText = identity.showFallbackText;
    if (!identity.displayName.isEmpty())
        updated.displayName = identity.displayName;

    m_model.updateWindow(updated);
}

void AltTabController::timerEvent(QTimerEvent *event) {
    if (event->timerId() == m_snapshotTimeout.timerId()) {
        m_snapshotTimeout.stop();
        if (m_state == State::Opening && m_timerGeneration == m_openingGeneration) {
            cancel();
        }
    }
}

void AltTabController::cleanup() {
    m_snapshotTimeout.stop();
    m_pendingOpenOffset = 0;
    m_commitAfterLoad = false;
    m_commitInFlight = false;
    m_model.clear();
    setSelectedIndex(-1);
    if (m_surfaceVisible) {
        m_surfaceVisible = false;
        emit surfaceVisibleChanged();
    }
}

QString AltTabController::buildStatusJson() const {
    QJsonObject obj;
    obj[QStringLiteral("running")] = true;
    obj[QStringLiteral("visible")] = m_surfaceVisible;
    obj[QStringLiteral("state")] = stateName();
    obj[QStringLiteral("windows")] = m_model.count();
    obj[QStringLiteral("windowCount")] = m_model.count();
    obj[QStringLiteral("selectedIndex")] = m_selectedIndex;
    obj[QStringLiteral("selectedAddress")] = (m_selectedIndex >= 0 && m_selectedIndex < m_model.count())
        ? m_model.at(m_selectedIndex).windowId.value : QString();
    obj[QStringLiteral("windowSource")] = m_backend ? m_backend->descriptor().name : QString();

    if (m_backend) {
        const BackendState bs = m_backend->state();
        obj[QStringLiteral("backendState")] = static_cast<int>(bs);
        const bool ready = (bs == BackendState::Ready);
        obj[QStringLiteral("windowSourceConnected")] = ready;

        QJsonObject backendObj;
        backendObj[QStringLiteral("state")] = static_cast<int>(bs);
        backendObj[QStringLiteral("stateName")] = [bs]() -> QString {
            switch (bs) {
                case BackendState::Stopped: return QStringLiteral("stopped");
                case BackendState::Starting: return QStringLiteral("starting");
                case BackendState::ConnectingEvents: return QStringLiteral("connectingEvents");
                case BackendState::LoadingInitialSnapshot: return QStringLiteral("loadingInitialSnapshot");
                case BackendState::Ready: return QStringLiteral("ready");
                case BackendState::Degraded: return QStringLiteral("degraded");
                case BackendState::Disconnected: return QStringLiteral("disconnected");
                case BackendState::Unsupported: return QStringLiteral("unsupported");
            }
            return QStringLiteral("unknown");
        }();
        obj[QStringLiteral("backend")] = backendObj;
    }

    obj[QStringLiteral("iconTheme")] = QIcon::themeName();
    obj[QStringLiteral("iconFallbackTheme")] = QIcon::fallbackThemeName();
    obj[QStringLiteral("lastActivationSuccess")] = m_lastActivationResult.success;
    if (!m_lastActivationResult.error.isEmpty())
        obj[QStringLiteral("lastActivationError")] = m_lastActivationResult.error;
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}
