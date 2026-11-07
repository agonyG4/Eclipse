#include "icons/AstreaIconProvider.hpp"
#include "icons/AstreaIconTheme.hpp"

#include <QIcon>
#include <QPixmapCache>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QStandardPaths>
#include <QReadLocker>
#include <QWriteLocker>
#include <QCoreApplication>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QSaveFile>
#include <QThreadPool>

#include <algorithm>
#include <cmath>
#include <limits>

struct AstreaIconProvider::TileShared final {
    QMutex mutex;
    // 512 px masters by tileMasterKey(); 2 MB each, so bounded.
    QCache<QString, AstreaTiles::Masters> masters{64 * 1024 * 1024};
    QSet<QString> pending;
    QSet<QString> failed;
};

namespace {

QHash<QString, QString> queryParameters(const QString &query)
{
    QHash<QString, QString> parameters;
    for (const QString &part : query.split(QLatin1Char('&'), Qt::SkipEmptyParts)) {
        const int separator = part.indexOf(QLatin1Char('='));
        const QString encodedKey = separator >= 0 ? part.left(separator) : part;
        const QString encodedValue = separator >= 0 ? part.mid(separator + 1) : QString();
        parameters.insert(QUrl::fromPercentEncoding(encodedKey.toUtf8()),
                          QUrl::fromPercentEncoding(encodedValue.toUtf8()));
    }
    return parameters;
}

bool parseReal(const QString &value, qreal *result)
{
    if (!result)
        return false;
    bool ok = false;
    const qreal parsed = value.toDouble(&ok);
    if (!ok || !std::isfinite(parsed) || parsed <= 0.0)
        return false;
    *result = parsed;
    return true;
}

QPixmap renderIcon(const QIcon &icon, const IconRenderRequest &request)
{
    if (icon.isNull())
        return {};

    QPixmap result = icon.pixmap(QSize(request.logicalExtent, request.logicalExtent),
                                 request.devicePixelRatio,
                                 QIcon::Normal,
                                 QIcon::Off);

    if (result.isNull())
        return {};

    result.setDevicePixelRatio(1.0);
    return result;
}

IconRenderRequest fallbackRequest(int logicalExtent)
{
    return *IconRenderRequest::fromValues(qMax(1, logicalExtent), 1.0);
}

QString tileCacheDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/AstreaOS/app-tiles");
}

// One master pair per source file, its contents (size + mtime), keep-dark and
// the renderer's version: any change makes a new key.
QString tileMasterKey(const QString &sourcePath, bool keepDark, int version)
{
    const QFileInfo info(sourcePath);
    const QByteArray material = QStringLiteral("%1|%2|%3|%4|v%5")
        .arg(info.absoluteFilePath())
        .arg(info.size())
        .arg(info.lastModified().toMSecsSinceEpoch())
        .arg(keepDark ? 1 : 0)
        .arg(version)
        .toUtf8();
    return QString::fromLatin1(
        QCryptographicHash::hash(material, QCryptographicHash::Sha1).toHex());
}

QSet<QString> readNameList(const QString &path)
{
    QSet<QString> names;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return names;
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine());
        line = line.section(QLatin1Char('#'), 0, 0).trimmed();
        if (!line.isEmpty())
            names.insert(line);
    }
    return names;
}

QHash<QString, QString> readOverrides(const QString &path)
{
    QHash<QString, QString> overrides;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return overrides;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0 || eq == line.size() - 1)
            continue;
        QString source = line.mid(eq + 1).trimmed();
        if (source.startsWith(QStringLiteral("~/")))
            source = QDir::homePath() + source.mid(1);
        overrides.insert(line.left(eq).trimmed(), source);
    }
    return overrides;
}

QString tileBaseName(const QString &iconName)
{
    if (iconName.startsWith(QLatin1Char('/')))
        return QFileInfo(iconName).completeBaseName();
    return iconName;
}

} // namespace

