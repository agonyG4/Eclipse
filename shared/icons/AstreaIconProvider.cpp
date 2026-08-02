#include "icons/AstreaIconProvider.hpp"
#include "icons/AstreaIconTheme.hpp"

#include <QIcon>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QSettings>
#include <QDebug>
#include <QSet>
#include <QReadLocker>
#include <QWriteLocker>

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

void AstreaIconProvider::discoverThemeInheritance(
    const QString &themeName, QStringList &result,
    const QStringList &searchDirs, QSet<QString> &visited) const
{
    if (themeName.isEmpty() || visited.contains(themeName)) return;
    visited.insert(themeName);
    result << themeName;

    for (const auto &dir : searchDirs) {
        const QString indexPath = dir + QStringLiteral("/") + themeName + QStringLiteral("/index.theme");
        if (!QFileInfo::exists(indexPath)) continue;

        QSettings index(indexPath, QSettings::IniFormat);
        index.beginGroup(QStringLiteral("Icon Theme"));
        const QStringList inherits = index.value(QStringLiteral("Inherits")).toStringList();
        index.endGroup();

        for (const auto &inh : inherits) {
            const QString trimmed = inh.trimmed();
            if (!trimmed.isEmpty())
                discoverThemeInheritance(trimmed, result, searchDirs, visited);
        }
    }
}

QStringList AstreaIconProvider::iconExtensions() {
    return {QStringLiteral(".png"), QStringLiteral(".svg"),
            QStringLiteral(".xpm"), QStringLiteral(".svgz")};
}

QStringList AstreaIconProvider::iconSubdirs(int size) {
    QStringList dirs;
    dirs << QStringLiteral("scalable")
         << QStringLiteral("symbolic")
         << QStringLiteral("%1x%1").arg(size)
         << QStringLiteral("48x48") << QStringLiteral("64x64")
         << QStringLiteral("32x32") << QStringLiteral("128x128")
         << QStringLiteral("256x256") << QStringLiteral("24x24")
         << QStringLiteral("16x16") << QStringLiteral("22x22")
         << QStringLiteral("192x192") << QStringLiteral("96x96");
    return dirs;
}

QStringList AstreaIconProvider::iconPrefixes() {
    return {QStringLiteral("apps/"), QStringLiteral("categories/"),
            QStringLiteral("places/"), QStringLiteral("devices/"),
            QStringLiteral("status/"), QStringLiteral("mimetypes/"),
            QStringLiteral("emblems/"), QString()};
}

QString AstreaIconProvider::lookupXdgTheme(const QString &iconName, int size) const {
    QStringList searchDirsOrdered = themeSearchDirs();
    for (const auto &d : {QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share/icons"),
                          QStringLiteral("/var/lib/flatpak/exports/share/icons")}) {
        const QString clean = QDir::cleanPath(d);
        if (QDir(clean).exists() && !searchDirsOrdered.contains(clean))
            searchDirsOrdered.append(clean);
    }

    QStringList themes;
    QSet<QString> visited;

    QString activeTheme = AstreaIconTheme::resolve();
    if (!activeTheme.isEmpty() && activeTheme != QStringLiteral("hicolor"))
        discoverThemeInheritance(activeTheme, themes, searchDirsOrdered, visited);

    visited.clear();
    discoverThemeInheritance(QStringLiteral("hicolor"), themes, searchDirsOrdered, visited);

    themes.removeDuplicates();

    QStringList extensions = iconExtensions();
    QStringList subdirs = iconSubdirs(size);
    QStringList prefixes = iconPrefixes();

    QString baseName = iconName;
    for (const auto &ext : extensions) {
        if (baseName.endsWith(ext)) {
            baseName.chop(ext.size());
            break;
        }
    }

    for (const auto &theme : themes) {
        if (theme.isEmpty()) continue;
        for (const auto &d : searchDirsOrdered) {
            QDir themeDir(d + QStringLiteral("/") + theme);
            if (!themeDir.exists()) continue;

            for (const auto &sub : subdirs) {
                for (const auto &prefix : prefixes) {
                    for (const auto &ext : extensions) {
                        QString path = themeDir.absoluteFilePath(sub + QStringLiteral("/") + prefix + baseName + ext);
                        if (QFileInfo::exists(path))
                            return path;
                    }
                    for (const auto &ext : extensions) {
                        if (iconName.endsWith(ext)) {
                            QString path = themeDir.absoluteFilePath(sub + QStringLiteral("/") + prefix + iconName);
                            if (QFileInfo::exists(path))
                                return path;
                        }
                    }
                    if (!prefix.isEmpty()) {
                        for (const auto &ext : extensions) {
                            QString invPath = themeDir.absoluteFilePath(prefix + sub + QStringLiteral("/") + baseName + ext);
                            if (QFileInfo::exists(invPath))
                                return invPath;
                        }
                    }
                }
            }
        }
    }

    for (const auto &ext : extensions) {
        QString path = QStringLiteral("/usr/share/pixmaps/") + baseName + ext;
        if (QFileInfo::exists(path)) return path;
    }
    const QString pixmapPath = QStringLiteral("/usr/share/pixmaps/") + iconName;
    if (QFileInfo::exists(pixmapPath)) return pixmapPath;

    return {};
}

