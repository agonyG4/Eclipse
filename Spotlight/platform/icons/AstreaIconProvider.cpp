#include "platform/icons/AstreaIconProvider.hpp"
#include "platform/icons/AstreaIconTheme.hpp"

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
#include <QStandardPaths>

AstreaIconProvider::AstreaIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap), m_cache(256) {
    refreshThemeState();

    QObject::connect(&m_themeWatcher, &QFileSystemWatcher::directoryChanged,
                     [this]() { refreshThemeState(); });
    QObject::connect(&m_themeWatcher, &QFileSystemWatcher::fileChanged,
                     [this]() { refreshThemeState(); });
}

void AstreaIconProvider::clearCache() {
    QWriteLocker lock(&m_cacheLock);
    m_cache.clear();
    ++m_themeRevision;
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
    m_themeWatcher.removePaths(m_themeWatcher.files());
    m_themeWatcher.removePaths(m_themeWatcher.directories());

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
    const QStringList &searchDirs, QSet<QString> &visited) const {
    if (themeName.isEmpty() || visited.contains(themeName)) return;
    visited.insert(themeName);
    result << themeName;

    for (const auto &dir : searchDirs) {
        QString indexPath = dir + QStringLiteral("/") + themeName + QStringLiteral("/index.theme");
        if (!QFileInfo::exists(indexPath)) continue;

        QSettings index(indexPath, QSettings::IniFormat);
        index.beginGroup(QStringLiteral("Icon Theme"));
        QStringList inherits = index.value(QStringLiteral("Inherits")).toStringList();
        index.endGroup();

        for (const auto &inh : inherits) {
            QString trimmed = inh.trimmed();
            if (!trimmed.isEmpty())
                discoverThemeInheritance(trimmed, result, searchDirs, visited);
        }
        break;
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
    // Add flatpak icon export dirs
    for (const auto &d : {QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share/icons"),
                          QStringLiteral("/var/lib/flatpak/exports/share/icons")}) {
        const QString clean = QDir::cleanPath(d);
        if (QDir(clean).exists() && !searchDirsOrdered.contains(clean))
            searchDirsOrdered.append(clean);
    }

    // Build theme list: active theme + inheritance chain, then hicolor
    QStringList themes;
    QSet<QString> visited;

    // Use AstreaIconTheme::resolve() — this is the authoritative theme source.
    // Do NOT rely on QIcon::themeName() because the platform theme plugin may
    // override it after AstreaIconTheme::apply() runs (Qt 6 platform themes
    // can reset the icon theme during their own initialization).
    QString activeTheme = AstreaIconTheme::resolve();
    if (!activeTheme.isEmpty() && activeTheme != QStringLiteral("hicolor"))
        discoverThemeInheritance(activeTheme, themes, searchDirsOrdered, visited);

    // hicolor is always included as the last resort in the chain
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

    // Search only the themes in the chain (no unrelated themes)
    for (const auto &theme : themes) {
        if (theme.isEmpty()) continue;
        for (const auto &d : searchDirsOrdered) {
            QDir themeDir(d + QStringLiteral("/") + theme);
            if (!themeDir.exists()) continue;

            for (const auto &sub : subdirs) {
                for (const auto &prefix : prefixes) {
                    for (const auto &ext : extensions) {
                        // Traditional layout: theme/<size>/<context>/base.ext
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
                    // Inverted layout (WhiteSur style): theme/<context>/<size>/base.ext
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

    // /usr/share/pixmaps fallback
    for (const auto &ext : extensions) {
        QString path = QStringLiteral("/usr/share/pixmaps/") + baseName + ext;
        if (QFileInfo::exists(path)) return path;
    }
    QString pixmapPath = QStringLiteral("/usr/share/pixmaps/") + iconName;
    if (QFileInfo::exists(pixmapPath)) return pixmapPath;

    return {};
}

QPixmap AstreaIconProvider::resolveIcon(const QString &iconName, int size) {
    QString key = QStringLiteral("%1-%2-%3").arg(iconName).arg(size).arg(m_themeRevision);

    {
        QReadLocker lock(&m_cacheLock);
        if (auto *cached = m_cache.object(key))
            return *cached;
    }

    QPixmap result;

    // 1. Direct file path or URL
    if (QFileInfo::exists(iconName)) {
        if (result.load(iconName))
            goto insert_cache;
    }

    // 2. Manual XDG resolution using the authoritative resolved theme.
    //    This runs before QIcon::fromTheme() because Qt 6 platform themes
    //    may override QIcon::themeName() after AstreaIconTheme::apply()
    //    runs, causing fromTheme() to look in the wrong theme.
    {
        QString found = lookupXdgTheme(iconName, size);
        if (!found.isEmpty()) {
            QPixmap loaded(found);
            if (!loaded.isNull()) {
                result = loaded.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                goto insert_cache;
            }
        }
    }

    // 3. QIcon::fromTheme fallback (uses Qt's internal icon engine)
    {
        QIcon themeIcon = QIcon::fromTheme(iconName);
        if (!themeIcon.isNull()) {
            QPixmap pm = themeIcon.pixmap(size, size);
            if (!pm.isNull()) {
                result = pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                goto insert_cache;
            }
        }
    }

insert_cache:
    {
        QWriteLocker lock(&m_cacheLock);
        if (!m_cache.contains(key))
            m_cache.insert(key, new QPixmap(result));
    }
    return result;
}

QPixmap AstreaIconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) {
    int target = requestedSize.isValid() ? qMax(requestedSize.width(), requestedSize.height()) : 80;

    QString iconName;
    int queryIdx = id.indexOf(QLatin1Char('?'));
    if (queryIdx >= 0) {
        iconName = QUrl::fromPercentEncoding(id.left(queryIdx).toUtf8());
        QString queryStr = id.mid(queryIdx + 1);
        QStringList parts = queryStr.split(QLatin1Char('&'));
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

    // Handle file:// URLs
    if (iconName.startsWith(QStringLiteral("file://"))) {
        QString localFile = QUrl(iconName).toLocalFile();
        QPixmap pm(localFile);
        if (!pm.isNull()) {
            if (size) *size = pm.size();
            return pm;
        }
    }

    // Handle absolute paths
    if (iconName.startsWith(QLatin1Char('/'))) {
        QPixmap pm(iconName);
        if (!pm.isNull()) {
            if (size) *size = pm.size();
            return pm;
        }
    }

    // Non-file URL -> let provider handle it
    if (iconName.contains(QStringLiteral("://"))) {
        if (size) *size = QSize(target, target);
        return {};
    }

    QPixmap result = resolveIcon(iconName, target);
    if (size) *size = result.isNull() ? QSize(target, target) : result.size();
    return result;
}
