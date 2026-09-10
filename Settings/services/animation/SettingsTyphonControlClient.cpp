#include "services/animation/SettingsTyphonControlClient.hpp"

#include <QFileInfo>
#include <QDir>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <unistd.h>

#include <sys/stat.h>

#include <cmath>
#include <utility>

namespace {

constexpr qsizetype kMaxRequestBytes = 64 * 1024;
constexpr qsizetype kMaxResponseBytes = 1024 * 1024;
QString protocolError(const QString &message)
{
    return QStringLiteral("Typhon control: ") + message;
}

} // namespace

SettingsTyphonControlClient::SettingsTyphonControlClient(QObject *parent, int deadlineMs)
    : QObject(parent)
    , m_deadlineMs(qMax(1, deadlineMs))
{
    connect(&m_socket, &QLocalSocket::connected, this,
            &SettingsTyphonControlClient::handleConnected);
    connect(&m_socket, &QLocalSocket::readyRead, this,
            &SettingsTyphonControlClient::handleReadyRead);
    connect(&m_socket, &QLocalSocket::errorOccurred, this,
            &SettingsTyphonControlClient::handleSocketError);
    connect(&m_socket, &QLocalSocket::disconnected, this,
            &SettingsTyphonControlClient::handleDisconnected);
    connect(&m_deadlineTimer, &QTimer::timeout, this,
            &SettingsTyphonControlClient::handleDeadline);
    m_deadlineTimer.setSingleShot(true);
}

bool SettingsTyphonControlClient::startRequest(const QString &command,
                                               const QVariantMap &arguments,
                                               QString *startError)
{
    if (startError)
        startError->clear();
    if (m_busy) {
        if (startError)
            *startError = protocolError(QStringLiteral("a request is already in flight"));
        return false;
    }
    if (command.isEmpty()) {
        if (startError)
            *startError = protocolError(QStringLiteral("command is empty"));
        return false;
    }

    QString discoveryError;
    const QString socketPath = discoverSocket(&discoveryError);
    if (socketPath.isEmpty()) {
        if (startError)
            *startError = discoveryError;
        return false;
    }

    const quint64 requestId = m_nextRequestId++;
    QJsonObject request{
        {QStringLiteral("protocol"), QStringLiteral("astrea.control")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("id"), static_cast<qint64>(requestId)},
        {QStringLiteral("command"), command},
        {QStringLiteral("args"), QJsonObject::fromVariantMap(arguments)},
    };
    QByteArray encoded = QJsonDocument(request).toJson(QJsonDocument::Compact);
    encoded.append('\n');
    if (encoded.size() > kMaxRequestBytes) {
        if (startError)
            *startError = protocolError(QStringLiteral("request exceeds 64 KiB"));
        return false;
    }

    m_socket.abort();
    m_response.clear();
    m_encodedRequest = std::move(encoded);
    m_activeRequestId = requestId;
    m_lastFailureWasServerRejection = false;
    m_busy = true;
    m_deadlineTimer.start(m_deadlineMs);
    m_socket.connectToServer(socketPath);
    return true;
}

void SettingsTyphonControlClient::handleConnected()
{
    if (!m_busy)
        return;
    if (m_socket.write(m_encodedRequest) != m_encodedRequest.size()) {
        complete(false, {}, protocolError(QStringLiteral("request write failed")));
    }
}

void SettingsTyphonControlClient::handleReadyRead()
{
    if (!m_busy)
        return;
    const qint64 remaining = kMaxResponseBytes - m_response.size();
    m_response.append(m_socket.read(remaining + 1));
    if (m_response.size() > kMaxResponseBytes) {
        complete(false, {}, protocolError(QStringLiteral("response exceeds 1 MiB")));
        return;
    }
    const qsizetype newline = m_response.indexOf('\n');
    if (newline < 0)
        return;
    if (newline == 0 || !m_response.mid(newline + 1).isEmpty()) {
        complete(false, {}, protocolError(QStringLiteral("response framing is invalid")));
        return;
    }
    QVariantMap result;
    QString error;
    if (parseResponse(m_response.left(newline), &result, &error))
        complete(true, result, {});
    else
        complete(false, {}, error, m_lastFailureWasServerRejection);
}

void SettingsTyphonControlClient::handleSocketError(QLocalSocket::LocalSocketError error)
{
    Q_UNUSED(error);
    if (m_busy)
        complete(false, {}, protocolError(QStringLiteral("socket transport failed")));
}

void SettingsTyphonControlClient::handleDisconnected()
{
    if (!m_busy)
        return;
    if (m_socket.bytesAvailable() > 0) {
        handleReadyRead();
        if (!m_busy)
            return;
    }
    complete(false, {}, protocolError(QStringLiteral("socket disconnected")));
}

void SettingsTyphonControlClient::handleDeadline()
{
    if (m_busy)
        complete(false, {}, protocolError(QStringLiteral("response timeout")));
}

