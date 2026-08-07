#pragma once

#include <QObject>

#include <cstdint>
#include <memory>

enum class TyphonShortcutPhase {
    Pressed,
    Repeated,
    Released,
    Cancelled,
};
Q_DECLARE_METATYPE(TyphonShortcutPhase)

enum class TyphonShortcutConnectionState {
    Stopped,
    Connecting,
    WaitingForManager,
    Registering,
    Ready,
    Degraded,
    Disconnected,
    Unsupported,
};
Q_DECLARE_METATYPE(TyphonShortcutConnectionState)

struct TyphonShortcutClientPrivate;

class TyphonShortcutClient final : public QObject {
    Q_OBJECT

public:
    explicit TyphonShortcutClient(QObject *parent = nullptr);
    ~TyphonShortcutClient() override;

    void start();
    void stop();

    TyphonShortcutConnectionState state() const;
    bool isReady() const;
    int registeredShortcutCount() const;
    std::uint64_t connectionGeneration() const;

signals:
    void stateChanged(TyphonShortcutConnectionState state);
    void shortcutEvent(QString namespaceName, QString name, TyphonShortcutPhase phase,
                       std::uint32_t serial, std::uint32_t timestamp);
    void diagnostic(QString message);

private:
    friend struct TyphonShortcutClientPrivate;

    void beginConnection();
    void setState(TyphonShortcutConnectionState state);
    void enterFailure(const QString &message, bool reconnect);
    void enterUnsupported(const QString &message);
    void handleDisplayDisconnected();
    void scheduleReconnect();
    int reconnectDelay() const;

    std::unique_ptr<TyphonShortcutClientPrivate> m_private;
};
