#include "icons/AstreaIconProvider.hpp"
#include "icons/AstreaIconTheme.hpp"

#include <QIcon>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QReadLocker>
#include <QWriteLocker>

#include <algorithm>
#include <cmath>
#include <limits>

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

    // QIcon can choose a non-exact fixed raster for a density-aware request.
    // Select the closest available source through QIcon's metadata so an exact
    // 96-pixel representation wins over a 128-pixel neighbor at a 96-pixel
    // target, while a smaller-only theme is never upscaled.
    const QList<QSize> available = icon.availableSizes(QIcon::Normal, QIcon::Off);
    QSize selected;
    int selectedDimension = std::numeric_limits<int>::max();
    int largestDimension = 0;
    for (const QSize &candidate : available) {
        const int dimension = qMax(candidate.width(), candidate.height());
        if (dimension <= 0)
            continue;
        largestDimension = qMax(largestDimension, dimension);
        if (dimension >= request.physicalExtent && dimension < selectedDimension) {
            selected = candidate;
            selectedDimension = dimension;
        }
    }
    if (selected.isEmpty() && largestDimension > 0) {
        for (const QSize &candidate : available) {
            if (qMax(candidate.width(), candidate.height()) == largestDimension) {
                selected = candidate;
                break;
            }
        }
    }
    if (!selected.isEmpty()) {
        const QPixmap source = icon.pixmap(selected, 1.0, QIcon::Normal, QIcon::Off);
        if (!source.isNull())
            result = source;
        if (!result.isNull()
            && qMax(selected.width(), selected.height()) > request.physicalExtent) {
            result = result.scaled(request.physicalExtent,
                                   request.physicalExtent,
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
        }
    }

    if (result.isNull())
        return {};

    result.setDevicePixelRatio(1.0);
    return result;
}

IconRenderRequest fallbackRequest(int logicalExtent)
{
    return *IconRenderRequest::fromValues(qMax(1, logicalExtent), 1.0);
}

} // namespace

AstreaIconProvider::AstreaIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap), m_cache(kCacheMaxCost)
{
    refreshThemeState();

    QObject::connect(&m_themeWatcher, &QFileSystemWatcher::directoryChanged,
                     [this]() { refreshThemeState(); });
    QObject::connect(&m_themeWatcher, &QFileSystemWatcher::fileChanged,
                     [this]() { refreshThemeState(); });
}

void AstreaIconProvider::clearCache() {
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

QStringList AstreaIconProvider::watchedConfigFiles() const {
    const QString configHome = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return {
        configHome + QStringLiteral("/qt6ct/qt6ct.conf"),
        configHome + QStringLiteral("/AstreaOS/ui/theme.json"),
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

    AstreaIconTheme::apply();
    clearCache();
}

QStringList AstreaIconProvider::themeSearchDirs() const {
    return AstreaIconTheme::searchPaths();
}

QPixmap AstreaIconProvider::resolveIcon(const QString &iconName,
                                        const IconRenderRequest &request) {
    const int rev = m_themeRevision.load(std::memory_order_acquire);
    const int dprKey = qRound(request.devicePixelRatio * IconRenderRequest::kDprPrecision);
    const QString key = QStringLiteral("%1|logical=%2|dpr=%3|physical=%4|revision=%5")
                            .arg(iconName)
                            .arg(request.logicalExtent)
                            .arg(dprKey)
                            .arg(request.physicalExtent)
                            .arg(rev);

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

    if (QFileInfo::exists(iconName))
        result = renderIcon(QIcon(iconName), request);

    if (result.isNull()) {
        QIcon themeIcon = QIcon::fromTheme(iconName);
        if (themeIcon.isNull()) {
            const QFileInfo iconInfo(iconName);
            if (!iconInfo.suffix().isEmpty())
                themeIcon = QIcon::fromTheme(iconInfo.completeBaseName());
        }
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

    const IconRenderRequest request = IconRenderRequest::fromValues(logicalExtent, dpr)
        .value_or(fallbackRequest(requestedExtent > 0 ? requestedExtent : 80));

    if (iconName.isEmpty()) {
        if (size) *size = QSize(request.physicalExtent, request.physicalExtent);
        return {};
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

    QPixmap result = resolveIcon(iconName, request);
    if (size) *size = result.isNull()
        ? QSize(request.physicalExtent, request.physicalExtent) : result.size();
    return result;
}
