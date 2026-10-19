#include "platform/DownloadServer.hpp"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLocalSocket>
#include <cerrno>
#include <sys/stat.h>

namespace Astrea::Downloads {

DownloadServer::DownloadServer(QObject *parent) : QObject(parent)
{
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    m_server.setMaxPendingConnections(MaxClients);
    connect(&m_server, &QLocalServer::newConnection, this, &DownloadServer::acceptConnections);
}

DownloadServer::~DownloadServer()
{
    m_server.close();
    for (auto *socket : m_buffers.keys()) {
        socket->disconnect(this);
        delete socket;
    }
}

bool DownloadServer::listen(const QString &path, QString *errorOut)
{
    const auto fail = [errorOut](const QString &message) {
        if (errorOut)
            *errorOut = message;
        return false;
    };
    if (m_server.isListening())
        return fail("Service is already listening");
    if (path.isEmpty() || !QFileInfo(path).isAbsolute() || path.contains(QChar::Null))
        return fail("Socket path must be absolute and nonempty");
    auto lock = std::make_unique<QLockFile>(path + ".lock");
    // A running service remains authoritative regardless of lock-file age.
    lock->setStaleLockTime(0);
    if (!lock->tryLock())
        return fail("Socket is locked by another service or its directory is not writable");
    QLocalSocket probe;
    probe.connectToServer(path);
    if (probe.waitForConnected(100))
        return fail("Socket is already in use");
    struct stat info {};
    const int status = ::lstat(QFile::encodeName(path).constData(), &info);
    if (status == 0 && !S_ISSOCK(info.st_mode))
        return fail("Refusing to replace an existing file or symbolic link");
    if (status != 0 && errno != ENOENT)
        return fail("Cannot inspect the existing socket path");
    // Only remove a stale socket after acquiring the exclusive companion lock.
    QLocalServer::removeServer(path);
    if (!m_server.listen(path))
        return fail(m_server.errorString());
    m_lock = std::move(lock);
    return true;
}

void DownloadServer::acceptConnections()
{
    while (m_server.hasPendingConnections()) {
        auto *socket = m_server.nextPendingConnection();
        if (m_buffers.size() >= MaxClients) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        socket->setReadBufferSize(MaxFrameBytes + 1);
        m_buffers.insert(socket, {});
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { consume(socket); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            m_watchers.remove(socket);
            m_buffers.remove(socket);
            socket->deleteLater();
        });
        if (socket->bytesAvailable())
            consume(socket);
    }
}

void DownloadServer::consume(QLocalSocket *socket)
{
    while (m_buffers.contains(socket) && socket->bytesAvailable()) {
        m_buffers[socket].append(socket->read(MaxFrameBytes + 1 - m_buffers[socket].size()));
        while (m_buffers.contains(socket)) {
            const auto newline = m_buffers[socket].indexOf('\n');
            if (newline < 0)
                break;
            const auto line = m_buffers[socket].left(newline);
            m_buffers[socket].remove(0, newline + 1);
            if (line.size() > MaxFrameBytes) {
                socket->abort();
                return;
            }
            dispatch(socket, line);
            // A slow watcher may be aborted while dispatch broadcasts an event.
            if (!m_buffers.contains(socket))
                return;
        }
        if (m_buffers[socket].size() > MaxFrameBytes) {
            socket->abort();
            return;
        }
    }
}

void DownloadServer::reply(QLocalSocket *socket, const QJsonObject &message)
{
    const auto frame = QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n';
    if (socket->bytesToWrite() + frame.size() > MaxPendingBytes) {
        socket->abort();
        return;
    }
    socket->write(frame);
}

void DownloadServer::broadcast(const QJsonObject &event)
{
    const auto watchers = m_watchers;
    for (auto *socket : watchers)
        reply(socket, event);
}

void DownloadServer::dispatch(QLocalSocket *socket, const QByteArray &line)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        reply(socket, failure("invalid_json", "Expected a JSON object"));
        return;
    }
    const auto request = document.object();
    if (!request.value("version").isDouble() || request.value("version").toDouble() != 1) {
        reply(socket, failure("unsupported_version", "Expected protocol version 1"));
        return;
    }
    const auto op = request.value("op").toString();
    if (op == "list" || op == "watch") {
        if (op == "watch")
            m_watchers.insert(socket);
        QJsonObject response{{"version", 1}, {"ok", true}, {"downloads", m_store.list()}};
        if (op == "watch")
            response["event"] = "snapshot";
        reply(socket, response);
    } else if (op == "update") {
        const auto result = m_store.update(request.value("download").toObject());
        reply(socket, result);
        if (result.value("ok").toBool())
            broadcast({{"version", 1}, {"ok", true}, {"event", "updated"},
                       {"download", result.value("download")}});
    } else if (op == "remove") {
        const auto source = request.value("source");
        const auto id = request.value("id");
        if (!source.isString() || !id.isString()) {
            reply(socket, failure("invalid_request", "Expected string source and id"));
            return;
        }
        auto result = m_store.remove(source.toString(), id.toString());
        reply(socket, result);
        if (result.value("ok").toBool()) {
            result["event"] = "removed";
            broadcast(result);
        }
    } else {
        reply(socket, failure("unknown_operation", "Expected update, list, watch or remove"));
    }
}
} // namespace Astrea::Downloads
