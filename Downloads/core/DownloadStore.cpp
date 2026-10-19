#include "core/DownloadStore.hpp"

#include <QJsonDocument>

#include <algorithm>
#include <cmath>

namespace Astrea::Downloads {
namespace {
bool validText(const QJsonValue &value, int maxLength)
{
    return value.isString() && !value.toString().trimmed().isEmpty()
        && value.toString().size() <= maxLength && !value.toString().contains(QChar::Null);
}

bool validBytes(const QJsonValue &value)
{
    const double number = value.toDouble(-1);
    return value.isDouble() && std::isfinite(number) && number >= 0
        && number <= 9007199254740991.0 && std::floor(number) == number;
}

QString key(const QString &source, const QString &id)
{
    return source + QChar::Null + id;
}
} // namespace

QJsonObject failure(const QString &code, const QString &message)
{
    return {{"version", 1}, {"ok", false}, {"error", code}, {"message", message}};
}

QJsonObject DownloadStore::update(const QJsonObject &snapshot)
{
    const auto source = snapshot.value("source");
    const auto id = snapshot.value("id");
    const auto filename = snapshot.value("filename");
    const auto state = snapshot.value("state");
    const auto received = snapshot.value("receivedBytes");
    const auto total = snapshot.value("totalBytes");
    const QStringList states = {"downloading", "paused", "completed", "cancelled", "failed"};
    if (!validText(source, 128) || !validText(id, 256) || !validText(filename, 4096)
        || !state.isString() || !states.contains(state.toString())
        || !validBytes(received) || (!total.isNull() && !validBytes(total))) {
        return failure("invalid_download", "Expected source, id, filename, state, receivedBytes "
                       "and totalBytes (nonnegative safe integers; totalBytes may be null)");
    }

    const QString identity = key(source.toString(), id.toString());
    if (!m_downloads.contains(identity) && m_downloads.size() >= MaxDownloads)
        return failure("capacity", "Remove retained downloads before adding another");

    QJsonValue progress(QJsonValue::Null);
    if (state.toString() == "completed")
        progress = 1.0;
    else if (total.isDouble() && total.toDouble() > 0)
        progress = std::clamp(received.toDouble() / total.toDouble(), 0.0, 1.0);
    const QJsonObject item{{"source", source}, {"id", id}, {"filename", filename},
                           {"state", state}, {"receivedBytes", received},
                           {"totalBytes", total}, {"progress", progress}};
    const auto previous = m_downloads.constFind(identity);
    const qsizetype previousBytes = previous == m_downloads.cend() ? 0
        : QJsonDocument(previous.value()).toJson(QJsonDocument::Compact).size() + 1;
    const qsizetype nextBytes = QJsonDocument(item).toJson(QJsonDocument::Compact).size() + 1;
    if (m_storedBytes - previousBytes + nextBytes > MaxStoredBytes)
        return failure("capacity", "Retained download metadata exceeds the 1 MiB limit");
    m_downloads.insert(identity, item);
    m_storedBytes += nextBytes - previousBytes;
    return {{"version", 1}, {"ok", true}, {"download", item}};
}

QJsonArray DownloadStore::list() const
{
    QJsonArray result;
    for (const auto &item : m_downloads)
        result.append(item);
    return result;
}

QJsonObject DownloadStore::remove(const QString &source, const QString &id)
{
    const auto entry = m_downloads.find(key(source, id));
    if (entry == m_downloads.end())
        return failure("not_found", "Download not found");
    m_storedBytes -= QJsonDocument(entry.value()).toJson(QJsonDocument::Compact).size() + 1;
    m_downloads.erase(entry);
    return {{"version", 1}, {"ok", true}, {"source", source}, {"id", id}};
}
} // namespace Astrea::Downloads
