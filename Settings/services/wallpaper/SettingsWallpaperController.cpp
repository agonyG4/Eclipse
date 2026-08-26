#include "services/wallpaper/SettingsWallpaperController.hpp"

#include "platform/paper/PaperProtocol.hpp"

#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QTimer>

#include <utility>

SettingsWallpaperController::SettingsWallpaperController(QString endpoint, QObject *parent)
    : QObject(parent)
    , m_endpoint(endpoint.isEmpty() ? defaultEndpoint() : std::move(endpoint))
{
    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        finishError(m_requestId,
                    QStringLiteral("paper-timeout"),
                    QStringLiteral("Paper wallpaper request timed out"));
    });
}

QString SettingsWallpaperController::defaultEndpoint()
{
    auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtime.isEmpty())
        runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    return runtime.isEmpty() ? QString() : QDir(runtime).filePath(QStringLiteral("astrea-shell/wallpaper.sock"));
}

bool SettingsWallpaperController::isSupportedFit(const QString &fit)
{
    const auto normalized = fit.trimmed().toLower();
    return normalized == QStringLiteral("cover") || normalized == QStringLiteral("contain")
        || normalized == QStringLiteral("stretch") || normalized == QStringLiteral("center")
        || normalized == QStringLiteral("tile");
}

QString SettingsWallpaperController::selectionFit() const
{
    if (isSupportedFit(m_configuredFit)) {
        return m_configuredFit.trimmed().toLower();
    }
    if (isSupportedFit(m_effectiveFit)) {
        return m_effectiveFit.trimmed().toLower();
    }
    return QStringLiteral("cover");
}

void SettingsWallpaperController::startRequest(const QString &action, const QJsonObject &argument)
{
    if (m_busy) {
        m_errorCode = QStringLiteral("paper-request-busy");
        m_errorMessage = QStringLiteral("A wallpaper request is already in progress");
        emit errorChanged();
        return;
    }
    const bool hadError = !m_errorCode.isEmpty() || !m_errorMessage.isEmpty();
    m_errorCode.clear();
    m_errorMessage.clear();
    if (hadError)
        emit errorChanged();
    ++m_requestId;
    const auto requestId = m_requestId;
    if (m_timeout)
        m_timeout->stop();
    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
        m_socket.clear();
    }
    m_readBuffer.clear();
    m_pendingAction = action;
    setBusy(true, action);

    if (m_endpoint.isEmpty()) {
        finishError(requestId,
                    QStringLiteral("paper-endpoint-unavailable"),
                    QStringLiteral("Paper runtime endpoint is unavailable"));
        return;
    }

    auto *socket = new QLocalSocket(this);
    m_socket = socket;
    connect(socket, &QLocalSocket::connected, this, [this, requestId, argument] {
        handleConnected(requestId, argument);
    });
    connect(socket, &QLocalSocket::readyRead, this, [this, requestId] {
        handleReadyRead(requestId);
    });
    connect(socket, &QLocalSocket::errorOccurred, this, [this, requestId](QLocalSocket::LocalSocketError) {
        if (requestId == m_requestId && m_busy) {
            finishError(requestId,
                        QStringLiteral("paper-transport-error"),
                        QStringLiteral("Could not connect to Paper wallpaper service"));
        }
    });
    connect(socket, &QLocalSocket::disconnected, this, [this, requestId] {
        if (requestId == m_requestId && m_busy && m_readBuffer.isEmpty()) {
            finishError(requestId,
                        QStringLiteral("paper-transport-error"),
                        QStringLiteral("Paper wallpaper service disconnected"));
        }
    });
    m_timeout->start(Astrea::PaperProtocol::kTransportDeadlineMs);
    socket->connectToServer(m_endpoint);
}

void SettingsWallpaperController::handleConnected(const quint64 requestId,
                                                   const QJsonObject &argument)
{
    if (requestId != m_requestId || !m_socket)
        return;
    const auto line = QStringLiteral("wallpaper ") + m_pendingAction + QLatin1Char(' ')
        + QString::fromUtf8(QJsonDocument(argument).toJson(QJsonDocument::Compact))
        + QLatin1Char('\n');
    const auto payload = line.toUtf8();
    if (payload.size() > 4096 || m_socket->write(payload) != payload.size()) {
        finishError(requestId,
                    QStringLiteral("paper-request-rejected"),
                    QStringLiteral("Paper wallpaper request was rejected"));
        return;
    }
    m_timeout->start(Astrea::PaperProtocol::kClientOperationDeadlineMs);
}

void SettingsWallpaperController::handleReadyRead(const quint64 requestId)
{
    if (requestId != m_requestId || !m_socket)
        return;
    m_readBuffer.append(m_socket->readAll());
    if (m_readBuffer.size() > 1024 * 1024) {
        finishError(requestId,
                    QStringLiteral("paper-response-too-large"),
                    QStringLiteral("Paper wallpaper response was too large"));
        return;
    }
    const auto newline = m_readBuffer.indexOf('\n');
    if (newline < 0)
        return;
    const auto payload = m_readBuffer.left(newline);
    finishResponse(requestId, payload);
}

