#include "RegionalPreferences.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
RegionalPreferences::RegionalPreferences(QObject *parent)
    : RegionalPreferences(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                              "/AstreaOS/system/settings.json",
                          parent)
{
}
RegionalPreferences::RegionalPreferences(QString path, QObject *parent)
    : QObject(parent), m_path(std::move(path))
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { reload(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { reload(); });
    connect(&m_fallback, &QTimer::timeout, this, [this] { reload(); });
    m_fallback.setInterval(5000);
    m_fallback.start();
    reload();
}
QString RegionalPreferences::defaultTimeFormat(const QString &country)
{
    const auto territory = QLocale::codeToTerritory(country);
    if (territory == QLocale::AnyTerritory)
        return QStringLiteral("24h");
    const auto format = QLocale(QLocale::AnyLanguage, territory).timeFormat(QLocale::ShortFormat);
    return format.contains(u'a', Qt::CaseInsensitive) ? QStringLiteral("12h") : QStringLiteral("24h");
}
QLocale RegionalPreferences::locale() const
{
    const QLocale language(m_language);
    const auto territory = QLocale::codeToTerritory(m_country);
    return territory == QLocale::AnyTerritory ? language : QLocale(language.language(), territory);
}
void RegionalPreferences::reload()
{
    QFile file(m_path);
    if (file.exists() && !m_watcher.files().contains(m_path))
        m_watcher.addPath(m_path);
    auto parent = QFileInfo(m_path).absoluteDir();
    while (!parent.exists() && parent.cdUp()) {
    }
    if (parent.exists() && !m_watcher.directories().contains(parent.absolutePath())) {
        if (!m_watcher.directories().isEmpty())
            m_watcher.removePaths(m_watcher.directories());
        m_watcher.addPath(parent.absolutePath());
    }
    if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024)
        return;
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return;
    const auto root = doc.object();
    const auto region = root.value("region").toObject();
    const auto language = root.value("language").toString("en_US");
    const auto country = region.value("country_code").toString("BR");
    auto format = region.value("time_format").toString("system");
    if (QLocale(language).language() == QLocale::C ||
        QLocale::codeToTerritory(country) == QLocale::AnyTerritory)
        return;
    if (format == "system")
        format = defaultTimeFormat(country);
    if (format != "12h" && format != "24h")
        return;
    if (m_language == language && m_country == country && m_timeFormat == format)
        return;
    m_language = language;
    m_country = country;
    m_timeFormat = format;
    emit changed();
}
