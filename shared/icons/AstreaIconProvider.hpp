#pragma once

#include <QQuickImageProvider>
#include <QPixmap>
#include <QCache>
#include <QFileSystemWatcher>
#include <QReadWriteLock>
#include <QObject>
#include <QHash>
#include <atomic>

#include "icons/IconRenderRequest.hpp"
#include <AstreaSDK/IconThemeCatalog.hpp>

class AstreaIconProvider : public QQuickImageProvider {
    Q_OBJECT
    Q_PROPERTY(int themeRevision READ themeRevision NOTIFY cacheInvalidated)
public:
    AstreaIconProvider();
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
    void clearCache();
    int themeRevision() const { return m_themeRevision.load(std::memory_order_acquire); }

signals:
    void cacheInvalidated();

private:
    QPixmap resolveIcon(const QString &iconName, const IconRenderRequest &request);
    QStringList themeSearchDirs() const;
    void refreshThemeState();
    void handleWatchedPathChanged(const QString &path);
    QHash<QString, QByteArray> currentConfigFingerprints() const;
    QStringList watchedConfigFiles() const;
    QStringList watchedConfigDirectories() const;

    AstreaSDK::IconThemeCatalog m_catalog;
    QCache<QString, QPixmap> m_cache;
    QReadWriteLock m_cacheLock;
    QHash<QString, int> m_negativeCache;
    mutable QReadWriteLock m_negativeCacheLock;
    std::atomic<int> m_themeRevision{0};
    int m_nextNegSeq = 0;
    QFileSystemWatcher m_themeWatcher;
    QStringList m_assetWatchPaths;
    QHash<QString, QByteArray> m_configFingerprints;

    static constexpr int kCacheMaxCost = 8 * 1024 * 1024;   // 8 MB
    static constexpr int kMaxNegativeEntries = 1024;
};
