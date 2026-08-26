#include "WallpaperControlServer.hpp"

#include "core/WallpaperService.hpp"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QStandardPaths>
#include <QTimer>

namespace Paper {
namespace {

QByteArray errorReply(const QString &code, const QString &message)
{
    return QJsonDocument(QJsonObject{
                             {QStringLiteral("ok"), false},
                             {QStringLiteral("completed"), true},
                             {QStringLiteral("requestId"), 0},
                             {QStringLiteral("status"), QStringLiteral("rejected")},
                             {QStringLiteral("errorCode"), code},
                             {QStringLiteral("message"), message},
                         })
        .toJson(QJsonDocument::Compact);
}

bool isKnownFit(const QString &value)
{
    const auto fit = value.trimmed().toLower();
    return fit == QStringLiteral("cover") || fit == QStringLiteral("contain")
        || fit == QStringLiteral("stretch") || fit == QStringLiteral("center")
        || fit == QStringLiteral("tile");
}

} // namespace

WallpaperControlServer::WallpaperControlServer(WallpaperService *service,
                                               QString endpoint,
                                               QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_endpoint(endpoint.isEmpty() ? defaultEndpoint() : std::move(endpoint))
{
    connect(&m_server, &QLocalServer::newConnection,
            this, &WallpaperControlServer::onNewConnection);
    if (m_service) {
        connect(m_service,
                &WallpaperService::wallpaperOperationFinished,
                this,
                &WallpaperControlServer::handleOperationFinished);
    }
}

WallpaperControlServer::~WallpaperControlServer()
{
    stopListening();
}

bool WallpaperControlServer::listen(QString *errorOut)
{
    if (!m_service) {
        if (errorOut) {
            *errorOut = QStringLiteral("Paper control server requires a service");
        }
        return false;
    }
    if (m_server.isListening()) {
        return true;
    }
    if (m_endpoint.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("XDG runtime directory is unavailable");
        }
        return false;
    }

    const QFileInfo endpointInfo(m_endpoint);
    if (!QDir().mkpath(endpointInfo.absolutePath())) {
        if (errorOut) {
            *errorOut = QStringLiteral("Could not create Paper runtime directory");
        }
        return false;
    }
    if (!QFile::setPermissions(endpointInfo.absolutePath(),
                               QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                   | QFileDevice::ExeOwner)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Could not secure Paper runtime directory");
        }
        return false;
    }

    if (!m_server.listen(m_endpoint)) {
        QLocalSocket probe;
        probe.connectToServer(m_endpoint);
        if (probe.waitForConnected(100)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Paper control endpoint is already in use");
            }
            return false;
        }
        QLocalServer::removeServer(m_endpoint);
        if (!m_server.listen(m_endpoint)) {
            if (errorOut) {
                *errorOut = m_server.errorString();
            }
            return false;
        }
    }

    m_endpointOwned = true;
    QFile::setPermissions(m_endpoint, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

void WallpaperControlServer::stopListening()
{
    m_server.close();
    const auto sockets = m_clients.keys();
    for (auto *socket : sockets) {
        QObject::disconnect(socket, nullptr, this, nullptr);
        socket->disconnectFromServer();
        socket->deleteLater();
    }
    m_clients.clear();
    m_operationWaiters.clear();
    m_completedResults.clear();
    if (m_endpointOwned && !m_endpoint.isEmpty()) {
        QLocalServer::removeServer(m_endpoint);
    }
    m_endpointOwned = false;
}

QString WallpaperControlServer::defaultEndpoint()
{
    auto runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtime.isEmpty()) {
        runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    }
    if (runtime.isEmpty()) {
        return {};
    }
    return QDir(runtime).filePath(QStringLiteral("astrea-shell/wallpaper.sock"));
}

