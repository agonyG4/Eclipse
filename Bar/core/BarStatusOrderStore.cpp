#include "core/BarStatusOrderStore.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

BarStatusOrderStore::BarStatusOrderStore(QString configFilePath, QObject *parent)
    : QObject(parent)
    , m_configFilePath(configFilePath.isEmpty()
          ? QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
                .filePath(QStringLiteral("AstreaOS/ui/topbar-order.json"))
          : std::move(configFilePath))
    , m_order(defaultOrder())
{
    load();
}

QStringList BarStatusOrderStore::defaultOrder()
{
    return {QStringLiteral("builtin:network"), QStringLiteral("builtin:bluetooth"),
            QStringLiteral("builtin:volume"), QStringLiteral("builtin:clock")};
}

void BarStatusOrderStore::load()
{
    QFile file(m_configFilePath);
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly)) {
        setLastError(file.errorString());
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || document.object().value(QStringLiteral("version")).toInt() != 1
        || !document.object().value(QStringLiteral("items")).isArray()) {
        setLastError(parseError.error == QJsonParseError::NoError
                         ? QStringLiteral("Topbar order file has an unsupported format")
                         : parseError.errorString());
        return;
    }

    QStringList loaded;
    QSet<QString> seen;
    for (const QJsonValue &value : document.object().value(QStringLiteral("items")).toArray()) {
        if (!value.isString())
            continue;
        const QString key = value.toString();
        if (key.isEmpty() || seen.contains(key))
            continue;
        seen.insert(key);
        loaded.append(key);
    }
    for (const QString &key : defaultOrder()) {
        if (!seen.contains(key))
            loaded.append(key);
    }
    m_order = loaded;
}

void BarStatusOrderStore::registerItem(const QString &key)
{
    if (key.isEmpty() || m_order.contains(key))
        return;

    const QStringList builtins = defaultOrder();
    if (key.startsWith(QStringLiteral("tray:"))
        || key.startsWith(QStringLiteral("tray-fallback:"))) {
        int firstBuiltin = m_order.size();
        for (const QString &builtin : builtins) {
            const int position = m_order.indexOf(builtin);
            if (position >= 0)
                firstBuiltin = qMin(firstBuiltin, position);
        }
        m_order.insert(firstBuiltin, key);
    } else {
        m_order.append(key);
    }
    emit orderChanged();
}

QString BarStatusOrderStore::trayKey(const QString &id, const QString &addressKey) const
{
    const QString normalizedId = id.trimmed();
    if (normalizedId.isEmpty())
        return QStringLiteral("tray-fallback:") + addressKey;

    const qsizetype separator = addressKey.indexOf(QLatin1Char('|'));
    const QString objectPath = separator < 0 ? addressKey : addressKey.mid(separator + 1);
    return QStringLiteral("tray:")
        + QString::fromLatin1(QUrl::toPercentEncoding(normalizedId))
        + QLatin1Char(':') + objectPath;
}

bool BarStatusOrderStore::commitVisibleOrder(const QStringList &visibleOrder)
{
    QSet<QString> visibleSet;
    for (const QString &key : visibleOrder) {
        if (key.isEmpty() || visibleSet.contains(key)) {
            setLastError(QStringLiteral("Topbar order contains an empty or duplicate key"));
            emit persistenceFailed(m_lastError);
            return false;
        }
        visibleSet.insert(key);
    }

    QStringList candidate = m_order;
    const QStringList builtins = defaultOrder();
    for (const QString &key : visibleOrder) {
        if (candidate.contains(key))
            continue;
        if (key.startsWith(QStringLiteral("tray:"))
            || key.startsWith(QStringLiteral("tray-fallback:"))) {
            int firstBuiltin = candidate.size();
            for (const QString &builtin : builtins) {
                const int position = candidate.indexOf(builtin);
                if (position >= 0)
                    firstBuiltin = qMin(firstBuiltin, position);
            }
            candidate.insert(firstBuiltin, key);
        } else {
            candidate.append(key);
        }
    }
    QList<int> visiblePositions;
    for (int i = 0; i < candidate.size(); ++i) {
        if (visibleSet.contains(candidate.at(i)))
            visiblePositions.append(i);
    }
    if (visiblePositions.size() != visibleOrder.size()) {
        setLastError(QStringLiteral("Topbar order could not match all visible items"));
        emit persistenceFailed(m_lastError);
        return false;
    }
    for (qsizetype i = 0; i < visiblePositions.size(); ++i)
        candidate[visiblePositions.at(i)] = visibleOrder.at(i);

    if (!persist(candidate)) {
        emit persistenceFailed(m_lastError);
        return false;
    }
    if (m_order != candidate) {
        m_order = candidate;
        emit orderChanged();
    }
    setLastError({});
    return true;
}

bool BarStatusOrderStore::persist(const QStringList &order)
{
    const QFileInfo info(m_configFilePath);
    if (!QDir().mkpath(info.absolutePath())) {
        setLastError(QStringLiteral("Could not create the topbar configuration directory"));
        return false;
    }

    QJsonArray items;
    for (const QString &key : order)
        items.append(key);
    const QJsonObject root{{QStringLiteral("version"), 1}, {QStringLiteral("items"), items}};
    QSaveFile file(m_configFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setLastError(file.errorString());
        return false;
    }
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        setLastError(file.errorString());
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        setLastError(file.errorString());
        return false;
    }
    return true;
}

void BarStatusOrderStore::setLastError(const QString &error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}