void SettingsWallpaperController::finishResponse(const quint64 requestId, const QByteArray &payload)
{
    if (requestId != m_requestId)
        return;
    if (!applyResponse(payload)) {
        const auto code = m_errorCode.isEmpty() ? QStringLiteral("paper-response-invalid")
                                                 : m_errorCode;
        const auto message = m_errorMessage.isEmpty()
            ? QStringLiteral("Paper wallpaper service returned invalid data")
            : m_errorMessage;
        finishError(requestId, code, message);
        return;
    }
    if (m_timeout)
        m_timeout->stop();
    auto *socket = m_socket.data();
    m_socket.clear();
    setBusy(false);
    if (socket) {
        socket->disconnectFromServer();
        socket->deleteLater();
    }
}

void SettingsWallpaperController::finishError(const quint64 requestId,
                                               const QString &code,
                                               const QString &message)
{
    if (requestId != m_requestId)
        return;
    if (m_timeout)
        m_timeout->stop();
    m_errorCode = code;
    m_errorMessage = message;
    emit errorChanged();
    auto *socket = m_socket.data();
    m_socket.clear();
    setBusy(false);
    if (socket) {
        socket->abort();
        socket->deleteLater();
    }
}

bool SettingsWallpaperController::applyResponse(const QByteArray &payload)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_errorCode = QStringLiteral("paper-response-invalid");
        m_errorMessage = QStringLiteral("Paper wallpaper service returned invalid data");
        return false;
    }
    const auto response = document.object();
    if (!response.value(QStringLiteral("completed")).toBool()) {
        m_errorCode = QStringLiteral("paper-response-incomplete");
        m_errorMessage = QStringLiteral("Paper wallpaper service did not complete the request");
        return false;
    }
    if (!response.value(QStringLiteral("ok")).toBool()
        && !response.value(QStringLiteral("snapshot")).isObject()) {
        const auto oldCode = m_errorCode;
        const auto oldMessage = m_errorMessage;
        m_errorCode = response.value(QStringLiteral("errorCode"))
                          .toString(QStringLiteral("paper-request-failed"));
        m_errorMessage = response.value(QStringLiteral("message"))
                             .toString(QStringLiteral("Paper wallpaper request failed"));
        if (oldCode != m_errorCode || oldMessage != m_errorMessage)
            emit errorChanged();
        return true;
    }
    const auto state = response.value(QStringLiteral("snapshot")).toObject();
    if (state.isEmpty()) {
        m_errorCode = QStringLiteral("paper-response-invalid");
        m_errorMessage = QStringLiteral("Paper wallpaper service returned no snapshot");
        return false;
    }
    const auto effective = state.value(QStringLiteral("effective")).toObject();
    const auto configured = state.value(QStringLiteral("configured"));
    const auto configuredObject = configured.isObject() ? configured.toObject() : QJsonObject{};
    const auto configuredFit = configuredObject.value(QStringLiteral("fit")).toString();
    const auto effectiveFit = effective.value(QStringLiteral("fit")).toString();
    if (!isSupportedFit(effectiveFit)
        || (configured.isObject() && !isSupportedFit(configuredFit))) {
        m_errorCode = QStringLiteral("invalid-fit");
        m_errorMessage = QStringLiteral("Paper wallpaper snapshot contains an unsupported fit");
        return false;
    }
    m_configuredId = configured.isObject()
        ? configuredObject.value(QStringLiteral("logicalId")).toString()
        : QString();
    m_configuredSource = configured.isObject()
        ? configuredObject.value(QStringLiteral("source")).toString()
        : QString();
    m_configuredFit = configured.isObject() ? configuredFit : QString();
    m_effectiveId = effective.value(QStringLiteral("logicalId")).toString();
    m_effectiveSource = effective.value(QStringLiteral("resolvedSource"))
                            .toString(effective.value(QStringLiteral("source")).toString());
    m_effectiveFit = effectiveFit;
    m_currentDisplayName = effective.value(QStringLiteral("displayName")).toString().trimmed();
    m_stateName = state.value(QStringLiteral("state")).toString();
    m_fallbackReason = state.value(QStringLiteral("fallback")).toString();
    m_generation = state.value(QStringLiteral("generation")).toVariant().toULongLong();

    if (response.value(QStringLiteral("wallpapers")).isArray()) {
        QVariantList wallpapers;
        for (const auto &entry : response.value(QStringLiteral("wallpapers")).toArray()) {
            if (entry.isObject()) {
                wallpapers.append(entry.toObject().toVariantMap());
            }
        }
        m_wallpapers = std::move(wallpapers);

        m_dynamicWallpapers.clear();
        m_userWallpapers.clear();
        m_landscapeWallpapers.clear();
        for (const auto &wallpaper : m_wallpapers) {
            const auto descriptor = wallpaper.toMap();
            const auto origin = descriptor.value(QStringLiteral("origin")).toString().trimmed().toLower();
            const auto kind = descriptor.value(QStringLiteral("kind")).toString().trimmed().toLower();
            if (origin == QStringLiteral("user")) {
                m_userWallpapers.append(descriptor);
            } else if (kind == QStringLiteral("dynamic")) {
                m_dynamicWallpapers.append(descriptor);
            } else if (origin == QStringLiteral("system") && kind == QStringLiteral("image")) {
                m_landscapeWallpapers.append(descriptor);
            }
        }
    }

    if (m_currentDisplayName.isEmpty() && !m_effectiveId.isEmpty()) {
        for (const auto &wallpaper : m_wallpapers) {
            const auto descriptor = wallpaper.toMap();
            if (descriptor.value(QStringLiteral("logicalId")).toString() == m_effectiveId) {
                m_currentDisplayName = descriptor.value(QStringLiteral("displayName")).toString().trimmed();
                break;
            }
        }
    }

    const auto oldCode = m_errorCode;
    const auto oldMessage = m_errorMessage;
    if (response.value(QStringLiteral("ok")).toBool()) {
        m_errorCode = state.value(QStringLiteral("errorCode")).toString();
        m_errorMessage = state.value(QStringLiteral("lastError")).toString();
    } else {
        m_errorCode = response.value(QStringLiteral("errorCode"))
                          .toString(QStringLiteral("paper-request-failed"));
        m_errorMessage = response.value(QStringLiteral("message"))
                             .toString(QStringLiteral("Paper wallpaper request failed"));
    }
    emit snapshotChanged();
    if (oldCode != m_errorCode || oldMessage != m_errorMessage)
        emit errorChanged();
    return true;
}