QByteArray WallpaperControlServer::requestReply(const QString &endpoint,
                                                const QString &line,
                                                const int timeoutMs)
{
    if (endpoint.isEmpty() || line.toUtf8().size() > kMaxCommandSize) {
        return {};
    }
    QLocalSocket socket;
    socket.connectToServer(endpoint);
    if (!socket.waitForConnected(Astrea::PaperProtocol::kTransportDeadlineMs)) {
        return {};
    }
    const QByteArray payload = line.toUtf8() + '\n';
    if (socket.write(payload) != payload.size()
        || !socket.waitForBytesWritten(Astrea::PaperProtocol::kTransportDeadlineMs)) {
        return {};
    }

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&socket, &QLocalSocket::readyRead, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QLocalSocket::disconnected, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(timeoutMs);
    loop.exec();
    return socket.readAll();
}

void WallpaperControlServer::onNewConnection()
{
    while (auto *socket = m_server.nextPendingConnection()) {
        if (m_clients.size() >= kMaxClients) {
            socket->disconnectFromServer();
            socket->deleteLater();
            continue;
        }
        ClientState state;
        state.idleTimer = new QTimer(socket);
        state.idleTimer->setSingleShot(true);
        connect(state.idleTimer, &QTimer::timeout, this, [this, socket] {
            expireClient(socket);
        });
        m_clients.insert(socket, state);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { consume(socket); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            if (auto it = m_clients.find(socket); it != m_clients.end()) {
                if (it->operationId) {
                    m_operationWaiters.remove(*it->operationId);
                }
                m_clients.erase(it);
            }
            socket->deleteLater();
        });
        state.idleTimer->start(kClientIdleTimeoutMs);
    }
}

void WallpaperControlServer::consume(QLocalSocket *socket)
{
    if (!socket || !m_clients.contains(socket)) {
        return;
    }
    auto &state = m_clients[socket];
    state.buffer.append(socket->readAll());
    if (state.buffer.size() > kMaxCommandSize) {
        socket->disconnectFromServer();
        return;
    }
    while (true) {
        const qsizetype newline = state.buffer.indexOf('\n');
        if (newline < 0) {
            return;
        }
        const QByteArray line = state.buffer.left(newline);
        state.buffer.remove(0, newline + 1);
        if (state.idleTimer) {
            state.idleTimer->start(kClientIdleTimeoutMs);
        }
        handleLine(socket, line);
        if (!m_clients.contains(socket)) {
            return;
        }
    }
}