void AstreaIconProvider::loadTileConfig()
{
    if (m_tileConfigLoaded)
        return;
    m_tileConfigLoaded = true;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/AstreaOS/icons");
    m_keepDark = readNameList(dir + QStringLiteral("/keep-dark.conf"));
    m_tileOverrides = readOverrides(dir + QStringLiteral("/overrides.conf"));
}

// Tiles are an Astrea icon theme's feature: its index.theme opts in with
// X-Astrea-AppTiles=true. Any other theme presents artwork as it is.
bool AstreaIconProvider::activeThemeWantsTiles()
{
    const QString theme = QIcon::themeName();
    if (theme.isEmpty())
        return false;
    if (const auto it = m_themeTileFlags.constFind(theme); it != m_themeTileFlags.cend())
        return it.value();
    bool wants = false;
    QStringList roots;
    {
        QMutexLocker lock(&AstreaIconTheme::qIconMutex());
        // A theme the catalog cannot read would leave only its parents in the
        // family, and every app would look foreign: no tiles then.
        if (!m_catalog.themeExists(theme)) {
            m_themeTileFlags.insert(theme, false);
            return false;
        }
        roots = m_catalog.searchRootPaths();
    }
    for (const QString &root : roots) {
        QFile index(QDir(root).filePath(theme + QStringLiteral("/index.theme")));
        if (!index.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        while (!index.atEnd()) {
            const QByteArray line = index.readLine().trimmed();
            if (line.startsWith("X-Astrea-AppTiles=")) {
                wants = line.mid(18).trimmed().toLower() == "true";
                break;
            }
        }
        break;
    }
    m_themeTileFlags.insert(theme, wants);
    return wants;
}

// The best artwork to tile for `iconName`, or empty when the active theme
// draws the icon itself (or nothing has art for it).
QString AstreaIconProvider::tileSourceFor(const QString &iconName)
{
    const QString base = tileBaseName(iconName);
    if (const auto it = m_tileOverrides.constFind(base); it != m_tileOverrides.cend())
        return QFileInfo::exists(it.value()) ? it.value() : QString();
    if (iconName.startsWith(QLatin1Char('/')))
        return QFileInfo::exists(iconName) ? iconName : QString();

    const QString theme = QIcon::themeName();
    {
        QMutexLocker lock(&AstreaIconTheme::qIconMutex());
        // Resolved at master size: the scalable or largest art.
        const auto asset = m_catalog.resolveIconAsset(
            theme, {iconName}, QSize(AstreaTiles::renderer().masterExtent,
                                     AstreaTiles::renderer().masterExtent), 1.0);
        if (!asset.filePath.isEmpty())
            return asset.themeName == theme ? QString() : asset.filePath;
    }
    for (const QString &suffix : {QStringLiteral(".svg"), QStringLiteral(".png"), QStringLiteral(".xpm")}) {
        const QString path = QStringLiteral("/usr/share/pixmaps/") + iconName + suffix;
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

AstreaIconProvider::TileOutcome AstreaIconProvider::resolveTiledIcon(
    const QString &iconName, const IconRenderRequest &request,
    AstreaTiles::Variant variant, QPixmap *result)
{
    const AstreaTiles::Renderer renderer = AstreaTiles::renderer();
    if (!renderer.render || !activeThemeWantsTiles())
        return TileOutcome::NotApplicable;
    loadTileConfig();
    const QString source = tileSourceFor(iconName);
    if (source.isEmpty())
        return TileOutcome::NotApplicable;
    const bool keepDark = m_keepDark.contains(tileBaseName(iconName));
    const QString masterKey = tileMasterKey(source, keepDark, renderer.version);

    {
        QMutexLocker lock(&m_tiles->mutex);
        if (const AstreaTiles::Masters *masters = m_tiles->masters.object(masterKey)) {
            QPixmap pixmap = QPixmap::fromImage(
                AstreaTiles::scaleMaster(masters->of(variant), request.physicalExtent));
            pixmap.setDevicePixelRatio(1.0);
            *result = pixmap;
            return TileOutcome::Ready;
        }
        if (m_tiles->failed.contains(masterKey))
            return TileOutcome::NotApplicable;
        if (m_tiles->pending.contains(masterKey))
            return TileOutcome::Pending;
        m_tiles->pending.insert(masterKey);
    }

    // Read the disk cache or make the tile off the GUI thread; the provider
    // refreshes once the burst of finished tiles lands.
    const std::shared_ptr<TileShared> shared = m_tiles;
    const QPointer<QObject> self(this);
    QThreadPool::globalInstance()->start([shared, self, source, keepDark, masterKey, renderer] {
        const QString dir = tileCacheDirectory();
        const QString lightPath = dir + QLatin1Char('/') + masterKey + QStringLiteral("-light.png");
        const QString darkPath = dir + QLatin1Char('/') + masterKey + QStringLiteral("-dark.png");
        AstreaTiles::Masters masters{QImage(lightPath), QImage(darkPath)};
        if (masters.isNull()) {
            const QImage artwork = AstreaTiles::loadArtwork(source, renderer.masterExtent);
            masters = artwork.isNull() ? AstreaTiles::Masters{} : renderer.render(artwork, keepDark);
            if (!masters.isNull() && QDir().mkpath(dir)) {
                for (const auto &[path, image] : {std::pair{lightPath, masters.light},
                                                  std::pair{darkPath, masters.dark}}) {
                    QSaveFile file(path);
                    if (file.open(QIODevice::WriteOnly) && image.save(&file, "PNG"))
                        file.commit();
                }
            }
        }
        {
            QMutexLocker lock(&shared->mutex);
            shared->pending.remove(masterKey);
            if (masters.isNull()) {
                shared->failed.insert(masterKey);
            } else {
                const int cost = int(masters.light.sizeInBytes() + masters.dark.sizeInBytes());
                shared->masters.insert(masterKey, new AstreaTiles::Masters(masters), cost);
            }
        }
        if (QCoreApplication *app = QCoreApplication::instance()) {
            QMetaObject::invokeMethod(app, [self] {
                if (auto *provider = qobject_cast<AstreaIconProvider *>(self.data()))
                    provider->scheduleTileRefresh();
            }, Qt::QueuedConnection);
        }
    });
    return TileOutcome::Pending;
}


AstreaIconProvider::AstreaIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap), m_cache(kCacheMaxCost)
    , m_tiles(std::make_shared<TileShared>())
{
    // Tiles finish on workers in bursts; one refresh picks up a burst.
    m_tileRefreshTimer.setSingleShot(true);
    m_tileRefreshTimer.setInterval(120);
    QObject::connect(&m_tileRefreshTimer, &QTimer::timeout,
                     [this] { invalidateRenderedIcons(); });
    refreshThemeState();

    QObject::connect(&m_themeWatcher, &QFileSystemWatcher::directoryChanged,
                     [this](const QString &path) { handleWatchedPathChanged(path); });
    QObject::connect(&m_themeWatcher, &QFileSystemWatcher::fileChanged,
                     [this](const QString &path) { handleWatchedPathChanged(path); });
}

void AstreaIconProvider::clearCache() {
    {
        QMutexLocker lock(&AstreaIconTheme::qIconMutex());
        m_catalog.invalidate();
    }
    {
        QWriteLocker lock(&m_cacheLock);
        m_cache.clear();
    }
    {
        QWriteLocker lock(&m_negativeCacheLock);
        m_negativeCache.clear();
    }
    m_themeTileFlags.clear();
    m_tileConfigLoaded = false;
    {
        QMutexLocker lock(&m_tiles->mutex);
        m_tiles->failed.clear();
    }
    m_themeRevision.fetch_add(1, std::memory_order_relaxed);
    emit cacheInvalidated();
}

void AstreaIconProvider::invalidateRenderedIcons()
{
    {
        QWriteLocker lock(&m_cacheLock);
        m_cache.clear();
    }
    {
        QWriteLocker lock(&m_negativeCacheLock);
        m_negativeCache.clear();
    }
    m_themeRevision.fetch_add(1, std::memory_order_relaxed);
    emit cacheInvalidated();
}

void AstreaIconProvider::scheduleTileRefresh()
{
    if (!m_tileRefreshTimer.isActive())
        m_tileRefreshTimer.start();
}

QStringList AstreaIconProvider::watchedConfigFiles() const {
    const QString configHome = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return {
        configHome + QStringLiteral("/qt6ct/qt6ct.conf"),
        QDir::homePath() + QStringLiteral("/.config/AstreaOS/ui/theme.json"),
    };
}

QStringList AstreaIconProvider::watchedConfigDirectories() const {
    QStringList dirs;
    for (const auto &file : watchedConfigFiles()) {
        const QString dir = QFileInfo(file).absolutePath();
        if (!dir.isEmpty() && !dirs.contains(dir))
            dirs.append(dir);
        QDir parent(dir);
        if (parent.cdUp()) {
            const QString parentDir = parent.absolutePath();
            if (!parentDir.isEmpty() && !dirs.contains(parentDir))
                dirs.append(parentDir);
        }
    }
    return dirs;
}

QHash<QString, QByteArray> AstreaIconProvider::currentConfigFingerprints() const
{
    QHash<QString, QByteArray> fingerprints;
    for (const QString &path : watchedConfigFiles()) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            fingerprints.insert(path, QByteArrayLiteral("missing"));
            continue;
        }
        fingerprints.insert(path, QByteArrayLiteral("present:")
                                      + QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256));
    }
    return fingerprints;
}

