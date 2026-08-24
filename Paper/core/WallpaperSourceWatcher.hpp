#pragma once

#include "WallpaperDescriptor.hpp"

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

namespace Paper {

class WallpaperSourceWatcher final : public QObject
{
    Q_OBJECT

public:
    explicit WallpaperSourceWatcher(QObject *parent = nullptr);

    void setSource(const WallpaperDescriptor &descriptor);
    void clear();
    int watchedPathCountForTests() const;

signals:
    void sourceChanged();

private slots:
    void scheduleReconcile();
    void reconcileWatchPaths();

private:
    void refreshPaths();
    static QString localPathFor(const QString &source);

    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_configuredPath;
    QString m_resolvedPath;
};

} // namespace Paper
