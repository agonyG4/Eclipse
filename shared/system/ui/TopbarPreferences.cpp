#include "TopbarPreferences.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

TopbarPreferences::TopbarPreferences(QObject *parent)
    : QObject(parent)
    , m_configPath(defaultConfigPath())
{
    m_reloadDebounce.setSingleShot(true);
    m_reloadDebounce.setInterval(40);
    connect(&m_reloadDebounce, &QTimer::timeout, this, &TopbarPreferences::reload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_reloadDebounce,
            qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_reloadDebounce,
            qOverload<>(&QTimer::start));
    reload();
}

QString TopbarPreferences::defaultConfigPath()
{
    return QDir::homePath() + QStringLiteral("/.config/AstreaOS/topbar.json");
}

QString TopbarPreferences::normalizedPopupStyle(const QString &style)
{
    return style.trimmed().compare(QStringLiteral("attached"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("attached")
        : QStringLiteral("floating");
}

void TopbarPreferences::setConfigPath(const QString &path)
{
    const QString resolved = path.isEmpty() ? defaultConfigPath() : path;
    if (m_configPath == resolved)
        return;
    m_configPath = resolved;
    emit configPathChanged();
    reload();
}

void TopbarPreferences::watch()
{
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    const QFileInfo info(m_configPath);
    // QSaveFile replaces the file, which drops a file watch; the directory
    // watch catches the rename and re-arms it.
    if (info.absoluteDir().exists())
        m_watcher.addPath(info.absolutePath());
    if (info.exists())
        m_watcher.addPath(info.absoluteFilePath());
}

void TopbarPreferences::reload()
{
    watch();
    QFile file(m_configPath);
    if (!file.exists()) {
        applyPopupStyle(QStringLiteral("floating"));
        setLastError({});
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        setLastError(file.errorString());
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        // A writer may be mid-replace; keep the current value.
        setLastError(parseError.error == QJsonParseError::NoError
                         ? QStringLiteral("Topbar preferences are not a JSON object")
                         : parseError.errorString());
        return;
    }
    applyPopupStyle(normalizedPopupStyle(
        document.object().value(QStringLiteral("popupStyle")).toString()));
    setLastError({});
}

void TopbarPreferences::setPopupStyle(const QString &style)
{
    const QString normalized = normalizedPopupStyle(style);
    if (m_popupStyle == normalized)
        return;
    applyPopupStyle(normalized);
    persist();
}

void TopbarPreferences::applyPopupStyle(const QString &style)
{
    if (m_popupStyle == style)
        return;
    m_popupStyle = style;
    emit popupStyleChanged();
}

bool TopbarPreferences::persist()
{
    const QFileInfo info(m_configPath);
    if (!QDir().mkpath(info.absolutePath())) {
        setLastError(QStringLiteral("Could not create the topbar configuration directory"));
        return false;
    }

    QJsonObject root;
    QFile existing(m_configPath);
    if (existing.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(existing.readAll());
        if (document.isObject())
            root = document.object();
    }
    root.insert(QStringLiteral("popupStyle"), m_popupStyle);

    QSaveFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        setLastError(file.errorString());
        return false;
    }
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size() || !file.commit()) {
        setLastError(file.errorString());
        return false;
    }
    setLastError({});
    watch();
    return true;
}

void TopbarPreferences::setLastError(const QString &error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}