void WallpaperControlServer::handleLine(QLocalSocket *socket, const QByteArray &line)
{
    const QString text = QString::fromUtf8(line).trimmed();
    if (text.size() > kMaxCommandSize) {
        socket->disconnectFromServer();
        return;
    }
    if (!text.startsWith(QStringLiteral("wallpaper "))) {
        sendReply(socket,
                  errorReply(QStringLiteral("control-protocol-error"),
                             QStringLiteral("Unsupported Paper command")));
        return;
    }

    const int actionStart = QStringLiteral("wallpaper ").size();
    const int separator = text.indexOf(QLatin1Char(' '), actionStart);
    const QString action = (separator < 0 ? text.mid(actionStart)
                                          : text.mid(actionStart, separator - actionStart))
                               .trimmed()
                               .toLower();
    const QString argument = separator < 0 ? QStringLiteral("{}") : text.mid(separator + 1).trimmed();
    if (action != QStringLiteral("get") && action != QStringLiteral("list")
        && action != QStringLiteral("import") && action != QStringLiteral("add")
        && action != QStringLiteral("set")
        && action != QStringLiteral("reset") && action != QStringLiteral("default")) {
        sendReply(socket,
                  errorReply(QStringLiteral("control-protocol-error"),
                             QStringLiteral("Unknown Paper wallpaper action")));
        return;
    }

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(argument.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        sendReply(socket,
                  errorReply(QStringLiteral("control-protocol-error"),
                             QStringLiteral("Paper command argument must be a JSON object")));
        return;
    }
    const auto object = document.object();
    if (action == QStringLiteral("list")) {
        QJsonArray wallpapers;
        for (const auto &descriptor : m_service->listWallpapers()) {
            wallpapers.append(descriptor.toJson());
        }
        sendReply(socket,
                  QJsonDocument(QJsonObject{
                                     {QStringLiteral("ok"), true},
                                     {QStringLiteral("completed"), true},
                                     {QStringLiteral("requestId"), 0},
                                     {QStringLiteral("wallpapers"), wallpapers},
                                     {QStringLiteral("snapshot"), m_service->snapshot().toJson()},
                                 })
                      .toJson(QJsonDocument::Compact));
        return;
    }

    auto queueOperation = [this, socket](const WallpaperOperationId operationId) {
        if (operationId == 0) {
            const auto snapshot = m_service->snapshot();
            sendReply(socket,
                      errorReply(snapshot.errorCode.isEmpty()
                                     ? QStringLiteral("paper-request-failed")
                                     : snapshot.errorCode,
                                 snapshot.lastError.isEmpty()
                                     ? QStringLiteral("Wallpaper operation was rejected")
                                     : snapshot.lastError));
            return;
        }
        auto &client = m_clients[socket];
        client.operationId = operationId;
        if (const auto completed = m_completedResults.take(operationId); completed.id != 0) {
            sendOperationResult(socket, completed);
        } else {
            m_operationWaiters.insert(operationId, QPointer<QLocalSocket>(socket));
            if (client.idleTimer) {
                client.idleTimer->start(kOperationTimeoutMs);
            }
        }
    };

    if (action == QStringLiteral("import")) {
        const auto path = object.value(QStringLiteral("path"));
        if (!path.isString() || path.toString().isEmpty()) {
            sendReply(socket,
                      errorReply(QStringLiteral("invalid-descriptor"),
                                 QStringLiteral("Wallpaper import requires a path")));
            return;
        }
        const auto fit = object.value(QStringLiteral("fit")).toString(QStringLiteral("cover"));
        if (!isKnownFit(fit)) {
            sendReply(socket,
                      errorReply(QStringLiteral("invalid-descriptor"),
                                 QStringLiteral("Wallpaper fit is not supported")));
            return;
        }
        const auto displayName = object.value(QStringLiteral("displayName"));
        if (!displayName.isUndefined() && !displayName.isString()) {
            sendReply(socket,
                      errorReply(QStringLiteral("invalid-descriptor"),
                                 QStringLiteral("Wallpaper display name must be text")));
            return;
        }
        auto &client = m_clients[socket];
        if (client.operationId) {
            sendReply(socket,
                      errorReply(QStringLiteral("control-protocol-error"),
                                 QStringLiteral("A wallpaper operation is already pending")));
            return;
        }
        queueOperation(m_service->importWallpaper(path.toString(),
                                                   wallpaperFitFromString(fit),
                                                   displayName.toString()));
        return;
    } else if (action == QStringLiteral("add")) {
        const auto path = object.value(QStringLiteral("path"));
        if (!path.isString() || path.toString().isEmpty()) {
            sendReply(socket,
                      errorReply(QStringLiteral("invalid-descriptor"),
                                 QStringLiteral("Wallpaper add requires a path")));
            return;
        }
        const auto displayName = object.value(QStringLiteral("displayName"));
        if (!displayName.isUndefined() && !displayName.isString()) {
            sendReply(socket,
                      errorReply(QStringLiteral("invalid-descriptor"),
                                 QStringLiteral("Wallpaper display name must be text")));
            return;
        }
        auto &client = m_clients[socket];
        if (client.operationId) {
            sendReply(socket,
                      errorReply(QStringLiteral("control-protocol-error"),
                                 QStringLiteral("A wallpaper operation is already pending")));
            return;
        }
        queueOperation(m_service->addWallpaper(path.toString(), displayName.toString()));
        return;
    } else if (action == QStringLiteral("set")) {
        const auto fit = object.value(QStringLiteral("fit")).toString(QStringLiteral("cover"));
        if (!isKnownFit(fit)) {
            sendReply(socket,
                      errorReply(QStringLiteral("invalid-descriptor"),
                                 QStringLiteral("Wallpaper fit is not supported")));
            return;
        }
        const auto id = object.value(QStringLiteral("id"));
        const auto source = object.value(QStringLiteral("source"));
        if ((!id.isString() || id.toString().isEmpty())
            && (!source.isString() || source.toString().isEmpty())) {
            sendReply(socket,
                      errorReply(QStringLiteral("invalid-descriptor"),
                                 QStringLiteral("Wallpaper set requires an ID or source")));
            return;
        }
        const auto kind = object.value(QStringLiteral("kind")).toString(QStringLiteral("image"));
        const auto scope = object.value(QStringLiteral("scope")).toString(QStringLiteral("global"));
        if (kind.compare(QStringLiteral("image"), Qt::CaseInsensitive) != 0
            || scope.compare(QStringLiteral("global"), Qt::CaseInsensitive) != 0) {
            sendReply(socket,
                      errorReply(QStringLiteral("invalid-descriptor"),
                                 QStringLiteral("Paper v1 accepts image/global only")));
            return;
        }
        auto &client = m_clients[socket];
        if (client.operationId) {
            sendReply(socket,
                      errorReply(QStringLiteral("control-protocol-error"),
                                 QStringLiteral("A wallpaper operation is already pending")));
            return;
        }
        const auto operationId = id.isString() && !id.toString().isEmpty()
            ? m_service->selectWallpaper(id.toString(), wallpaperFitFromString(fit))
            : m_service->importWallpaper(source.toString(), wallpaperFitFromString(fit));
        queueOperation(operationId);
        return;
    } else if (action == QStringLiteral("reset")) {
        const auto operationId = m_service->resetWallpaper();
        queueOperation(operationId);
        return;
    }

    sendReply(socket,
              QJsonDocument(QJsonObject{
                                 {QStringLiteral("ok"), true},
                                 {QStringLiteral("completed"), true},
                                 {QStringLiteral("requestId"), 0},
                                 {QStringLiteral("snapshot"), m_service->snapshot().toJson()},
                             })
                  .toJson(QJsonDocument::Compact));
}

