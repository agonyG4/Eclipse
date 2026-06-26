#include "platform/hyprland/HyprlandSocket.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QPointer>
#include <QTimer>
#include <unistd.h>

namespace {

HyprlandCommandResult transportSuccess(const QByteArray &reply)
{
    HyprlandCommandResult result;
    result.success = true;
    result.rawReply = reply;
    return result;
}

HyprlandCommandResult transportError(const QString &code, const QString &message, const QByteArray &reply = {})
{
    HyprlandCommandResult result;
    result.success = false;
    result.errorCode = code;
    result.errorMessage = message;
    result.rawReply = reply;
    return result;
}

} // namespace

// --- HyprlandCommandRequest ---

void HyprlandCommandRequest::start(const QString &socketPath, const QByteArray &request, Options options)
{
    if (m_state != State::Idle) {
        complete(transportError(QStringLiteral("already_started"), QStringLiteral("request already started")));
        return;
    }

    m_request = request;
    m_maxReplyBytes = options.maxReplyBytes > 0 ? options.maxReplyBytes : kMaxCommandReplyBytes;
    m_state = State::Connecting;
    m_completed = false;
    m_bytesWritten = 0;
    m_latency.start();

    m_socket = new QLocalSocket(this);
    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(options.timeoutMs > 0 ? options.timeoutMs : 750);

    connect(m_socket, &QLocalSocket::connected, this, &HyprlandCommandRequest::onConnected);
    connect(m_socket, &QLocalSocket::bytesWritten, this, &HyprlandCommandRequest::onBytesWritten);
    connect(m_socket, &QLocalSocket::readyRead, this, &HyprlandCommandRequest::onReadyRead);
    connect(m_socket, &QLocalSocket::disconnected, this, &HyprlandCommandRequest::onDisconnected);
    connect(m_timeout, &QTimer::timeout, this, &HyprlandCommandRequest::onTimeout);

    m_timeout->start();
    m_socket->connectToServer(socketPath, QIODevice::ReadWrite);
}

void HyprlandCommandRequest::complete(HyprlandCommandResult result)
{
    if (m_completed)
        return;
    m_completed = true;
    m_state = State::Finished;

    if (m_timeout) {
        m_timeout->stop();
        m_timeout->deleteLater();
        m_timeout = nullptr;
    }

    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    emit finished(result);
    deleteLater();
}

void HyprlandCommandRequest::onConnected()
{
    if (m_completed)
        return;
    m_state = State::Writing;
    m_socket->write(m_request);
}

void HyprlandCommandRequest::onBytesWritten(qint64 bytes)
{
    if (m_completed)
        return;
    m_bytesWritten += bytes;
    if (m_bytesWritten < m_request.size())
        return;

    m_state = State::Reading;
}

void HyprlandCommandRequest::onReadyRead()
{
    if (m_completed)
        return;
    if (m_state != State::Reading && m_state != State::Writing)
        return;

    m_reply.append(m_socket->readAll());
    if (m_reply.size() > m_maxReplyBytes) {
        complete(transportError(QStringLiteral("oversized_response"),
            QStringLiteral("command reply exceeds %1 bytes").arg(m_maxReplyBytes),
            m_reply));
    }
}

void HyprlandCommandRequest::onDisconnected()
{
    if (m_completed)
        return;

    // Final drain: disconnect may arrive before readyRead on some schedulers
    if (m_socket && m_socket->bytesAvailable() > 0) {
        m_reply.append(m_socket->readAll());
        if (m_reply.size() > m_maxReplyBytes) {
            complete(transportError(QStringLiteral("oversized_response"),
                QStringLiteral("command reply exceeds %1 bytes").arg(m_maxReplyBytes),
                m_reply));
            return;
        }
    }

    if (m_reply.isEmpty()) {
        complete(transportError(QStringLiteral("empty_response"),
            QStringLiteral("server closed connection without sending data")));
    } else {
        complete(transportSuccess(m_reply));
    }
}

void HyprlandCommandRequest::onTimeout()
{
    if (m_completed)
        return;
    complete(transportError(QStringLiteral("timeout"),
        QStringLiteral("command timed out after %1 ms").arg(m_timeout ? m_timeout->interval() : 750),
        m_reply));
}

// --- HyprlandSocket ---

HyprlandSocket::HyprlandSocket(QObject *parent)
    : QObject(parent)
{
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &HyprlandSocket::tryReconnect);
}

HyprlandSocket::~HyprlandSocket()
{
    stop();
}

void HyprlandSocket::start()
{
    if (m_started)
        return;
    m_started = true;
    m_stopRequested = false;
    connectToEventSocket();
}

void HyprlandSocket::stop()
{
    if (!m_started && m_stopRequested)
        return;

    m_started = false;
    m_stopRequested = true;
    m_reconnectTimer->stop();
    disconnectEventSocket();
}

