#include "platform/ipc/SpotlightIpcServer.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>

static QString serializeCommand(const SpotlightIpcServer::IpcCommand &cmd) {
    switch (cmd.type) {
    case SpotlightIpcServer::Command::Show: return QStringLiteral("show");
    case SpotlightIpcServer::Command::Hide: return QStringLiteral("hide");
    case SpotlightIpcServer::Command::Toggle: return QStringLiteral("toggle");
    case SpotlightIpcServer::Command::Activate: return QStringLiteral("activate");
    case SpotlightIpcServer::Command::ReloadIndex: return QStringLiteral("reload-index");
    case SpotlightIpcServer::Command::Status: return QStringLiteral("status");
    case SpotlightIpcServer::Command::Query: return QStringLiteral("query ") + cmd.text;
    case SpotlightIpcServer::Command::Unknown: return {};
    }
    return {};
}

SpotlightIpcServer::SpotlightIpcServer(QObject *parent)
    : QObject(parent) {}

bool SpotlightIpcServer::listen(const QString &name, QString *errorOut) {
    m_serverName = name;
    m_server = new QLocalServer(this);
    connect(m_server, &QLocalServer::newConnection, this, &SpotlightIpcServer::onNewConnection);
    if (!m_server->listen(name)) {
        if (errorOut) *errorOut = m_server->errorString();
        return false;
    }
    return true;
}

void SpotlightIpcServer::stopListening() {
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_buffers.clear();
}

void SpotlightIpcServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QLocalSocket *socket = m_server->nextPendingConnection();
        m_buffers[socket] = QByteArray();

        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            QByteArray &buf = m_buffers[socket];
            buf.append(socket->readAll());

            if (buf.size() > kMaxCommandSize) {
                buf.clear();
                return;
            }

            while (true) {
                int nl = buf.indexOf('\n');
                if (nl < 0) break;
                QByteArray line = buf.left(nl).trimmed();
                buf.remove(0, nl + 1);
                if (!line.isEmpty()) {
                    IpcCommand cmd = parseCommand(QString::fromUtf8(line));
                    if (cmd.type == Command::Status && m_replyCb) {
                        QString reply = m_replyCb(cmd);
                        socket->write(reply.toUtf8() + "\n");
                    }
                    if (cmd.type != Command::Unknown)
                        emit commandReceived(cmd);
                }
            }
        });

        connect(socket, &QLocalSocket::disconnected, socket, [this, socket]() {
            m_buffers.remove(socket);
            socket->deleteLater();
        });
    }
}

SpotlightIpcServer::IpcCommand SpotlightIpcServer::parseCommand(const QString &text) {
    IpcCommand cmd;
    QString trimmed = text.trimmed();

    if (trimmed == QStringLiteral("show")) {
        cmd.type = Command::Show;
    } else if (trimmed == QStringLiteral("hide")) {
        cmd.type = Command::Hide;
    } else if (trimmed == QStringLiteral("toggle")) {
        cmd.type = Command::Toggle;
    } else if (trimmed == QStringLiteral("activate")) {
        cmd.type = Command::Activate;
    } else if (trimmed == QStringLiteral("reload-index")) {
        cmd.type = Command::ReloadIndex;
    } else if (trimmed == QStringLiteral("status")) {
        cmd.type = Command::Status;
    } else if (trimmed.startsWith(QStringLiteral("query "))) {
        cmd.type = Command::Query;
        cmd.text = trimmed.mid(6).trimmed();
    } else if (trimmed.startsWith(QStringLiteral("--show"))) {
        cmd.type = Command::Show;
    } else if (trimmed.startsWith(QStringLiteral("--hide"))) {
        cmd.type = Command::Hide;
    } else if (trimmed.startsWith(QStringLiteral("--toggle"))) {
        cmd.type = Command::Toggle;
    } else if (trimmed.startsWith(QStringLiteral("--activate"))) {
        cmd.type = Command::Activate;
    } else if (trimmed.startsWith(QStringLiteral("--reload-index"))) {
        cmd.type = Command::ReloadIndex;
    } else if (trimmed.startsWith(QStringLiteral("--status"))) {
        cmd.type = Command::Status;
    } else if (trimmed.startsWith(QStringLiteral("--query "))) {
        cmd.type = Command::Query;
        cmd.text = trimmed.mid(8).trimmed();
    } else if (trimmed.startsWith(QStringLiteral("--daemon"))) {
        cmd.type = Command::Hide;
    } else {
        cmd.type = Command::Unknown;
    }
    return cmd;
}

bool SpotlightIpcServer::sendCommand(const QString &serverName, const IpcCommand &cmd) {
    QLocalSocket socket;
    socket.connectToServer(serverName, QIODevice::ReadWrite);
    if (!socket.waitForConnected(150))
        return false;

    const QString cmdText = serializeCommand(cmd);
    if (cmdText.isEmpty())
        return false;

    QByteArray data = cmdText.toUtf8() + "\n";
    socket.write(data);
    if (!socket.waitForBytesWritten(200))
        return false;

    socket.disconnectFromServer();
    return true;
}

QByteArray SpotlightIpcServer::requestReply(const QString &serverName, const IpcCommand &cmd, int timeoutMs) {
    QLocalSocket socket;
    socket.connectToServer(serverName, QIODevice::ReadWrite);
    if (!socket.waitForConnected(150))
        return {};

    const QString cmdText = serializeCommand(cmd);
    if (cmdText.isEmpty())
        return {};

    QByteArray data = cmdText.toUtf8() + "\n";
    socket.write(data);
    if (!socket.waitForBytesWritten(200))
        return {};

    int pollMs = timeoutMs;
    while (pollMs > 0 && socket.bytesAvailable() == 0) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        if (socket.bytesAvailable() == 0)
            socket.waitForReadyRead(25);
        if (socket.bytesAvailable() == 0)
            pollMs -= 25;
    }

    QByteArray reply = socket.readAll();
    socket.disconnectFromServer();
    return reply.trimmed();
}