void WallpaperControlServer::sendReply(QLocalSocket *socket, const QByteArray &reply)
{
    if (!socket || !m_clients.contains(socket) || reply.isEmpty()) {
        return;
    }
    socket->write(reply);
    socket->write("\n");
    socket->flush();
}

void WallpaperControlServer::sendOperationResult(QLocalSocket *socket,
                                                  const WallpaperOperationResult &result)
{
    if (!socket || !m_clients.contains(socket)) {
        return;
    }
    auto &client = m_clients[socket];
    if (client.operationId && *client.operationId == result.id) {
        client.operationId.reset();
    }
    QJsonObject response{
        {QStringLiteral("ok"), result.succeeded()},
        {QStringLiteral("completed"), true},
        {QStringLiteral("requestId"), static_cast<qint64>(result.id)},
        {QStringLiteral("status"), result.toJson().value(QStringLiteral("status"))},
        {QStringLiteral("snapshot"), result.snapshot.toJson()},
    };
    QJsonArray wallpapers;
    for (const auto &descriptor : m_service->listWallpapers()) {
        wallpapers.append(descriptor.toJson());
    }
    response.insert(QStringLiteral("wallpapers"), wallpapers);
    if (!result.succeeded()) {
        response.insert(QStringLiteral("errorCode"), result.errorCode);
        response.insert(QStringLiteral("message"), result.message);
    }
    sendReply(socket, QJsonDocument(response).toJson(QJsonDocument::Compact));
    if (client.idleTimer) {
        client.idleTimer->start(kClientIdleTimeoutMs);
    }
}

void WallpaperControlServer::handleOperationFinished(const quint64 id,
                                                      WallpaperOperationResult result)
{
    const auto waiter = m_operationWaiters.take(id);
    if (!waiter) {
        m_completedResults.insert(id, result);
        while (m_completedResults.size() > 32) {
            m_completedResults.erase(m_completedResults.begin());
        }
        return;
    }

    sendOperationResult(waiter.data(), result);
}

void WallpaperControlServer::expireClient(QLocalSocket *socket)
{
    if (!socket || !m_clients.contains(socket)) {
        return;
    }
    socket->disconnectFromServer();
}

} // namespace Paper
