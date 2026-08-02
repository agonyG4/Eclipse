#pragma once

#include <QByteArray>
#include <QHash>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>

#include <functional>

class DockIpcServer final : public QObject {
    Q_OBJECT

public:
    enum class Command { Status, Reload, Show, Hide, Quit, Unknown };
    Q_ENUM(Command)

    struct IpcCommand {
        Command type = Command::Unknown;
    };

    explicit DockIpcServer(QObject *parent = nullptr);

    bool listen(const QString &name, QString *errorOut = nullptr);
    void stopListening();

    static bool sendCommand(const QString &serverName, const IpcCommand &command);
    static QByteArray requestReply(const QString &serverName, const IpcCommand &command,
                                   int timeoutMs = 500);
    static IpcCommand parseCommand(const QString &text);

    void setReplyCallback(std::function<QString()> callback) { m_replyCallback = std::move(callback); }

signals:
    void commandReceived(const DockIpcServer::IpcCommand &command);

private slots:
    void onNewConnection();

private:
    static QString serializeCommand(const IpcCommand &command);

    QLocalServer *m_server = nullptr;
    QString m_serverName;
    QHash<QLocalSocket *, QByteArray> m_buffers;
    std::function<QString()> m_replyCallback;
    static constexpr int kMaxCommandSize = 4096;
};

Q_DECLARE_METATYPE(DockIpcServer::IpcCommand)