void SettingsWallpaperController::refresh()
{
    startRequest(QStringLiteral("get"), {});
}

void SettingsWallpaperController::refreshLibrary()
{
    startRequest(QStringLiteral("list"), {});
}

void SettingsWallpaperController::setSource(const QString &source, const QString &fit)
{
    if (!isSupportedFit(fit)) {
        m_errorCode = QStringLiteral("invalid-fit");
        m_errorMessage = QStringLiteral("Wallpaper fit is not supported");
        emit errorChanged();
        setBusy(false);
        return;
    }
    startRequest(QStringLiteral("set"),
                 QJsonObject{{QStringLiteral("source"), source},
                             {QStringLiteral("fit"), fit},
                             {QStringLiteral("kind"), QStringLiteral("image")},
                             {QStringLiteral("scope"), QStringLiteral("global")} });
}

void SettingsWallpaperController::selectWallpaper(const QString &logicalId, const QString &fit)
{
    if (logicalId.trimmed().isEmpty()) {
        m_errorCode = QStringLiteral("invalid-descriptor");
        m_errorMessage = QStringLiteral("Wallpaper ID is required");
        emit errorChanged();
        setBusy(false);
        return;
    }
    if (!isSupportedFit(fit)) {
        m_errorCode = QStringLiteral("invalid-fit");
        m_errorMessage = QStringLiteral("Wallpaper fit is not supported");
        emit errorChanged();
        setBusy(false);
        return;
    }
    startRequest(QStringLiteral("set"),
                 QJsonObject{{QStringLiteral("id"), logicalId},
                             {QStringLiteral("fit"), fit},
                             {QStringLiteral("kind"), QStringLiteral("image")},
                             {QStringLiteral("scope"), QStringLiteral("global")} });
}

void SettingsWallpaperController::importWallpaper(const QString &path, const QString &fit)
{
    importAndSelectWallpaper(path, {}, fit);
}

void SettingsWallpaperController::importAndSelectWallpaper(const QString &path,
                                                           const QString &displayName,
                                                           const QString &fit)
{
    if (path.trimmed().isEmpty()) {
        m_errorCode = QStringLiteral("invalid-descriptor");
        m_errorMessage = QStringLiteral("Wallpaper import path is required");
        emit errorChanged();
        setBusy(false);
        return;
    }
    if (!isSupportedFit(fit)) {
        m_errorCode = QStringLiteral("invalid-fit");
        m_errorMessage = QStringLiteral("Wallpaper fit is not supported");
        emit errorChanged();
        setBusy(false);
        return;
    }
    startRequest(QStringLiteral("import"),
                 QJsonObject{{QStringLiteral("path"), path},
                             {QStringLiteral("fit"), fit},
                             {QStringLiteral("displayName"), displayName}});
}

void SettingsWallpaperController::addUserWallpaper(const QString &path,
                                                   const QString &displayName)
{
    if (path.trimmed().isEmpty()) {
        m_errorCode = QStringLiteral("invalid-descriptor");
        m_errorMessage = QStringLiteral("Wallpaper add path is required");
        emit errorChanged();
        setBusy(false);
        return;
    }
    startRequest(QStringLiteral("add"),
                 QJsonObject{{QStringLiteral("path"), path},
                             {QStringLiteral("displayName"), displayName}});
}

void SettingsWallpaperController::reset()
{
    startRequest(QStringLiteral("reset"), {});
}

void SettingsWallpaperController::loadDefault()
{
    startRequest(QStringLiteral("default"), {});
}

void SettingsWallpaperController::setBusy(const bool busy, const QString &action)
{
    const auto changed = m_busy != busy || m_pendingAction != action;
    m_busy = busy;
    m_pendingAction = busy ? action : QString();
    if (changed)
        emit busyChanged();
}