void AstreaIconProvider::handleWatchedPathChanged(const QString &path)
{
    // AstreaOS/screenshot.json (and other unrelated config files) share an
    // ancestor directory with ui/theme.json. Do not invalidate every Dock icon
    // merely because that directory was touched by an atomic config save.
    if (watchedConfigDirectories().contains(path)
        && currentConfigFingerprints() == m_configFingerprints)
        return;
    if (m_assetWatchPaths.contains(path)) {
        // QIcon's file-backed pixmaps otherwise survive an atomic replacement
        // under the same filename, even after our own cache is cleared.
        QMutexLocker lock(&AstreaIconTheme::qIconMutex());
        QPixmapCache::clear();
    }
    refreshThemeState();
}

void AstreaIconProvider::refreshThemeState() {
    const QStringList files = m_themeWatcher.files();
    if (!files.isEmpty())
        m_themeWatcher.removePaths(files);
    const QStringList directories = m_themeWatcher.directories();
    if (!directories.isEmpty())
        m_themeWatcher.removePaths(directories);

    const QStringList watchDirs = themeSearchDirs();
    m_themeWatcher.addPaths(watchDirs);
    m_themeWatcher.addPaths(watchedConfigDirectories());

    for (const auto &file : watchedConfigFiles()) {
        if (QFileInfo::exists(file))
            m_themeWatcher.addPath(file);
    }

    m_configFingerprints = currentConfigFingerprints();
    AstreaIconTheme::apply();
    {
        QMutexLocker lock(&AstreaIconTheme::qIconMutex());
        m_catalog.invalidate();
        m_assetWatchPaths = m_catalog.watchPaths({QIcon::themeName(), QIcon::fallbackThemeName()});
    }
    if (!m_assetWatchPaths.isEmpty())
        m_themeWatcher.addPaths(m_assetWatchPaths);
    clearCache();
}

