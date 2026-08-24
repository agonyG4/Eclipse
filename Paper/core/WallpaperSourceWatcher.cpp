#include "WallpaperSourceWatcher.hpp"

#include "WallpaperResolver.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QUrl>

namespace Paper {

WallpaperSourceWatcher::WallpaperSourceWatcher(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(75);
    connect(&m_watcher,
            &QFileSystemWatcher::fileChanged,
            this,
            &WallpaperSourceWatcher::scheduleReconcile);
    connect(&m_watcher,
            &QFileSystemWatcher::directoryChanged,
            this,
            &WallpaperSourceWatcher::scheduleReconcile);
    connect(&m_debounce,
            &QTimer::timeout,
            this,
            &WallpaperSourceWatcher::reconcileWatchPaths);
}

void WallpaperSourceWatcher::setSource(const WallpaperDescriptor &descriptor)
{
    const auto configuredPath = localPathFor(descriptor.source());
    const auto resolvedPath = localPathFor(descriptor.resolvedSource().isEmpty()
                                                ? descriptor.source()
                                                : descriptor.resolvedSource());
    if (configuredPath == m_configuredPath && resolvedPath == m_resolvedPath) {
        refreshPaths();
        return;
    }
    m_configuredPath = configuredPath;
    m_resolvedPath = resolvedPath;
    refreshPaths();
}

int WallpaperSourceWatcher::watchedPathCountForTests() const
{
    return m_watcher.files().size() + m_watcher.directories().size();
}

void WallpaperSourceWatcher::clear()
{
    m_debounce.stop();
    const auto files = m_watcher.files();
    const auto directories = m_watcher.directories();
    if (!files.isEmpty())
        m_watcher.removePaths(files);
    if (!directories.isEmpty())
        m_watcher.removePaths(directories);
    m_configuredPath.clear();
    m_resolvedPath.clear();
}

void WallpaperSourceWatcher::scheduleReconcile()
{
    if (!m_configuredPath.isEmpty() || !m_resolvedPath.isEmpty()) {
        m_debounce.start();
    }
}

void WallpaperSourceWatcher::reconcileWatchPaths()
{
    refreshPaths();
    if (!m_configuredPath.isEmpty() || !m_resolvedPath.isEmpty()) {
        emit sourceChanged();
    }
}

void WallpaperSourceWatcher::refreshPaths()
{
    const auto files = m_watcher.files();
    const auto directories = m_watcher.directories();
    if (!files.isEmpty())
        m_watcher.removePaths(files);
    if (!directories.isEmpty())
        m_watcher.removePaths(directories);
    if (m_configuredPath.isEmpty() && m_resolvedPath.isEmpty()) {
        return;
    }

    QSet<QString> paths;
    const auto addSourceAndParent = [&paths](const QString &path) {
        if (path.isEmpty()) {
            return;
        }
        const QFileInfo info(path);
        const auto absolutePath = info.absoluteFilePath();
        const auto parentPath = info.absolutePath();
        if (QFileInfo(parentPath).isDir()) {
            paths.insert(parentPath);
        }
        if (info.exists() && (info.isFile() || info.isSymLink())) {
            paths.insert(absolutePath);
        }
    };
    addSourceAndParent(m_configuredPath);
    addSourceAndParent(m_resolvedPath);
    for (const auto &path : paths) {
        m_watcher.addPath(path);
    }
}

QString WallpaperSourceWatcher::localPathFor(const QString &source)
{
    const auto expanded = WallpaperResolver::expandSource(source);
    const QUrl url(expanded);
    if (url.isValid() && url.scheme().compare(QStringLiteral("file"), Qt::CaseInsensitive) == 0) {
        return QFileInfo(url.toLocalFile()).absoluteFilePath();
    }
    if (!url.scheme().isEmpty() || expanded.startsWith(QStringLiteral(":/"))) {
        return {};
    }
    return QFileInfo(expanded).absoluteFilePath();
}

} // namespace Paper