bool HyprlandSocket::discoverInstance()
{
    const auto env = QProcessEnvironment::systemEnvironment();
    const QString sig = env.value(QStringLiteral("HYPRLAND_INSTANCE_SIGNATURE"));

    QStringList baseDirs;
    const QString xdgRuntime = env.value(QStringLiteral("XDG_RUNTIME_DIR"));
    if (!xdgRuntime.isEmpty())
        baseDirs << xdgRuntime + QStringLiteral("/hypr");
    baseDirs << QStringLiteral("/tmp/hypr");
    baseDirs << QStringLiteral("/run/user/") + QString::number(getuid()) + QStringLiteral("/hypr");

    if (!sig.isEmpty()) {
        for (const auto &base : baseDirs) {
            const QString path = base + QLatin1Char('/') + sig;
            if (QDir(path).exists()) {
                m_runtimeDir = path;
                m_instanceSignature = sig;
                return true;
            }
        }
    }

    for (const auto &base : baseDirs) {
        const QDir dir(base);
        if (!dir.exists())
            continue;

        const QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto &sub : subDirs) {
            const QString path = base + QLatin1Char('/') + sub;
            if (QFile::exists(path + QStringLiteral("/.socket.sock"))) {
                m_runtimeDir = path;
                m_instanceSignature = sub;
                return true;
            }
        }
    }

    return false;
}

QString HyprlandSocket::socketPath(const QString &name) const
{
    return m_runtimeDir + QLatin1Char('/') + name;
}

void HyprlandSocket::disconnectEventSocket()
{
    if (!m_eventSocket)
        return;

    m_eventSocket->disconnect(this);
    m_eventSocket->abort();
    m_eventSocket->deleteLater();
    m_eventSocket = nullptr;
    m_eventBuffer.clear();
}

void HyprlandSocket::connectToEventSocket()
{
    if (!m_started || m_stopRequested)
        return;

    if (m_runtimeDir.isEmpty() && !discoverInstance()) {
        m_reconnectTimer->start(m_reconnectDelay);
        return;
    }

    disconnectEventSocket();

    const QString sockPath = socketPath(QStringLiteral(".socket2.sock"));
    m_eventSocket = new QLocalSocket(this);

    connect(m_eventSocket, &QLocalSocket::connected, this, [this]() {
        m_reconnectDelay = 1000;
        emit eventSocketConnected();
    });
    connect(m_eventSocket, &QLocalSocket::readyRead, this, &HyprlandSocket::onEventReadyRead);
    connect(m_eventSocket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError err) {
        Q_UNUSED(err);
        m_lastEventError = m_eventSocket ? m_eventSocket->errorString() : QStringLiteral("event socket error");
    });
    connect(m_eventSocket, &QLocalSocket::disconnected, this, &HyprlandSocket::onEventDisconnected);

    m_eventSocket->connectToServer(sockPath, QIODevice::ReadOnly);
}

bool HyprlandSocket::isEventConnected() const
{
    return m_eventSocket && m_eventSocket->state() == QLocalSocket::ConnectedState;
}

void HyprlandSocket::sendCommand(const QByteArray &cmd,
                                 std::function<void(const HyprlandCommandResult &)> callback,
                                 int timeoutMs)
{
    if (!callback)
        return;

    if (m_runtimeDir.isEmpty() && !discoverInstance()) {
        callback(transportError(QStringLiteral("instance_not_found"),
            QStringLiteral("hyprland instance not found")));
        return;
    }

    m_commandInFlight = true;

    auto *request = new HyprlandCommandRequest(this);
    HyprlandCommandRequest::Options opts;
    opts.timeoutMs = timeoutMs > 0 ? timeoutMs : 750;
    opts.maxReplyBytes = kMaxCommandReplyBytes;

    QObject::connect(request, &HyprlandCommandRequest::finished, this,
        [this, callback, startTime = QDateTime::currentMSecsSinceEpoch()](const HyprlandCommandResult &result) {
            m_commandInFlight = false;
            m_lastCommandLatency = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch() - startTime);
            if (result.success) {
                m_lastCommandSuccessTime = QDateTime::currentMSecsSinceEpoch();
                m_lastCommandError.clear();
            } else {
                m_lastCommandError = result.errorCode + QStringLiteral(": ") + result.errorMessage;
            }
            callback(result);
        });

    request->start(socketPath(QStringLiteral(".socket.sock")), cmd, opts);
}

void HyprlandSocket::onEventReadyRead()
{
    if (!m_eventSocket)
        return;

    m_eventBuffer.append(m_eventSocket->readAll());
    m_lastEventTime = QDateTime::currentMSecsSinceEpoch();

    if (m_eventBuffer.size() > kMaxEventBufferBytes) {
        m_lastEventError = QStringLiteral("event buffer overflow");
        m_eventSocket->abort();
        return;
    }

    // Event wire format: EVENT>>DATA\n
    while (true) {
        const int nl = m_eventBuffer.indexOf('\n');
        if (nl < 0)
            break;
        const QByteArray line = m_eventBuffer.left(nl);
        m_eventBuffer.remove(0, nl + 1);
        if (!line.isEmpty() && m_eventCb)
            m_eventCb(QString::fromUtf8(line));
    }
}

void HyprlandSocket::onEventDisconnected()
{
    emit eventSocketDisconnected();
    emit connectionLost();
    if (m_stopRequested)
        return;
    m_reconnectDelay = qMin(m_reconnectDelay * 2, 30000);
    m_reconnectTimer->start(m_reconnectDelay);
}

void HyprlandSocket::tryReconnect()
{
    if (!isEventConnected() && m_started && !m_stopRequested)
        connectToEventSocket();
}