QStringList AstreaIconProvider::themeSearchDirs() const {
    return AstreaIconTheme::searchPaths();
}

QPixmap AstreaIconProvider::resolvePlainIcon(const QString &iconName,
                                             const IconRenderRequest &request) {
    QPixmap result;

    if (QFileInfo::exists(iconName))
        result = renderIcon(QIcon(iconName), request);

    if (result.isNull()) {
        QMutexLocker lock(&AstreaIconTheme::qIconMutex());
        QStringList candidates{iconName};
        const QFileInfo iconInfo(iconName);
        if (!iconInfo.suffix().isEmpty())
            candidates.append(iconInfo.completeBaseName());
        const auto asset = m_catalog.resolveIconAsset(QIcon::themeName(), candidates,
            QSize(request.logicalExtent, request.logicalExtent), request.devicePixelRatio);
        QIcon themeIcon = asset.filePath.isEmpty() ? QIcon::fromTheme(iconName) : QIcon(asset.filePath);
        if (themeIcon.isNull() && candidates.size() > 1)
            themeIcon = QIcon::fromTheme(candidates.last());
        result = renderIcon(themeIcon, request);
    }

    if (result.isNull()) {
        QStringList candidates{QFileInfo(iconName).fileName()};
        if (QFileInfo(iconName).suffix().isEmpty()) {
            candidates << iconName + QStringLiteral(".png")
                       << iconName + QStringLiteral(".svg")
                       << iconName + QStringLiteral(".xpm");
        }
        for (const QString &candidate : candidates) {
            const QString path = QDir(QStringLiteral("/usr/share/pixmaps"))
                                     .filePath(candidate);
            if (!QFileInfo::exists(path))
                continue;
            result = renderIcon(QIcon(path), request);
            if (!result.isNull())
                break;
        }
    }

    return result;
}

