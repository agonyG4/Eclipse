#pragma once

#include <QHash>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>

#include <functional>

class ShellIpcServer final : public QObject {
    Q_OBJECT

public:
    struct Command {
        bool valid = false;
        QString feature;
        QString action;
        QString argument;
    };

    explicit ShellIpcServer(QObject *parent = nullptr);

    bool listen(const QString &name, QString *errorOut = nullptr);
    void stopListening();

    static Command parseCommand(const QString &line);
    static QString serializeCommand(const Command &command);
    static bool sendCommand(const QString &name, const Command &command);
    static QByteArray requestReply(const QString &name, const Command &command,
                                   int timeoutMs = 500);

    void setReplyCallback(std::function<QString(const Command &)> callback)
    { m_replyCallback = std::move(callback); }

signals:
    void commandReceived(const ShellIpcServer::Command &command);

private:
    void onNewConnection();
    void consume(QLocalSocket *socket);

    QLocalServer m_server;
    QHash<QLocalSocket *, QByteArray> m_buffers;
    std::function<QString(const Command &)> m_replyCallback;
    static constexpr int kMaxCommandSize = 4096;
};

Q_DECLARE_METATYPE(ShellIpcServer::Command)
