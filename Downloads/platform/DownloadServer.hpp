#pragma once

#include "core/DownloadStore.hpp"

#include <QLocalServer>
#include <QLockFile>
#include <QHash>
#include <QSet>
#include <memory>

class QLocalSocket;

namespace Astrea::Downloads {

class DownloadServer final : public QObject {
public:
    static constexpr qsizetype MaxFrameBytes = 64 * 1024;
    static constexpr qint64 MaxPendingBytes = 2 * 1024 * 1024;
    static constexpr int MaxClients = 64;

    explicit DownloadServer(QObject *parent = nullptr);
    ~DownloadServer() override;
    bool listen(const QString &path, QString *errorOut);

private:
    void acceptConnections();
    void consume(QLocalSocket *socket);
    void dispatch(QLocalSocket *socket, const QByteArray &line);
    void reply(QLocalSocket *socket, const QJsonObject &message);
    void broadcast(const QJsonObject &event);

    std::unique_ptr<QLockFile> m_lock;
    QLocalServer m_server;
    DownloadStore m_store;
    QHash<QLocalSocket *, QByteArray> m_buffers;
    QSet<QLocalSocket *> m_watchers;
};

} // namespace Astrea::Downloads
