#pragma once

#include "core/WallpaperService.hpp"
#include "shared/platform/paper/PaperProtocol.hpp"

#include <QHash>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QPointer>

#include <optional>

class QLocalSocket;
class QTimer;

namespace Paper {

class WallpaperControlServer final : public QObject
{
    Q_OBJECT

public:
    explicit WallpaperControlServer(WallpaperService *service,
                                    QString endpoint = {},
                                    QObject *parent = nullptr);
    ~WallpaperControlServer() override;

    bool listen(QString *errorOut = nullptr);
    void stopListening();
    QString endpoint() const { return m_endpoint; }

    static QString defaultEndpoint();
    static QByteArray requestReply(const QString &endpoint,
                                   const QString &line,
                                   int timeoutMs = 500);

private:
    struct ClientState final
    {
        QByteArray buffer;
        QTimer *idleTimer = nullptr;
        std::optional<quint64> operationId;
    };

    void onNewConnection();
    void consume(QLocalSocket *socket);
    void handleLine(QLocalSocket *socket, const QByteArray &line);
    void sendReply(QLocalSocket *socket, const QByteArray &reply);
    void sendOperationResult(QLocalSocket *socket, const WallpaperOperationResult &result);
    void handleOperationFinished(quint64 id, WallpaperOperationResult result);
    void expireClient(QLocalSocket *socket);

    WallpaperService *m_service = nullptr;
    QString m_endpoint;
    QLocalServer m_server;
    QHash<QLocalSocket *, ClientState> m_clients;
    QHash<quint64, QPointer<QLocalSocket>> m_operationWaiters;
    QHash<quint64, WallpaperOperationResult> m_completedResults;
    bool m_endpointOwned = false;
    static constexpr int kMaxCommandSize = 4096;
    static constexpr int kMaxClients = 16;
    static constexpr int kClientIdleTimeoutMs = 500;
    static constexpr int kOperationTimeoutMs = Astrea::PaperProtocol::kClientOperationDeadlineMs;
};

} // namespace Paper
