#include "platform/ipc/AltTabIpcServer.hpp"
#include <QCoreApplication>
#include <QPointer>

static QString serializeCommand(const AltTabIpcServer::IpcCommand &cmd) {
    switch (cmd.type) {
    case AltTabIpcServer::Command::Next: return QStringLiteral("next");
    case AltTabIpcServer::Command::Previous: return QStringLiteral("previous");
    case AltTabIpcServer::Command::Commit: return QStringLiteral("commit");
    case AltTabIpcServer::Command::Cancel: return QStringLiteral("cancel");
    case AltTabIpcServer::Command::Show: return QStringLiteral("show");
    case AltTabIpcServer::Command::Hide: return QStringLiteral("hide");
    case AltTabIpcServer::Command::ReloadWindows: return QStringLiteral("reload-windows");
    case AltTabIpcServer::Command::Status: return QStringLiteral("status");
    case AltTabIpcServer::Command::Unknown: return {};
    }
    return {};
}

AltTabIpcServer::AltTabIpcServer(QObject *parent)
    : QObject(parent) {}

bool AltTabIpcServer::listen(const QString &name, QString *errorOut) {
    if (m_server)
        stopListening();

    m_serverName = name;
    m_server = new QLocalServer(this);
    connect(m_server, &QLocalServer::newConnection, this, &AltTabIpcServer::onNewConnection);
    if (m_server->listen(name))
        return true;

    const QString firstError = m_server->errorString();
    QLocalSocket probe;
    probe.connectToServer(name, QIODevice::ReadWrite);
    if (probe.waitForConnected(100)) {
        if (errorOut)
            *errorOut = firstError;
        m_server->close();
        delete m_server;
        m_server = nullptr;
        return false;
    }

    m_server->close();
    delete m_server;
    m_server = nullptr;
    QLocalServer::removeServer(name);

    m_server = new QLocalServer(this);
    connect(m_server, &QLocalServer::newConnection, this, &AltTabIpcServer::onNewConnection);
    if (m_server->listen(name))
        return true;

    if (errorOut)
        *errorOut = m_server->errorString().isEmpty() ? firstError : m_server->errorString();
    m_server->close();
    delete m_server;
    m_server = nullptr;
    return false;
}

void AltTabIpcServer::stopListening() {
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_buffers.clear();
}

void AltTabIpcServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QLocalSocket *socket = m_server->nextPendingConnection();
        QPointer<QLocalSocket> guard(socket);
        m_buffers[socket] = QByteArray();

        connect(socket, &QLocalSocket::readyRead, this, [this, guard]() {
            if (!guard)
                return;
            QLocalSocket *socket = guard.data();
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
                        if (guard)
                            socket->write(reply.toUtf8() + "\n");
                    }
                    if (cmd.type != Command::Unknown)
                        emit commandReceived(cmd);
                }
            }
        });

        connect(socket, &QLocalSocket::disconnected, this, [this, guard]() {
            if (!guard)
                return;
            QLocalSocket *socket = guard.data();
            m_buffers.remove(socket);
            socket->deleteLater();
        });
    }
}

AltTabIpcServer::IpcCommand AltTabIpcServer::parseCommand(const QString &text) {
    IpcCommand cmd;
    const QString trimmed = text.trimmed();

    if (trimmed == QStringLiteral("next")) {
        cmd.type = Command::Next;
    } else if (trimmed == QStringLiteral("previous")) {
        cmd.type = Command::Previous;
    } else if (trimmed == QStringLiteral("commit")) {
        cmd.type = Command::Commit;
    } else if (trimmed == QStringLiteral("cancel")) {
        cmd.type = Command::Cancel;
    } else if (trimmed == QStringLiteral("show")) {
        cmd.type = Command::Show;
    } else if (trimmed == QStringLiteral("hide")) {
        cmd.type = Command::Hide;
    } else if (trimmed == QStringLiteral("reload-windows")) {
        cmd.type = Command::ReloadWindows;
    } else if (trimmed == QStringLiteral("status")) {
        cmd.type = Command::Status;
    } else if (trimmed == QStringLiteral("--next")) {
        cmd.type = Command::Next;
    } else if (trimmed == QStringLiteral("--previous")) {
        cmd.type = Command::Previous;
    } else if (trimmed == QStringLiteral("--commit")) {
        cmd.type = Command::Commit;
    } else if (trimmed == QStringLiteral("--cancel")) {
        cmd.type = Command::Cancel;
    } else if (trimmed == QStringLiteral("--show")) {
        cmd.type = Command::Show;
    } else if (trimmed == QStringLiteral("--hide")) {
        cmd.type = Command::Hide;
    } else if (trimmed == QStringLiteral("--reload-windows")) {
        cmd.type = Command::ReloadWindows;
    } else if (trimmed == QStringLiteral("--status")) {
        cmd.type = Command::Status;
    } else if (trimmed == QStringLiteral("--daemon")) {
        cmd.type = Command::Hide;
    } else {
        cmd.type = Command::Unknown;
    }
    return cmd;
}

bool AltTabIpcServer::sendCommand(const QString &serverName, const IpcCommand &cmd) {
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

QByteArray AltTabIpcServer::requestReply(const QString &serverName, const IpcCommand &cmd, int timeoutMs) {
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
