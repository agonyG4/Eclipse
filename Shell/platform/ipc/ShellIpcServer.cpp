#include "platform/ipc/ShellIpcServer.hpp"

#include <QEventLoop>
#include <QTimer>

ShellIpcServer::ShellIpcServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QLocalServer::newConnection, this, &ShellIpcServer::onNewConnection);
}

bool ShellIpcServer::listen(const QString &name, QString *errorOut)
{
    if (m_server.isListening())
        return true;
    if (!m_server.listen(name)) {
        QLocalSocket probe;
        probe.connectToServer(name);
        if (probe.waitForConnected(100)) {
            if (errorOut)
                *errorOut = QStringLiteral("IPC endpoint is already in use");
            return false;
        }
        QLocalServer::removeServer(name);
        if (!m_server.listen(name)) {
            if (errorOut)
                *errorOut = m_server.errorString();
            return false;
        }
    }
    return true;
}

void ShellIpcServer::stopListening()
{
    m_server.close();
    const auto sockets = m_buffers.keys();
    for (QLocalSocket *socket : sockets) {
        socket->disconnectFromServer();
        socket->deleteLater();
    }
    m_buffers.clear();
}

ShellIpcServer::Command ShellIpcServer::parseCommand(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > kMaxCommandSize)
        return {};

    const QStringList fields = trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (fields.isEmpty())
        return {};

    Command command;
    if (fields.size() == 1 && fields.first() == QStringLiteral("status")) {
        command.feature = QStringLiteral("shell");
        command.action = QStringLiteral("status");
        command.valid = true;
        return command;
    }
    if (fields.size() < 2)
        return {};

    command.feature = fields.at(0).toLower();
    command.action = fields.at(1).toLower();
    const int firstArgument = trimmed.indexOf(QLatin1Char(' '));
    if (firstArgument >= 0) {
        const int secondArgument = trimmed.indexOf(QLatin1Char(' '), firstArgument + 1);
        if (secondArgument >= 0)
            command.argument = trimmed.mid(secondArgument + 1).trimmed();
    }
    command.valid = command.feature != QStringLiteral("shell")
        ? !command.feature.isEmpty() && !command.action.isEmpty()
        : command.action == QStringLiteral("status")
            || command.action == QStringLiteral("reload")
            || command.action == QStringLiteral("quit");
    return command;
}

QString ShellIpcServer::serializeCommand(const Command &command)
{
    if (!command.valid)
        return {};
    QString line = command.feature + QLatin1Char(' ') + command.action;
    if (!command.argument.isEmpty())
        line += QLatin1Char(' ') + command.argument;
    return line;
}

bool ShellIpcServer::sendCommand(const QString &name, const Command &command)
{
    QLocalSocket socket;
    socket.connectToServer(name);
    if (!socket.waitForConnected(500))
        return false;
    const QByteArray payload = (serializeCommand(command) + QLatin1Char('\n')).toUtf8();
    if (socket.write(payload) != payload.size() || !socket.waitForBytesWritten(500))
        return false;
    socket.disconnectFromServer();
    return true;
}

QByteArray ShellIpcServer::requestReply(const QString &name, const Command &command,
                                        int timeoutMs)
{
    QLocalSocket socket;
    socket.connectToServer(name);
    if (!socket.waitForConnected(timeoutMs))
        return {};
    const QByteArray payload = (serializeCommand(command) + QLatin1Char('\n')).toUtf8();
    if (socket.write(payload) != payload.size() || !socket.waitForBytesWritten(timeoutMs))
        return {};
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&socket, &QLocalSocket::readyRead, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QLocalSocket::disconnected, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(timeoutMs);
    loop.exec();
    if (!socket.bytesAvailable())
        return {};
    return socket.readAll();
}

void ShellIpcServer::onNewConnection()
{
    while (QLocalSocket *socket = m_server.nextPendingConnection()) {
        m_buffers.insert(socket, {});
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { consume(socket); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            m_buffers.remove(socket);
            socket->deleteLater();
        });
    }
}

void ShellIpcServer::consume(QLocalSocket *socket)
{
    if (!socket || !m_buffers.contains(socket))
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
            return;
        const QByteArray line = buffer.left(newline);
        buffer.remove(0, newline + 1);
        const Command command = parseCommand(QString::fromUtf8(line));
        if (!command.valid)
            continue;
        emit commandReceived(command);
        if (m_replyCallback) {
            const QString reply = m_replyCallback(command);
            if (!reply.isEmpty()) {
                socket->write(reply.toUtf8());
                socket->write("\n");
                socket->flush();
            }
        }
    }
}
