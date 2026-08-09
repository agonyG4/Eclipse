#pragma once

#include <QObject>
#include <QTimer>

#include <cstdint>
#include <functional>
#include <memory>

struct wl_display;
class TyphonWaylandDisplay;

class TyphonSharedConnection final : public QObject {
    Q_OBJECT

public:
    enum class State {
        Stopped,
        Connecting,
        Authenticating,
        Ready,
        Degraded,
        Disconnected,
        Unsupported,
    };
    Q_ENUM(State)

    struct Hooks {
        std::function<bool()> connect;
        std::function<bool(QString *)> authenticate;
        std::function<void()> disconnect;
        std::function<void(std::function<void()>)> setDisconnectHandler;
    };

    explicit TyphonSharedConnection(Hooks hooks = {}, QObject *parent = nullptr);
    ~TyphonSharedConnection() override;

    void start();
    void stop();

    State state() const { return m_state; }
    bool isReady() const { return m_state == State::Ready; }
    std::uint64_t connectionGeneration() const { return m_generation; }
    std::uint64_t authenticationGeneration() const { return m_authenticationGeneration; }
    wl_display *nativeDisplay() const;
    TyphonWaylandDisplay *waylandDisplay() const;
    bool flush();

    // Deterministic test driver; production disconnects arrive from the display.
    void reconnectNowForTest();

signals:
    void stateChanged(TyphonSharedConnection::State state);
    void ready(std::uint64_t generation);
    void disconnected(std::uint64_t generation);
    void diagnostic(QString message);

private:
    void beginConnection();
    void handleDisconnected();
    void handleProtocolError(const QString &message);
    void setState(State state);
    void fail(const QString &message);
    void scheduleReconnect();
    int reconnectDelay() const;
    bool connectDisplay();
    bool authenticate(QString *diagnostic);
    void disconnectDisplay();

    struct Private;
    std::unique_ptr<Private> m_private;
    Hooks m_hooks;
    QTimer m_reconnectTimer;
    State m_state = State::Stopped;
    std::uint64_t m_generation = 0;
    std::uint64_t m_authenticationGeneration = 0;
    int m_backoffIndex = 0;
    bool m_started = false;
    bool m_failureInProgress = false;
};

Q_DECLARE_METATYPE(TyphonSharedConnection::State)
