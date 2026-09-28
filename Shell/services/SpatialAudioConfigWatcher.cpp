#include "services/SpatialAudioConfigWatcher.hpp"

#include "system/audio/spatial/SpatialAudioConfig.hpp"

#include <QDir>
#include <QFileInfo>

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
    return Astrea::System::SpatialAudioConfigStore::defaultPath();
}

void SpatialAudioConfigWatcher::scheduleRefresh() { m_debounce.start(); }

void SpatialAudioConfigWatcher::refresh() {
    const auto snapshot =
        Astrea::System::SpatialAudioConfigStore::read(m_configPath);

    refreshWatchPaths();
    if (m_enabled != snapshot.enabled) {
        m_enabled = snapshot.enabled;
        emit enabledChanged(m_enabled);
    }
    if (m_errorString != snapshot.error) {
        m_errorString = snapshot.error;
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
