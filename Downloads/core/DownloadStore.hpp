#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

namespace Astrea::Downloads {

QJsonObject failure(const QString &code, const QString &message);

// Complete producer snapshots, retained in memory until explicitly removed.
// Access from one thread (the local service's event loop).
class DownloadStore {
public:
    static constexpr int MaxDownloads = 4096;
    static constexpr qsizetype MaxStoredBytes = 1024 * 1024;
    QJsonObject update(const QJsonObject &snapshot);
    QJsonArray list() const;
    QJsonObject remove(const QString &source, const QString &id);

private:
    QMap<QString, QJsonObject> m_downloads;
    qsizetype m_storedBytes = 0;
};

} // namespace Astrea::Downloads
