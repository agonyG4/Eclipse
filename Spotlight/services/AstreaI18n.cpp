#include "services/AstreaI18n.hpp"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcessEnvironment>
#include <QDebug>
#include <QFileInfo>
#include <QStandardPaths>

static QString astreaConfigHome() {
    const QString xdg = qEnvironmentVariable("XDG_CONFIG_HOME").trimmed();
    if (!xdg.isEmpty())
        return xdg;
    return QDir::homePath() + QStringLiteral("/.config");
}

static QStringList astreaConfigWatchFiles() {
    const QString configHome = astreaConfigHome();
    return {
        configHome + QStringLiteral("/AstreaOS/system/settings.json"),
        configHome + QStringLiteral("/AstreaOS/system/system.json"),
    };
}

static QStringList astreaConfigWatchDirs() {
    QStringList dirs;
    for (const auto &path : astreaConfigWatchFiles()) {
        QFileInfo fi(path);
        const QString dirPath = fi.absoluteDir().absolutePath();
        if (!dirPath.isEmpty() && !dirs.contains(dirPath))
            dirs.append(dirPath);
        QDir parent = fi.absoluteDir();
        if (parent.cdUp()) {
            const QString parentPath = parent.absolutePath();
            if (!parentPath.isEmpty() && !dirs.contains(parentPath))
                dirs.append(parentPath);
        }
    }
    return dirs;
}

AstreaI18n::AstreaI18n(const QString &i18nDir, QObject *parent)
    : QObject(parent), m_i18nDir(i18nDir) {
    m_reloadDebounce = new QTimer(this);
    m_reloadDebounce->setSingleShot(true);
    m_reloadDebounce->setInterval(120);
    connect(m_reloadDebounce, &QTimer::timeout, this, &AstreaI18n::reload);

    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        m_reloadDebounce->start();
    });
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        m_reloadDebounce->start();
    });

    reload();
}

void AstreaI18n::reload() {
    const QString previousLanguage = m_language;
    m_ready = false;
    emit readyChanged();
    loadCatalogs();
    m_ready = true;
    emit readyChanged();
    emit messagesChanged();
    if (m_language != previousLanguage)
        emit languageChanged();
}

void AstreaI18n::loadCatalogs() {
    m_fallbackMessages = QJsonObject();
    m_activeMessages = QJsonObject();

    m_watcher->removePaths(m_watcher->files());
    m_watcher->removePaths(m_watcher->directories());

    QString fallbackPath = QDir(m_i18nDir).filePath(QStringLiteral("en_US.json"));
    QFile fallbackFile(fallbackPath);
    if (fallbackFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(fallbackFile.readAll());
        if (doc.isObject())
            m_fallbackMessages = doc.object();
        if (!m_watcher->files().contains(fallbackPath))
            m_watcher->addPath(fallbackPath);
    }

    m_language = detectLanguage();
    QString activePath = QDir(m_i18nDir).filePath(m_language + QStringLiteral(".json"));
    if (m_language != QStringLiteral("en_US")) {
        QFile activeFile(activePath);
        if (activeFile.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(activeFile.readAll());
            if (doc.isObject())
                m_activeMessages = doc.object();
            if (!m_watcher->files().contains(activePath))
                m_watcher->addPath(activePath);
        }
    }

    m_messages = QJsonObject();
    for (auto it = m_fallbackMessages.begin(); it != m_fallbackMessages.end(); ++it)
        m_messages.insert(it.key(), it.value());
    for (auto it = m_activeMessages.begin(); it != m_activeMessages.end(); ++it)
        m_messages.insert(it.key(), it.value());

    for (const auto &path : astreaConfigWatchFiles()) {
        QFileInfo fi(path);
        if (fi.exists() && !m_watcher->files().contains(path))
            m_watcher->addPath(path);
        QDir dir = fi.absoluteDir();
        const QString dirPath = dir.absolutePath();
        if (dir.exists() && !m_watcher->directories().contains(dirPath))
            m_watcher->addPath(dirPath);
        if (dir.cdUp()) {
            const QString parentPath = dir.absolutePath();
            if (dir.exists() && !m_watcher->directories().contains(parentPath))
                m_watcher->addPath(parentPath);
        }
    }

    for (const auto &dir : astreaConfigWatchDirs()) {
        if (!m_watcher->directories().contains(dir))
            m_watcher->addPath(dir);
    }
}

QString AstreaI18n::detectLanguage() const {
    auto env = QProcessEnvironment::systemEnvironment();
    QString configDir = astreaConfigHome() + QStringLiteral("/AstreaOS/system");
    QStringList configFiles = {QStringLiteral("settings.json"), QStringLiteral("system.json")};
    QStringList langKeys = {QStringLiteral("language"), QStringLiteral("locale"),
                            QStringLiteral("ui_language"), QStringLiteral("lang")};

    for (const QString &file : configFiles) {
        QString path = QDir(configDir).filePath(file);
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                for (const QString &key : langKeys) {
                    if (obj.contains(key) && obj.value(key).isString()) {
                        QString lang = obj.value(key).toString().trimmed();
                        if (!lang.isEmpty()) return lang;
                    }
                }
            }
        }
    }
    return QStringLiteral("en_US");
}

QString AstreaI18n::tr(const QString &key, const QString &fallback,
                       const QVariantMap &params) const {
    QString value;
    if (m_messages.contains(key))
        value = m_messages.value(key).toString();
    else if (!fallback.isEmpty())
        value = fallback;
    else
        value = key;

    if (!params.isEmpty()) {
        for (auto it = params.begin(); it != params.end(); ++it)
            value.replace(QStringLiteral("{") + it.key() + QStringLiteral("}"), it.value().toString());
    }
    return value;
}
