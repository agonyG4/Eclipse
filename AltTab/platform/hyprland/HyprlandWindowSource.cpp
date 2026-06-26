#include "platform/hyprland/HyprlandWindowSource.hpp"
#include "platform/hyprland/HyprlandCommand.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QDateTime>

#include <algorithm>

namespace {

QString eventNameFromLine(const QString &line)
{
    const int sep = line.indexOf(QStringLiteral(">>"));
    if (sep < 0)
        return {};
    return line.left(sep).trimmed();
}

QString eventPayloadFromLine(const QString &line)
{
    const int sep = line.indexOf(QStringLiteral(">>"));
    if (sep < 0)
        return {};
    return line.mid(sep + 2);
}

bool isRefreshEvent(const QString &name)
{
    // Exact event name matching - no broad startsWith checks
    static const QSet<QString> kRefreshEvents = {
        QStringLiteral("workspace"),
        QStringLiteral("workspacev2"),
        QStringLiteral("focusedmon"),
        QStringLiteral("focusedmonv2"),
        QStringLiteral("activewindow"),
        QStringLiteral("activewindowv2"),
        QStringLiteral("openwindow"),
        QStringLiteral("closewindow"),
        QStringLiteral("movewindow"),
        QStringLiteral("movewindowv2"),
        QStringLiteral("windowtitle"),
        QStringLiteral("windowtitlev2"),
        QStringLiteral("monitoradded"),
        QStringLiteral("monitoraddedv2"),
        QStringLiteral("monitorremoved"),
        QStringLiteral("monitorremovedv2"),
        QStringLiteral("configreloaded")
    };
    return kRefreshEvents.contains(name);
}

bool workspaceIdValid(const QString &workspaceId)
{
    bool ok = false;
    const int value = workspaceId.trimmed().toInt(&ok);
    return ok && value > 0;
}

enum class CommandResponseType {
    Success,
    LuaError,
    UnknownCommand,
    InvalidSelector,
    Empty,
    ConnectionError,
    Timeout,
    Oversized
};

struct ParsedCommandResponse {
    CommandResponseType type = CommandResponseType::Empty;
    QString detail;
};

ParsedCommandResponse parseCommandResponse(const QByteArray &raw)
{
    ParsedCommandResponse pr;
    if (raw.isEmpty()) {
        pr.type = CommandResponseType::Empty;
        pr.detail = QStringLiteral("empty response");
        return pr;
    }

    const QString text = QString::fromUtf8(raw).trimmed();
    if (text.isEmpty()) {
        pr.type = CommandResponseType::Empty;
        pr.detail = QStringLiteral("empty response (whitespace only)");
        return pr;
    }

    if (text.contains(QStringLiteral("lua error"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("attempt to"), Qt::CaseInsensitive)) {
        pr.type = CommandResponseType::LuaError;
        pr.detail = text;
        return pr;
    }
    if (text.contains(QStringLiteral("unknown request"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("unknown command"), Qt::CaseInsensitive)) {
        pr.type = CommandResponseType::UnknownCommand;
        pr.detail = text;
        return pr;
    }
    if (text.contains(QStringLiteral("invalid"), Qt::CaseInsensitive)) {
        pr.type = CommandResponseType::InvalidSelector;
        pr.detail = text;
        return pr;
    }
    pr.type = CommandResponseType::Success;
    return pr;
}

} // namespace

HyprlandWindowSource::HyprlandWindowSource(QObject *parent)
    : CompositorBackend(parent)
{
    m_socket = new HyprlandSocket(this);
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(50);

    connect(m_debounceTimer, &QTimer::timeout, this, &HyprlandWindowSource::triggerDebouncedRefresh);

    connect(m_socket, &HyprlandSocket::eventSocketConnected, this, [this]() {
        setBackendState(BackendState::LoadingInitialSnapshot);
        requestInitialSnapshot();
    });

    connect(m_socket, &HyprlandSocket::connectionLost, this, &HyprlandWindowSource::onConnectionLost);

    m_socket->setEventCallback([this](const QString &eventLine) {
        onEvent(eventLine);
    });
}

HyprlandWindowSource::~HyprlandWindowSource()
{
    stop();
}

void HyprlandWindowSource::start()
{
    if (m_started)
        return;
    m_started = true;
    ++m_lifecycleGeneration;
    setBackendState(BackendState::Starting);
    connectSockets();
}

void HyprlandWindowSource::stop()
{
    if (!m_started && m_state == BackendState::Stopped)
        return;

    m_started = false;
    ++m_lifecycleGeneration;
    m_activationInFlight = false;
    m_debounceTimer->stop();
    if (m_socket)
        m_socket->stop();
    m_cachedSnapshot = {};
    setBackendState(BackendState::Stopped);
}

BackendDescriptor HyprlandWindowSource::descriptor() const
{
    return { QStringLiteral("hyprland"), 1 };
}

BackendState HyprlandWindowSource::state() const
{
    return m_state;
}

QVector<BackendCapability> HyprlandWindowSource::capabilities() const
{
    return {
        BackendCapability::WindowList,
        BackendCapability::EventStream,
        BackendCapability::WindowActivation,
        BackendCapability::ActiveWindow,
        BackendCapability::ActiveOutput
    };
}

std::optional<WindowSnapshot> HyprlandWindowSource::cachedSnapshot() const
{
    if (m_cachedSnapshot.windows.isEmpty())
        return std::nullopt;
    return m_cachedSnapshot;
}

void HyprlandWindowSource::connectSockets()
{
    setBackendState(BackendState::ConnectingEvents);
    if (m_socket)
        m_socket->start();
}

void HyprlandWindowSource::setBackendState(BackendState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(m_state);
}

void HyprlandWindowSource::requestInitialSnapshot()
{
    if (!m_socket || m_state != BackendState::LoadingInitialSnapshot)
        return;

    const quint64 requestGeneration = m_lifecycleGeneration;
    struct SharedState { QAtomicInt remaining = {4}; };
    auto state = std::make_shared<SharedState>();

    auto onComplete = [this, requestGeneration, state]() {
        if (requestGeneration != m_lifecycleGeneration)
            return;
        if (state->remaining.fetchAndAddRelaxed(-1) <= 1 && m_state == BackendState::LoadingInitialSnapshot) {
            setBackendState(BackendState::Ready);
        }
    };

    // Load clients
    m_socket->sendCommand(HyprlandRequest::jsonInfoRequest(u"clients"),
        [this, requestGeneration, onComplete](const HyprlandCommandResult &result) {
            if (requestGeneration != m_lifecycleGeneration)
                return;
            if (result.success && !result.rawReply.isEmpty()) {
                const QVector<WindowInfo> windows = parseClientsFromJson(result.rawReply);
                m_cachedSnapshot.windows = windows;
                m_cachedSnapshot.revision++;
                m_cachedSnapshot.backendGeneration = m_lifecycleGeneration;
                updateSnapshotStateFromWindows(windows);
            }
            onComplete();
        });

    // Load monitors
    m_socket->sendCommand(HyprlandRequest::jsonInfoRequest(u"monitors"),
        [this, requestGeneration, onComplete](const HyprlandCommandResult &result) {
            if (requestGeneration != m_lifecycleGeneration)
                return;
            if (result.success && !result.rawReply.isEmpty()) {
                const QJsonDocument doc = QJsonDocument::fromJson(result.rawReply);
                if (doc.isArray()) {
                    for (const auto &val : doc.array()) {
                        if (!val.isObject()) continue;
                        const QJsonObject mon = val.toObject();
                        const QString name = mon.value(QStringLiteral("name")).toString();
                        if (!name.isEmpty()) {
                            if (mon.value(QStringLiteral("focused")).toBool()) {
                                m_cachedSnapshot.activeOutputId = OutputId{name};
                                m_cachedSnapshot.focusedMonitorName = name;
                            } else if (m_cachedSnapshot.focusedMonitorName.isEmpty()) {
                                m_cachedSnapshot.focusedMonitorName = name;
                            }
                        }
                    }
                }
            }
            onComplete();
        });

    // Load active window
    m_socket->sendCommand(HyprlandRequest::jsonInfoRequest(u"activewindow"),
        [this, requestGeneration, onComplete](const HyprlandCommandResult &result) {
            if (requestGeneration != m_lifecycleGeneration)
                return;
            if (result.success && !result.rawReply.isEmpty()) {
                const QJsonDocument doc = QJsonDocument::fromJson(result.rawReply);
                if (doc.isObject()) {
                    const auto obj = doc.object();
                    const QString addr = obj.value(QStringLiteral("address")).toString();
                    if (!addr.isEmpty()) {
                        const QString norm = HyprlandCommand::normalizeAddress(addr);
                        if (!norm.isEmpty())
                            m_cachedSnapshot.activeWindowId = WindowId{norm.mid(QStringLiteral("address:").size())};
                    }
                }
            }
            onComplete();
        });

    // Load active workspace
    m_socket->sendCommand(HyprlandRequest::jsonInfoRequest(u"activeworkspace"),
        [this, requestGeneration, onComplete](const HyprlandCommandResult &result) {
            if (requestGeneration != m_lifecycleGeneration)
                return;
            if (result.success && !result.rawReply.isEmpty()) {
                const QJsonDocument doc = QJsonDocument::fromJson(result.rawReply);
                if (doc.isObject()) {
                    const auto obj = doc.object();
                    const int wsId = static_cast<int>(obj.value(QStringLiteral("id")).toDouble());
                    if (wsId > 0)
                        m_cachedSnapshot.activeWorkspaceId = WorkspaceId{QString::number(wsId)};
                }
            }
            onComplete();
        });
}

void HyprlandWindowSource::requestSnapshot(RequestToken token)
{
    if (!m_socket)
        return;

    const quint64 requestGeneration = m_lifecycleGeneration;
    m_socket->sendCommand(HyprlandRequest::jsonInfoRequest(u"clients"),
        [this, token, requestGeneration](const HyprlandCommandResult &result) {
            if (requestGeneration != m_lifecycleGeneration)
                return;

            if (!result.success || result.rawReply.isEmpty()) {
                if (!m_cachedSnapshot.windows.isEmpty())
                    emit snapshotReady(token, m_cachedSnapshot);
                return;
            }

            const QVector<WindowInfo> windows = parseClientsFromJson(result.rawReply);
            m_cachedSnapshot.windows = windows;
            m_cachedSnapshot.revision++;
            m_cachedSnapshot.backendGeneration = m_lifecycleGeneration;
            updateSnapshotStateFromWindows(windows);
            emit snapshotReady(token, m_cachedSnapshot);
        });
}

void HyprlandWindowSource::activateWindow(ActivationRequest request)
{
    if (m_activationInFlight || !m_socket) {
        ActivationResult result;
        result.success = false;
        if (m_activationInFlight)
            result.error = QStringLiteral("activation already in flight");
        else
            result.error = QStringLiteral("no socket");
        emit activationFinished(request.token, result);
        return;
    }

    const QString normalizedAddress = HyprlandCommand::normalizeAddress(request.windowId.value);
    if (normalizedAddress.isEmpty()) {
        ActivationResult result;
        result.success = false;
        result.error = QStringLiteral("invalid window address");
        emit activationFinished(request.token, result);
        return;
    }

    const auto it = std::find_if(m_cachedSnapshot.windows.cbegin(), m_cachedSnapshot.windows.cend(),
                                 [&](const WindowInfo &window) { return window.windowId.value == request.windowId.value; });
    if (it == m_cachedSnapshot.windows.cend()) {
        ActivationResult result;
        result.success = false;
        result.error = QStringLiteral("selected window not found");
        emit activationFinished(request.token, result);
        return;
    }

    const WindowInfo target = *it;
    const QString targetWorkspace = target.workspaceId.value.trimmed();
    if (!workspaceIdValid(targetWorkspace)) {
        ActivationResult result;
        result.success = false;
        result.error = QStringLiteral("invalid workspace id");
        emit activationFinished(request.token, result);
        return;
    }

    m_activationInFlight = true;
    const quint64 gen = ++m_activationGeneration;

    const auto complete = [this, request, gen](bool success, const QString &error) {
        if (gen != m_activationGeneration)
            return;
        m_activationInFlight = false;
        ActivationResult result;
        result.success = success;
        result.error = error;
        emit activationFinished(request.token, result);
        if (!success)
            emit backendError(BackendError::CompositorRejected);
    };

    const auto focusWindow = [this, normalizedAddress, complete]() {
        const QByteArray cmd = HyprlandRequest::focusWindowRequest(normalizedAddress);
        if (cmd.isEmpty()) {
            complete(false, QStringLiteral("invalid normalized address"));
            return;
        }
        m_socket->sendCommand(cmd, [complete](const HyprlandCommandResult &result) {
            if (!result.success) {
                complete(false, QStringLiteral("socket error: ") + result.errorMessage);
                return;
            }
            const auto parsed = parseCommandResponse(result.rawReply);
            if (parsed.type != CommandResponseType::Success) {
                complete(false, QStringLiteral("window focus failed"));
                return;
            }
            complete(true, {});
        });
    };

    const bool workspaceNeeded = m_cachedSnapshot.activeWorkspaceId.value.trimmed() != targetWorkspace;
    if (!workspaceNeeded) {
        focusWindow();
        return;
    }

    const qint64 wsInt = targetWorkspace.toLongLong();
    const QByteArray wsCmd = HyprlandRequest::focusWorkspaceRequest(wsInt);
    if (wsCmd.isEmpty()) {
        complete(false, QStringLiteral("invalid workspace in request"));
        return;
    }

    m_socket->sendCommand(wsCmd, [focusWindow, complete, targetWorkspace](const HyprlandCommandResult &result) {
        if (!result.success) {
            complete(false, QStringLiteral("workspace socket error: ") + result.errorMessage);
            return;
        }
        const auto parsed = parseCommandResponse(result.rawReply);
        if (parsed.type != CommandResponseType::Success) {
            complete(false, QStringLiteral("workspace focus failed"));
            return;
        }
        focusWindow();
    });
}

void HyprlandWindowSource::updateSnapshotStateFromWindows(const QVector<WindowInfo> &windows)
{
    for (const auto &window : windows) {
        if (!window.isActive)
            continue;
        m_cachedSnapshot.activeWindowId = window.windowId;
        m_cachedSnapshot.activeWorkspaceId = window.workspaceId;
        if (!window.outputId.isEmpty())
            m_cachedSnapshot.activeOutputId = window.outputId;
        return;
    }
}

void HyprlandWindowSource::onEvent(const QString &eventLine)
{
    const QString eventName = eventNameFromLine(eventLine);
    if (eventName.isEmpty())
        return;

    const QString payload = eventPayloadFromLine(eventLine);

    if (eventName == QStringLiteral("activewindow") || eventName == QStringLiteral("activewindowv2")) {
        const QString address = payload.section(QLatin1Char(','), 0, 0).trimmed();
        const QString normalized = HyprlandCommand::normalizeAddress(address);
        if (!normalized.isEmpty())
            m_cachedSnapshot.activeWindowId = WindowId{normalized.mid(QStringLiteral("address:").size())};
    } else if (eventName == QStringLiteral("workspace") || eventName == QStringLiteral("workspacev2")) {
        const QString workspace = payload.section(QLatin1Char(','), 0, 0).trimmed();
        if (workspaceIdValid(workspace))
            m_cachedSnapshot.activeWorkspaceId = WorkspaceId{workspace};
    } else if (eventName == QStringLiteral("focusedmon") || eventName == QStringLiteral("focusedmonv2")) {
        const QString monitor = payload.section(QLatin1Char(','), 0, 0).trimmed();
        if (!monitor.isEmpty()) {
            m_cachedSnapshot.activeOutputId = OutputId{monitor};
            m_cachedSnapshot.focusedMonitorName = monitor;
        }
    } else if (eventName == QStringLiteral("monitoradded") || eventName == QStringLiteral("monitoraddedv2")) {
        const QString monitor = payload.section(QLatin1Char(','), 0, 0).trimmed();
        if (!monitor.isEmpty() && m_cachedSnapshot.activeOutputId.isEmpty())
            m_cachedSnapshot.activeOutputId = OutputId{monitor};
    }

    if (isRefreshEvent(eventName))
        m_debounceTimer->start();
}

void HyprlandWindowSource::onConnectionLost()
{
    setBackendState(BackendState::Disconnected);
}

void HyprlandWindowSource::triggerDebouncedRefresh()
{
    if (!m_socket)
        return;

    const quint64 requestGeneration = m_lifecycleGeneration;
    m_socket->sendCommand(HyprlandRequest::jsonInfoRequest(u"clients"),
        [this, requestGeneration](const HyprlandCommandResult &result) {
            if (requestGeneration != m_lifecycleGeneration)
                return;
            if (!result.success || result.rawReply.isEmpty())
                return;

            const QVector<WindowInfo> windows = parseClientsFromJson(result.rawReply);
            m_cachedSnapshot.windows = windows;
            m_cachedSnapshot.revision++;
            m_cachedSnapshot.backendGeneration = m_lifecycleGeneration;
            updateSnapshotStateFromWindows(windows);
            emit snapshotChanged(m_cachedSnapshot);
        });
}

QVector<WindowInfo> HyprlandWindowSource::parseClientsFromJson(const QByteArray &data)
{
    QVector<WindowInfo> windows;
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray())
        return windows;

    const QJsonArray arr = doc.array();
    windows.reserve(arr.size());

    for (const auto &val : arr) {
        if (!val.isObject())
            continue;
        WindowInfo info = WindowInfo::fromJson(val.toObject());
        if (!info.windowId.isEmpty())
            windows.append(info);
    }

    return windows;
}
