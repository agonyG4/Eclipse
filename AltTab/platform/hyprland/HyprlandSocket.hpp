#pragma once

#include <QObject>
#include <QLocalSocket>
#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QElapsedTimer>
#include <functional>

struct HyprlandCommandResult {
    bool success = false;
    QByteArray rawReply;
    QString errorCode;
    QString errorMessage;
};

inline constexpr qsizetype kMaxCommandReplyBytes = 1024 * 1024;
inline constexpr qsizetype kMaxEventBufferBytes = 64 * 1024;

class HyprlandCommandRequest final : public QObject {
    Q_OBJECT
public:
    struct Options {
        int timeoutMs;
        qsizetype maxReplyBytes;
        static Options defaults() { return {750, 1024 * 1024}; }
    };

    explicit HyprlandCommandRequest(QObject *parent = nullptr)
        : QObject(parent) {}

    void start(const QString &socketPath, const QByteArray &request, Options options = Options::defaults());

signals:
    void finished(HyprlandCommandResult result);

private:
    enum class State { Idle, Connecting, Writing, Reading, Finished };

    void complete(HyprlandCommandResult result);
    void onConnected();
    void onBytesWritten(qint64 bytes);
    void onReadyRead();
    void onDisconnected();
    void onTimeout();

    bool m_completed = false;
    State m_state = State::Idle;
    QLocalSocket *m_socket = nullptr;
    QTimer *m_timeout = nullptr;
    QByteArray m_request;
    QByteArray m_reply;
    qsizetype m_bytesWritten = 0;
    qsizetype m_maxReplyBytes = 1024 * 1024;
    QElapsedTimer m_latency;
};

class HyprlandSocket : public QObject {
    Q_OBJECT
public:
    explicit HyprlandSocket(QObject *parent = nullptr);
    ~HyprlandSocket() override;

    void start();
    void stop();
    void connectToEventSocket();
    bool isEventConnected() const;

    void sendCommand(const QByteArray &cmd, std::function<void(const HyprlandCommandResult &)> callback,
                     int timeoutMs = 750);

    void setEventCallback(std::function<void(const QString &)> cb) { m_eventCb = cb; }

    QString instanceSignature() const { return m_instanceSignature; }
    QString runtimeDir() const { return m_runtimeDir; }
    void setInstance(const QString &sig, const QString &dir) {
        m_instanceSignature = sig; m_runtimeDir = dir;
    }
    bool discoverInstance();

    // Health metrics
    bool commandInFlight() const { return m_commandInFlight; }
    quint64 lastCommandLatency() const { return m_lastCommandLatency; }
    QString lastCommandError() const { return m_lastCommandError; }
    qint64 lastCommandSuccessTime() const { return m_lastCommandSuccessTime; }
    qint64 lastEventTime() const { return m_lastEventTime; }
    QString lastEventError() const { return m_lastEventError; }
    qint64 lastSnapshotTime() const { return m_lastSnapshotTime; }
    bool lastActivationResult() const { return m_lastActivationOk; }

signals:
    void eventSocketConnected();
    void eventSocketDisconnected();
    void connectionLost();

private slots:
    void onEventReadyRead();
    void onEventDisconnected();
    void tryReconnect();

private:
    QString socketPath(const QString &name) const;
    void disconnectEventSocket();

    QString m_instanceSignature;
    QString m_runtimeDir;
    QLocalSocket *m_eventSocket = nullptr;
    std::function<void(const QString &)> m_eventCb;
    QTimer *m_reconnectTimer;
    int m_reconnectDelay = 1000;
    QByteArray m_eventBuffer;
    bool m_started = false;
    bool m_stopRequested = false;

    bool m_commandInFlight = false;
    quint64 m_lastCommandLatency = 0;
    QString m_lastCommandError;
    qint64 m_lastCommandSuccessTime = 0;
    qint64 m_lastEventTime = 0;
    QString m_lastEventError;
    qint64 m_lastSnapshotTime = 0;
    bool m_lastActivationOk = false;
};
