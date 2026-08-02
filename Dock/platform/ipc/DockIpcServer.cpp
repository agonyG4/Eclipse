#include "platform/ipc/DockIpcServer.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>

DockIpcServer::DockIpcServer(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<IpcCommand>();
}

bool DockIpcServer::listen(const QString &name, QString *errorOut)
{
    if (m_server)
        return false;
    m_serverName = name;
    m_server = new QLocalServer(this);
    connect(m_server, &QLocalServer::newConnection, this, &DockIpcServer::onNewConnection);
    if (m_server->listen(name))
        return true;

    const QString firstError = m_server->errorString();
    QLocalSocket probe;
    probe.connectToServer(name, QIODevice::ReadWrite);
    if (probe.waitForConnected(100)) {
        if (errorOut)
            *errorOut = firstError;
        m_server->deleteLater();
        m_server = nullptr;
        return false;
    }

    m_server->close();
    QLocalServer::removeServer(name);
    if (!m_server->listen(name)) {
        if (errorOut)
            *errorOut = m_server->errorString();
        m_server->deleteLater();
        m_server = nullptr;
        return false;
    }
    return true;
}

void DockIpcServer::stopListening()
{
    if (!m_server)
        return;
    m_server->close();
    m_server->deleteLater();
    m_server = nullptr;
    m_buffers.clear();
    if (!m_serverName.isEmpty())
        QLocalServer::removeServer(m_serverName);
}

void DockIpcServer::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QLocalSocket *socket = m_server->nextPendingConnection();
        m_buffers.insert(socket, {});
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            if (!m_buffers.contains(socket))
                return;
            QByteArray &buffer = m_buffers[socket];
            buffer.append(socket->readAll());
            if (buffer.size() > kMaxCommandSize) {
                socket->disconnectFromServer();
                return;
            }

            while (true) {
                const qsizetype newline = buffer.indexOf('\n');
                if (newline < 0)
                    break;
                const QByteArray line = buffer.left(newline).trimmed();
                buffer.remove(0, newline + 1);
                if (line.isEmpty())
                    continue;
                const IpcCommand command = parseCommand(QString::fromUtf8(line));
                if (command.type == Command::Unknown)
                    continue;
                if (command.type == Command::Status && m_replyCallback) {
                    socket->write(m_replyCallback().toUtf8() + '\n');
                    socket->flush();
                }
                emit commandReceived(command);
            }
        });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            m_buffers.remove(socket);
            socket->deleteLater();
        });
    }
}

DockIpcServer::IpcCommand DockIpcServer::parseCommand(const QString &text)
{
    const QString command = text.trimmed();
    IpcCommand result;
    if (command == QStringLiteral("status") || command == QStringLiteral("--status"))
        result.type = Command::Status;
    else if (command == QStringLiteral("reload") || command == QStringLiteral("--reload"))
        result.type = Command::Reload;
    else if (command == QStringLiteral("show") || command == QStringLiteral("--show"))
        result.type = Command::Show;
    else if (command == QStringLiteral("hide") || command == QStringLiteral("--hide"))
        result.type = Command::Hide;
    else if (command == QStringLiteral("quit") || command == QStringLiteral("--quit"))
        result.type = Command::Quit;
    return result;
}

QString DockIpcServer::serializeCommand(const IpcCommand &command)
{
    switch (command.type) {
    case Command::Status: return QStringLiteral("status");
    case Command::Reload: return QStringLiteral("reload");
    case Command::Show: return QStringLiteral("show");
    case Command::Hide: return QStringLiteral("hide");
    case Command::Quit: return QStringLiteral("quit");
    case Command::Unknown: return {};
    }
    return {};
}

bool DockIpcServer::sendCommand(const QString &serverName, const IpcCommand &command)
{
    QLocalSocket socket;
    socket.connectToServer(serverName, QIODevice::ReadWrite);
    if (!socket.waitForConnected(150))
        return false;
    const QString text = serializeCommand(command);
    if (text.isEmpty())
        return false;
    socket.write(text.toUtf8() + '\n');
    if (!socket.waitForBytesWritten(200))
        return false;
    socket.disconnectFromServer();
    return true;
}

QByteArray DockIpcServer::requestReply(const QString &serverName, const IpcCommand &command,
                                       int timeoutMs)
{
    QLocalSocket socket;
    socket.connectToServer(serverName, QIODevice::ReadWrite);
    if (!socket.waitForConnected(150))
        return {};
    const QString text = serializeCommand(command);
    if (text.isEmpty())
        return {};
    socket.write(text.toUtf8() + '\n');
    if (!socket.waitForBytesWritten(200))
        return {};

    QElapsedTimer timer;
    timer.start();
    const int boundedTimeout = qBound(1, timeoutMs, 2000);
    while (socket.bytesAvailable() == 0 && timer.elapsed() < boundedTimeout) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        socket.waitForReadyRead(10);
    }
    const QByteArray response = socket.readAll().trimmed();
    socket.disconnectFromServer();
    return response;
}
