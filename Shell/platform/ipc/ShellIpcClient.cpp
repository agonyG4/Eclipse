#include "platform/ipc/ShellIpcClient.hpp"

#include <QEventLoop>
#include <QLocalSocket>
#include <QTimer>

namespace {

QByteArray payloadFor(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > 4096)
        return {};
    return trimmed.toUtf8() + '\n';
}

} // namespace

bool ShellIpcClient::send(const QString &serverName, const QString &line, int timeoutMs)
{
    const QByteArray payload = payloadFor(line);
    if (payload.isEmpty())
        return false;

    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (!socket.waitForConnected(timeoutMs))
        return false;
    if (socket.write(payload) != payload.size() || !socket.waitForBytesWritten(timeoutMs))
        return false;
    socket.disconnectFromServer();
    return true;
}

QByteArray ShellIpcClient::requestReply(const QString &serverName, const QString &line,
                                        int timeoutMs)
{
    const QByteArray payload = payloadFor(line);
    if (payload.isEmpty())
        return {};

    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (!socket.waitForConnected(timeoutMs))
        return {};
    if (socket.write(payload) != payload.size() || !socket.waitForBytesWritten(timeoutMs))
        return {};

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&socket, &QLocalSocket::readyRead, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QLocalSocket::disconnected, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    const QByteArray reply = socket.readAll();
    socket.disconnectFromServer();
    return reply.trimmed();
}