QPixmap AstreaIconProvider::resolveIcon(const QString &iconName,
                                        const IconRenderRequest &request,
                                        std::optional<AstreaTiles::Variant> tile) {
    const int rev = m_themeRevision.load(std::memory_order_acquire);
    const int dprKey = qRound(request.devicePixelRatio * IconRenderRequest::kDprPrecision);
    const QString tileKey = !tile ? QStringLiteral("none")
        : *tile == AstreaTiles::Variant::Light ? QStringLiteral("light") : QStringLiteral("dark");
    const QString key = QStringLiteral("%1|logical=%2|dpr=%3|physical=%4|revision=%5|tile=%6")
                            .arg(iconName)
                            .arg(request.logicalExtent)
                            .arg(dprKey)
                            .arg(request.physicalExtent)
                            .arg(rev)
                            .arg(tileKey);

    {
        QReadLocker lock(&m_cacheLock);
        if (auto *cached = m_cache.object(key))
            return *cached;
    }

    {
        QReadLocker lock(&m_negativeCacheLock);
        if (m_negativeCache.contains(key))
            return {};
    }

    QPixmap result;
    if (tile) {
        switch (resolveTiledIcon(iconName, request, *tile, &result)) {
        case TileOutcome::Ready:
            break;
        case TileOutcome::Pending:
            // The tile is being made; show the plain icon until the refresh,
            // and keep it out of the cache so the refresh resolves again.
            return resolvePlainIcon(iconName, request);
        case TileOutcome::NotApplicable:
            result = resolvePlainIcon(iconName, request);
            break;
        }
    } else {
        result = resolvePlainIcon(iconName, request);
    }

    if (result.isNull()) {
        QWriteLocker lock(&m_negativeCacheLock);
        if (!m_negativeCache.contains(key)) {
            if (m_negativeCache.size() >= kMaxNegativeEntries) {
                int oldestSeq = std::numeric_limits<int>::max();
                QString oldestKey;
                for (auto it = m_negativeCache.constBegin(); it != m_negativeCache.constEnd(); ++it) {
                    if (it.value() < oldestSeq) {
                        oldestSeq = it.value();
                        oldestKey = it.key();
                    }
                }
                if (!oldestKey.isEmpty())
                    m_negativeCache.remove(oldestKey);
            }
            m_negativeCache.insert(key, m_nextNegSeq++);
        }
    } else {
        QWriteLocker lock(&m_cacheLock);
        if (!m_cache.contains(key)) {
            const qsizetype bytes = result.toImage().sizeInBytes();
            const qsizetype boundedBytes = std::clamp<qsizetype>(
                bytes, 1, static_cast<qsizetype>(kCacheMaxCost));
            m_cache.insert(key, new QPixmap(result), static_cast<int>(boundedBytes));
        }
    }

    return result;
}

