#pragma once

#include <QQuickImageProvider>
#include <QIcon>
#include <QPixmap>
#include <QCache>
#include <QFileSystemWatcher>
#include <QReadWriteLock>
#include <QObject>
#include <QSet>
#include <QHash>
#include <QList>
#include <atomic>

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
    QPixmap resolveIcon(const QString &iconName, int size);
    QString lookupXdgTheme(const QString &iconName, int size) const;
    QStringList themeSearchDirs() const;
    void discoverThemeInheritance(const QString &themeName, QStringList &result,
                                  const QStringList &searchDirs, QSet<QString> &visited) const;
    void refreshThemeState();
    QStringList watchedConfigFiles() const;
    QStringList watchedConfigDirectories() const;
    static QStringList iconSubdirs(int size);
    static QStringList iconExtensions();
    static QStringList iconPrefixes();

    QCache<QString, QPixmap> m_cache;
    QReadWriteLock m_cacheLock;
    QHash<QString, int> m_negativeCache;
    mutable QReadWriteLock m_negativeCacheLock;
    std::atomic<int> m_themeRevision{0};
    int m_nextNegSeq = 0;
    QFileSystemWatcher m_themeWatcher;

    static constexpr int kCacheMaxCost = 8 * 1024 * 1024;   // 8 MB
    static constexpr int kMaxNegativeEntries = 1024;
};
