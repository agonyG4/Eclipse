#include "services/SpatialAudioConfigWatcher.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <utility>

SpatialAudioConfigWatcher::SpatialAudioConfigWatcher(const QString &configPath,
                                                     QObject *parent)
    : QObject(parent), m_configPath(QFileInfo(configPath).absoluteFilePath()),
      m_watcher(), m_debounce() {
    QDir root = QFileInfo(m_configPath).absoluteDir();
    root.cdUp(); // AstreaOS
    root.cdUp(); // config home
    m_watchRoot = root.absolutePath();

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this,
            &SpatialAudioConfigWatcher::refresh);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this,
            [this](const QString &) { scheduleRefresh(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
            [this](const QString &) { scheduleRefresh(); });

    refresh();
}

QString SpatialAudioConfigWatcher::defaultConfigPath() {
    QString configHome = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (configHome.isEmpty())
        configHome = QDir::home().filePath(QStringLiteral(".config"));
    return QDir(configHome)
        .filePath(QStringLiteral("AstreaOS/audio/spatial.json"));
}

void SpatialAudioConfigWatcher::scheduleRefresh() { m_debounce.start(); }

void SpatialAudioConfigWatcher::refresh() {
    bool enabled = false;
    QString error;
    const QFileInfo configInfo(m_configPath);
    if (configInfo.exists()) {
        QFile file(m_configPath);
        if (!file.open(QIODevice::ReadOnly)) {
            error =
                QStringLiteral("Cannot read Spatial Audio configuration: %1")
                    .arg(file.errorString());
        } else {
            QJsonParseError parseError;
            const QJsonDocument document =
                QJsonDocument::fromJson(file.readAll(), &parseError);
            if (parseError.error != QJsonParseError::NoError) {
                error =
                    QStringLiteral("Invalid Spatial Audio configuration: %1")
                        .arg(parseError.errorString());
            } else if (!document.isObject()) {
                error = QStringLiteral(
                    "Spatial Audio configuration must be a JSON object");
            } else {
                const QJsonObject object = document.object();
                const QJsonValue enabledValue =
                    object.value(QStringLiteral("enabled"));
                if (enabledValue.isUndefined()) {
                    enabled = false;
                } else if (enabledValue.isBool()) {
                    enabled = enabledValue.toBool();
                } else {
                    error = QStringLiteral("Spatial Audio configuration "
                                           "'enabled' must be boolean");
                }
            }
        }
    }

    refreshWatchPaths();
    if (m_enabled != enabled) {
        m_enabled = enabled;
        emit enabledChanged(m_enabled);
    }
    if (m_errorString != error) {
        m_errorString = std::move(error);
        emit errorChanged(m_errorString);
    }
}

void SpatialAudioConfigWatcher::refreshWatchPaths() {
    const QString filePath = m_configPath;
    const QString targetDirectory = QFileInfo(filePath).absolutePath();
    QString nearestExisting = m_watchRoot;
    while (!QFileInfo(nearestExisting).isDir()) {
        const QString parent = QFileInfo(nearestExisting).absolutePath();
        if (parent == nearestExisting)
            break;
        nearestExisting = parent;
    }

    QStringList directories;
    QString directory = targetDirectory;
    while (true) {
        if (QFileInfo(directory).isDir())
            directories.prepend(directory);
        if (directory == nearestExisting)
            break;
        const QString parent = QFileInfo(directory).absolutePath();
        if (parent == directory)
            break;
        directory = parent;
    }

    for (const QString &path : m_watcher.directories()) {
        if (!directories.contains(path))
            m_watcher.removePath(path);
    }
    for (const QString &path : directories) {
        if (!m_watcher.directories().contains(path))
            m_watcher.addPath(path);
    }

    if (m_watcher.files().contains(filePath))
        m_watcher.removePath(filePath);
    if (QFileInfo(filePath).isFile())
        m_watcher.addPath(filePath);
}