QPixmap AstreaIconProvider::resolveIcon(const QString &iconName, int size) {
    const int rev = m_themeRevision.load(std::memory_order_acquire);
    const QString key = QStringLiteral("%1-%2-%3").arg(iconName).arg(size).arg(rev);

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

    if (QFileInfo::exists(iconName)) {
        result.load(iconName);
    }

    if (result.isNull()) {
        QString found = lookupXdgTheme(iconName, size);
        if (!found.isEmpty()) {
            QPixmap loaded(found);
            if (!loaded.isNull())
                result = loaded.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    if (result.isNull()) {
        QIcon themeIcon = QIcon::fromTheme(iconName);
        if (!themeIcon.isNull()) {
            QPixmap pm = themeIcon.pixmap(size, size);
            if (!pm.isNull())
                result = pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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
            int cost = result.width() * result.height() * 4;
            m_cache.insert(key, new QPixmap(result), cost);
        }
    }

    return result;
}

QPixmap AstreaIconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) {
    int target = requestedSize.isValid() ? qMax(requestedSize.width(), requestedSize.height()) : 80;

    QString iconName;
    int queryIdx = id.indexOf(QLatin1Char('?'));
    if (queryIdx >= 0) {
        iconName = QUrl::fromPercentEncoding(id.left(queryIdx).toUtf8());
        const QString queryStr = id.mid(queryIdx + 1);
        const QStringList parts = queryStr.split(QLatin1Char('&'));
        for (const auto &part : parts) {
            if (part.startsWith(QStringLiteral("size="))) {
                bool ok;
                int parsedSize = part.mid(5).toInt(&ok);
                if (ok && parsedSize > 0) target = parsedSize;
            }
        }
    } else {
        iconName = QUrl::fromPercentEncoding(id.toUtf8());
    }

    if (iconName.isEmpty()) {
        if (size) *size = QSize(target, target);
        return {};
    }

    if (iconName.startsWith(QStringLiteral("file://"))) {
        const QString localFile = QUrl(iconName).toLocalFile();
        QPixmap pm(localFile);
        if (!pm.isNull()) {
            if (size) *size = pm.size();
            return pm;
        }
    }

    if (iconName.startsWith(QLatin1Char('/'))) {
        QPixmap pm(iconName);
        if (!pm.isNull()) {
            if (size) *size = pm.size();
            return pm;
        }
    }

    if (iconName.contains(QStringLiteral("://"))) {
        if (size) *size = QSize(target, target);
        return {};
    }

    QPixmap result = resolveIcon(iconName, target);
    if (size) *size = result.isNull() ? QSize(target, target) : result.size();
    return result;
}