void SettingsTyphonControlClient::complete(bool success, const QVariantMap &result,
                                            const QString &error, bool serverRejection)
{
    if (!m_busy)
        return;
    m_deadlineTimer.stop();
    m_busy = false;
    m_activeRequestId = 0;
    m_encodedRequest.clear();
    m_response.clear();
    m_lastFailureWasServerRejection = serverRejection;
    m_socket.abort();
    emit requestFinished(success, result, error);
}

bool SettingsTyphonControlClient::parseResponse(const QByteArray &line, QVariantMap *result,
                                                 QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = protocolError(QStringLiteral("response JSON is invalid"));
        return false;
    }
    const QJsonObject responseObject = document.object();
    const QJsonValue protocol = responseObject.value(QStringLiteral("protocol"));
    const QJsonValue version = responseObject.value(QStringLiteral("version"));
    if (!protocol.isString() || protocol.toString() != QStringLiteral("astrea.control")
        || !version.isDouble() || !std::isfinite(version.toDouble())
        || version.toDouble() != 1.0) {
        *error = protocolError(QStringLiteral("response protocol is incompatible"));
        return false;
    }
    const QJsonValue responseId = responseObject.value(QStringLiteral("id"));
    const double responseIdNumber = responseId.toDouble();
    if (!responseId.isDouble() || !std::isfinite(responseIdNumber)
        || std::trunc(responseIdNumber) != responseIdNumber
        || responseId.toInteger() != static_cast<qint64>(m_activeRequestId)) {
        *error = protocolError(QStringLiteral("response id does not match request"));
        return false;
    }
    const QJsonValue ok = responseObject.value(QStringLiteral("ok"));
    if (!ok.isBool()) {
        *error = protocolError(QStringLiteral("response success flag is invalid"));
        return false;
    }
    if (ok.toBool()) {
        if (!responseObject.value(QStringLiteral("result")).isObject()) {
            *error = protocolError(QStringLiteral("successful response has no result object"));
            return false;
        }
        *result = responseObject.value(QStringLiteral("result")).toObject().toVariantMap();
        return true;
    }
    if (!responseObject.value(QStringLiteral("error")).isObject()) {
        *error = protocolError(QStringLiteral("error response has no error object"));
        return false;
    }
    const QJsonObject errorObject = responseObject.value(QStringLiteral("error")).toObject();
    const QString message = errorObject.value(QStringLiteral("message")).toString();
    *error = message.isEmpty() ? protocolError(QStringLiteral("server rejected request")) : message;
    m_lastFailureWasServerRejection = true;
    return false;
}

QString SettingsTyphonControlClient::discoverSocket(QString *error) const
{
    const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtime.isEmpty() || !QFileInfo(runtime).isAbsolute() || !secureDirectory(runtime)) {
        *error = protocolError(QStringLiteral("XDG_RUNTIME_DIR is not secure"));
        return {};
    }
    const QString root = runtime + QStringLiteral("/astrea/typhon");
    if (!secureDirectory(runtime + QStringLiteral("/astrea")) || !secureDirectory(root)) {
        *error = protocolError(QStringLiteral("Typhon runtime directory is not secure"));
        return {};
    }

    const QString display = qEnvironmentVariable("WAYLAND_DISPLAY");
    auto candidate = [&](const QString &name) -> QString {
        if (!validInstanceName(name))
            return {};
        const QString directory = root + QStringLiteral("/") + name;
        const QString path = directory + QStringLiteral("/control.sock");
        return secureDirectory(directory) && secureSocket(path) ? path : QString();
    };
    if (const QString preferred = candidate(display); !preferred.isEmpty())
        return preferred;

    QDir directory(root);
    const QStringList entries = directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                    QDir::Name);
    QString found;
    int count = 0;
    for (const QString &entry : entries) {
        if (const QString path = candidate(entry); !path.isEmpty()) {
            found = path;
            ++count;
        }
    }
    if (count == 1)
        return found;
    *error = count == 0 ? protocolError(QStringLiteral("no secure Typhon instance found"))
                        : protocolError(QStringLiteral("multiple Typhon instances found"));
    return {};
}

bool SettingsTyphonControlClient::validInstanceName(const QString &value)
{
    if (value.isEmpty() || value.size() > 128 || value == QStringLiteral(".")
        || value == QStringLiteral("..") || value.contains(QStringLiteral("..")))
        return false;
    for (const QChar character : value) {
        if (!character.isLetterOrNumber() && character != QLatin1Char('.')
            && character != QLatin1Char('_') && character != QLatin1Char('-'))
            return false;
    }
    return true;
}

bool SettingsTyphonControlClient::secureDirectory(const QString &path)
{
    const QFileInfo info(path);
    struct stat metadata {
    };
    return info.exists() && ::lstat(path.toLocal8Bit().constData(), &metadata) == 0
        && S_ISDIR(metadata.st_mode) && metadata.st_uid == geteuid()
        && (metadata.st_mode & 0777) == 0700;
}

bool SettingsTyphonControlClient::secureSocket(const QString &path)
{
    struct stat metadata {
    };
    return ::lstat(path.toLocal8Bit().constData(), &metadata) == 0
        && S_ISSOCK(metadata.st_mode) && metadata.st_uid == geteuid()
        && (metadata.st_mode & 0777) == 0600;
}
