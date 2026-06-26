#pragma once

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QByteArray>
#include <QString>
#include <QHash>
#include <functional>

class AltTabIpcServer : public QObject {
    Q_OBJECT
public:
    enum class Command {
        Next,
        Previous,
        Commit,
        Cancel,
        Show,
        Hide,
        ReloadWindows,
        Status,
        Unknown
    };

    struct IpcCommand {
        Command type = Command::Unknown;
        QString text;
    };

    explicit AltTabIpcServer(QObject *parent = nullptr);

    bool listen(const QString &name, QString *errorOut = nullptr);
    void stopListening();

    static bool sendCommand(const QString &serverName, const IpcCommand &cmd);
    static QByteArray requestReply(const QString &serverName, const IpcCommand &cmd, int timeoutMs = 500);
    static IpcCommand parseCommand(const QString &text);

    void setReplyCallback(std::function<QString(const IpcCommand &)> cb) { m_replyCb = cb; }

signals:
    void commandReceived(const AltTabIpcServer::IpcCommand &cmd);

private slots:
    void onNewConnection();

private:
    QLocalServer *m_server = nullptr;
    QString m_serverName;
    QHash<QLocalSocket *, QByteArray> m_buffers;
    std::function<QString(const IpcCommand &)> m_replyCb;
    static constexpr int kMaxCommandSize = 4096;
};
