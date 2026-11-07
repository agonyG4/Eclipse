#pragma once

#include <QQuickImageProvider>
#include <QPixmap>
#include <QCache>
#include <QFileSystemWatcher>
#include <QReadWriteLock>
#include <QObject>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <atomic>
#include <memory>
#include <optional>

#include "icons/AppTileHook.hpp"
#include "icons/IconRenderRequest.hpp"
#include <AstreaSDK/IconThemeCatalog.hpp>

class AstreaIconProvider : public QQuickImageProvider {
    Q_OBJECT
    Q_PROPERTY(int themeRevision READ themeRevision NOTIFY cacheInvalidated)
    // Whether app art the active theme lacks is wrapped in runtime tiles.
    Q_PROPERTY(bool appTilesActive READ appTilesActive NOTIFY cacheInvalidated)
public:
    AstreaIconProvider();
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
    void clearCache();
    int themeRevision() const { return m_themeRevision.load(std::memory_order_acquire); }
    bool appTilesActive() { return AstreaTiles::available() && activeThemeWantsTiles(); }

signals:
    void cacheInvalidated();

private:
    enum class TileOutcome { NotApplicable, Ready, Pending };
    struct TileShared;

    QPixmap resolveIcon(const QString &iconName, const IconRenderRequest &request,
                        std::optional<AstreaTiles::Variant> tile = std::nullopt);
    QPixmap resolvePlainIcon(const QString &iconName, const IconRenderRequest &request);
    TileOutcome resolveTiledIcon(const QString &iconName, const IconRenderRequest &request,
                                 AstreaTiles::Variant variant, QPixmap *result);
    QString tileSourceFor(const QString &iconName);
    bool activeThemeWantsTiles();
    void loadTileConfig();
    void scheduleTileRefresh();
    void invalidateRenderedIcons();
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

    // Runtime app tiles: masters and job bookkeeping shared with workers.
    std::shared_ptr<TileShared> m_tiles;
    QHash<QString, bool> m_themeTileFlags;
    QHash<QString, QString> m_tileOverrides;
    QSet<QString> m_keepDark;
    bool m_tileConfigLoaded = false;
    QTimer m_tileRefreshTimer;

    static constexpr int kCacheMaxCost = 8 * 1024 * 1024;   // 8 MB
    static constexpr int kMaxNegativeEntries = 1024;
};