QPixmap AstreaIconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) {
    const int requestedExtent = requestedSize.isValid()
        ? qMax(requestedSize.width(), requestedSize.height()) : 80;
    qreal logicalExtent = requestedExtent > 0 ? requestedExtent : 80;
    qreal dpr = 1.0;

    QString iconName;
    QHash<QString, QString> parameters;
    int queryIdx = id.indexOf(QLatin1Char('?'));
    if (queryIdx >= 0) {
        iconName = QUrl::fromPercentEncoding(id.left(queryIdx).toUtf8());
        parameters = queryParameters(id.mid(queryIdx + 1));
    } else {
        iconName = QUrl::fromPercentEncoding(id.toUtf8());
    }

    qreal parsedValue = 0.0;
    if (parseReal(parameters.value(QStringLiteral("size")), &parsedValue))
        logicalExtent = parsedValue;
    if (parseReal(parameters.value(QStringLiteral("logicalSize")), &parsedValue))
        logicalExtent = parsedValue;
    if (parseReal(parameters.value(QStringLiteral("dpr")), &parsedValue))
        dpr = parsedValue;
    std::optional<AstreaTiles::Variant> tile;
    const QString tileParameter = parameters.value(QStringLiteral("tile"));
    if (tileParameter == QStringLiteral("light"))
        tile = AstreaTiles::Variant::Light;
    else if (tileParameter == QStringLiteral("dark"))
        tile = AstreaTiles::Variant::Dark;

    const IconRenderRequest request = IconRenderRequest::fromValues(logicalExtent, dpr)
        .value_or(fallbackRequest(requestedExtent > 0 ? requestedExtent : 80));

    if (iconName.isEmpty()) {
        if (size) *size = QSize(request.physicalExtent, request.physicalExtent);
        return {};
    }

    if (tile && iconName.startsWith(QStringLiteral("file://")))
        iconName = QUrl(iconName).toLocalFile();
    if (tile && iconName.startsWith(QLatin1Char('/'))) {
        QPixmap pm = resolveIcon(iconName, request, tile);
        if (size) *size = pm.isNull()
            ? QSize(request.physicalExtent, request.physicalExtent) : pm.size();
        return pm;
    }

    if (iconName.startsWith(QStringLiteral("file://"))) {
        const QString localFile = QUrl(iconName).toLocalFile();
        QPixmap pm = renderIcon(QIcon(localFile), request);
        if (pm.isNull())
            pm.load(localFile);
        if (!pm.isNull()) {
            pm.setDevicePixelRatio(1.0);
            if (size) *size = pm.size();
            return pm;
        }
    }

    if (iconName.startsWith(QLatin1Char('/'))) {
        QPixmap pm = renderIcon(QIcon(iconName), request);
        if (pm.isNull())
            pm.load(iconName);
        if (!pm.isNull()) {
            pm.setDevicePixelRatio(1.0);
            if (size) *size = pm.size();
            return pm;
        }
    }

    if (iconName.contains(QStringLiteral("://"))) {
        if (size) *size = QSize(request.physicalExtent, request.physicalExtent);
        return {};
    }

    QPixmap result = resolveIcon(iconName, request, tile);
    if (size) *size = result.isNull()
        ? QSize(request.physicalExtent, request.physicalExtent) : result.size();
    return result;
}
