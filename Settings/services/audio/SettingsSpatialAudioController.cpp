#include "services/audio/SettingsSpatialAudioController.hpp"

#include "system/audio/spatial/SpatialAudioConfig.hpp"

#include <QDir>
#include <QFileInfo>

#include <utility>

SettingsSpatialAudioController::SettingsSpatialAudioController(const QString &configPath,
                                                               QObject *parent)
    : QObject(parent)
    , m_configPath(QFileInfo(configPath.isEmpty()
                                 ? Astrea::System::SpatialAudioConfigStore::defaultPath()
                                 : configPath)
                       .absoluteFilePath())
    , m_watcher()
    , m_debounce()
{
    QDir root = QFileInfo(m_configPath).absoluteDir();
    root.cdUp();
    root.cdUp();
    m_watchRoot = root.absolutePath();

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
    connect(&m_debounce, &QTimer::timeout, this,
            [this] { refresh(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this,
            [this](const QString &) { scheduleRefresh(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
            [this](const QString &) { scheduleRefresh(); });

    refresh();
}

bool SettingsSpatialAudioController::setSpatialAudioEnabled(bool enabled)
{
    QString error;
    if (!Astrea::System::SpatialAudioConfigStore::writeEnabled(m_configPath, enabled, &error)) {
        if (m_persistenceError != error) {
            m_persistenceError = std::move(error);
            emit configurationChanged();
        }
        return false;
    }

    refresh(true);
    return true;
}

void SettingsSpatialAudioController::refresh(bool clearPersistenceError)
{
    const auto snapshot = Astrea::System::SpatialAudioConfigStore::read(m_configPath);
    refreshWatchPaths();

    const bool sourceChanged = m_enabled != snapshot.enabled
        || m_configValid != snapshot.valid || m_configError != snapshot.error;
    const bool persistenceErrorChanged = clearPersistenceError && !m_persistenceError.isEmpty();
    if (sourceChanged || persistenceErrorChanged)
        m_persistenceError.clear();

    if (!sourceChanged && !persistenceErrorChanged)
        return;

    m_enabled = snapshot.enabled;
    m_configValid = snapshot.valid;
    m_configError = snapshot.error;
    emit configurationChanged();
}

void SettingsSpatialAudioController::scheduleRefresh()
{
    m_debounce.start();
}

void SettingsSpatialAudioController::refreshWatchPaths()
{
    const QString targetDirectory = QFileInfo(m_configPath).absolutePath();
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

    if (m_watcher.files().contains(m_configPath))
        m_watcher.removePath(m_configPath);
    if (QFileInfo(m_configPath).isFile())
        m_watcher.addPath(m_configPath);
}
