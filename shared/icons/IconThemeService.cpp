#include <AstreaSDK/IconThemeService.hpp>

#include "IconDirectoryPolicy.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QtMath>
#include <QUrl>

#include <algorithm>

namespace AstreaSDK {

namespace {

constexpr int kMaxIconSize = 512;

QStringList specialDirectoryCandidates(const QString &path)
{
    const QString normalized = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QList<QPair<QStandardPaths::StandardLocation, QString>> locations {
        {QStandardPaths::DesktopLocation, QStringLiteral("user-desktop")},
        {QStandardPaths::DownloadLocation, QStringLiteral("folder-download")},
        {QStandardPaths::DocumentsLocation, QStringLiteral("folder-documents")},
        {QStandardPaths::PicturesLocation, QStringLiteral("folder-pictures")},
        {QStandardPaths::MusicLocation, QStringLiteral("folder-music")},
        {QStandardPaths::MoviesLocation, QStringLiteral("folder-videos")},
        {QStandardPaths::TemplatesLocation, QStringLiteral("folder-templates")},
        {QStandardPaths::PublicShareLocation, QStringLiteral("folder-publicshare")},
    };

    for (const auto &[location, iconName] : locations) {
        for (const QString &candidate : QStandardPaths::standardLocations(location)) {
            if (!candidate.isEmpty() && QDir::cleanPath(QFileInfo(candidate).absoluteFilePath()) == normalized) {
                return {iconName};
            }
        }
    }

    if (normalized == QDir::cleanPath(QDir::homePath())) {
        return {QStringLiteral("user-home")};
    }

    const QString trashPath = QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation))
        .filePath(QStringLiteral(".local/share/Trash/files"));
    if (normalized == QDir::cleanPath(QFileInfo(trashPath).absoluteFilePath())) {
        return {QStringLiteral("user-trash")};
    }

    return {};
}

QString fileNameForMimeLookup(const QString &path)
{
    const QUrl url(path);
    if (url.isValid() && !url.scheme().isEmpty()) {
        return QFileInfo(url.path()).fileName();
    }
    return QFileInfo(path).fileName();
}

QSize boundedPixelSize(const QSize &logicalSize, qreal devicePixelRatio)
{
    const qreal dpr = std::clamp(devicePixelRatio, 0.5, 4.0);
    const int width = std::clamp(qCeil(std::max(1, logicalSize.width()) * dpr), 1, kMaxIconSize * 4);
    const int height = std::clamp(qCeil(std::max(1, logicalSize.height()) * dpr), 1, kMaxIconSize * 4);
    return {width, height};
}

bool isSymbolicName(const QString &name)
{
    return name.endsWith(QStringLiteral("-symbolic"));
}

QString symbolicBaseName(const QString &name)
{
    return isSymbolicName(name)
        ? name.left(name.size() - QStringLiteral("-symbolic").size())
        : name;
}

QJsonObject readThemeConfigObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject();
}

} // namespace

IconThemeService::IconThemeService(QObject *parent)
    : IconThemeService(QString(), parent)
{
}

IconThemeService::IconThemeService(const QString &configPath, QObject *parent)
    : QObject(parent)
    , m_configPath(configPath.trimmed().isEmpty() ? canonicalConfigPath() : QFileInfo(configPath).absoluteFilePath())
    , m_watcher(new QFileSystemWatcher(this))
    , m_reloadTimer(new QTimer(this))
    , m_platformTheme(QIcon::themeName())
{
    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(60);
    connect(m_reloadTimer, &QTimer::timeout, this, &IconThemeService::reloadConfig);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &IconThemeService::scheduleReload);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &IconThemeService::scheduleReload);
    reloadConfig();
}

QString IconThemeService::effectiveTheme() const
{
    return m_effectiveTheme;
}

QString IconThemeService::configuredBaseTheme() const
{
    const QJsonObject config = readThemeConfigObject(m_configPath);
    QString configured = config.value(QStringLiteral("desktop_icon_theme")).toString().trimmed();
    if (configured.isEmpty())
        configured = config.value(QStringLiteral("system_icon_theme")).toString().trimmed();
    return isValidThemeIdentifier(configured) ? configured : QString();
}

IconThemeService::AppearanceMode IconThemeService::appearance() const
{
    const QJsonObject object = readThemeConfigObject(m_configPath);
    if (object.value(QStringLiteral("theme")).toString().trimmed().compare(
            QStringLiteral("light"), Qt::CaseInsensitive) == 0
        || object.value(QStringLiteral("theme_mode")).toInt() == 1) {
        return AppearanceMode::Light;
    }
    return AppearanceMode::Dark;
}

quint64 IconThemeService::revision() const
{
    return m_revision;
}

bool IconThemeService::isValidThemeIdentifier(const QString &themeName)
{
    static const QRegularExpression expression(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._+-]{0,127}$"));
    return expression.match(themeName).hasMatch()
        && !themeName.contains(QStringLiteral(".."));
}

QStringList IconThemeService::iconCandidatesForFile(
    const QString &path,
    bool isDirectory,
    bool isExecutable) const
{
    QStringList candidates;
    if (isDirectory) {
        for (const QString &name : specialDirectoryCandidates(path)) {
            appendUnique(candidates, name);
        }
        appendUnique(candidates, QStringLiteral("folder"));
        appendUnique(candidates, QStringLiteral("inode-directory"));
        return candidates;
    }

    if (isExecutable) {
        appendUnique(candidates, QStringLiteral("application-x-executable"));
    }

    const QString fileName = fileNameForMimeLookup(path);
    const QMimeType mimeType = QMimeDatabase().mimeTypeForFile(
        fileName.isEmpty() ? path : fileName,
        QMimeDatabase::MatchExtension);
    if (mimeType.isValid()) {
        appendUnique(candidates, mimeType.iconName());
        appendUnique(candidates, mimeType.genericIconName());
    }
    for (const QString &name : fallbackCandidates()) {
        appendUnique(candidates, name);
    }
    return candidates;
}

QStringList IconThemeService::iconCandidatesForNames(const QStringList &names) const
{
    QStringList candidates;
    for (const QString &name : names) {
        appendUnique(candidates, name.trimmed());
    }
    for (const QString &name : fallbackCandidates()) {
        appendUnique(candidates, name);
    }
    return candidates;
}

QStringList IconThemeService::symbolicCandidatesForNames(const QStringList &names) const
{
    QStringList candidates;
    bool folderSemantic = false;
    bool removableDriveSemantic = false;
    bool hardDiskSemantic = false;
    bool networkSemantic = false;

    for (const QString &rawName : names) {
        const QString name = rawName.trimmed();
        if (name.isEmpty()) {
            continue;
        }

        const QString baseName = symbolicBaseName(name);
        folderSemantic = folderSemantic
            || baseName == QStringLiteral("inode-directory")
            || baseName.startsWith(QStringLiteral("folder"));
        removableDriveSemantic = removableDriveSemantic
            || baseName == QStringLiteral("drive-removable-media");
        hardDiskSemantic = hardDiskSemantic
            || baseName == QStringLiteral("drive-harddisk");
        networkSemantic = networkSemantic
            || baseName == QStringLiteral("network-workgroup");

        if (isSymbolicName(name)) {
            appendUnique(candidates, name);
            continue;
        }

        if (name == QStringLiteral("inode-directory")) {
            appendUnique(candidates, QStringLiteral("folder-symbolic"));
        } else if (name == QStringLiteral("folder-home")) {
            appendUnique(candidates, QStringLiteral("user-home-symbolic"));
        } else if (name == QStringLiteral("folder-desktop")) {
            appendUnique(candidates, QStringLiteral("user-desktop-symbolic"));
        } else if (name == QStringLiteral("folder-downloads")) {
            appendUnique(candidates, QStringLiteral("folder-download-symbolic"));
            appendUnique(candidates, QStringLiteral("folder-downloads-symbolic"));
        } else {
            appendUnique(candidates, name + QStringLiteral("-symbolic"));
        }
    }

    if (removableDriveSemantic) {
        appendUnique(candidates, QStringLiteral("drive-removable-media-symbolic"));
        appendUnique(candidates, QStringLiteral("drive-harddisk-symbolic"));
    } else if (hardDiskSemantic) {
        appendUnique(candidates, QStringLiteral("drive-harddisk-symbolic"));
    }
    if (networkSemantic) {
        appendUnique(candidates, QStringLiteral("network-workgroup-symbolic"));
    }
    if (folderSemantic) {
        appendUnique(candidates, QStringLiteral("folder-symbolic"));
    }
    appendUnique(candidates, QStringLiteral("image-missing-symbolic"));
    return candidates;
}

QIcon IconThemeService::resolveIcon(const QStringList &candidates) const
{
    if (!m_effectiveTheme.isEmpty()) {
        const auto asset = m_catalog.resolveIconAsset(
            m_effectiveTheme,
            candidates,
            QSize(32, 32),
            1.0);
        if (!asset.filePath.isEmpty()) {
            return QIcon(asset.filePath);
        }
    }

    for (const QString &candidate : candidates) {
        if (candidate.isEmpty()) {
            continue;
        }
        const QIcon icon = QIcon::fromTheme(candidate);
        if (!icon.isNull()) {
            return icon;
        }
    }
    return {};
}

QImage IconThemeService::renderIcon(
    const QStringList &candidates,
    const QSize &logicalSize,
    qreal devicePixelRatio) const
{
    const qreal boundedDpr = std::clamp(devicePixelRatio, 0.5, 4.0);
    const QSize pixelSize = boundedPixelSize(logicalSize, boundedDpr);
    const QStringList normalizedCandidates = candidates.isEmpty()
        ? fallbackCandidates()
        : candidates;
    const bool symbolic = !normalizedCandidates.isEmpty()
        && std::all_of(normalizedCandidates.cbegin(), normalizedCandidates.cend(), isSymbolicName);
    const QString key = cacheKey(normalizedCandidates, pixelSize, boundedDpr);
    if (const QImage *cached = m_renderedCache.object(key))
        return *cached;

    QImage result(pixelSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    const QSize logicalRenderSize(
        std::max(1, qRound(pixelSize.width() / boundedDpr)),
        std::max(1, qRound(pixelSize.height() / boundedDpr)));
    const QIcon icon = resolveIconForRender(
        normalizedCandidates,
        logicalRenderSize,
        boundedDpr);
    bool renderedThemeIcon = false;
    if (!icon.isNull()) {
        // Selection uses logical size and DPR; rasterization uses the exact
        // physical target, avoiding a second fractional-DPR rounding by QIcon.
        const QPixmap pixmap = icon.pixmap(
            pixelSize,
            1.0,
            QIcon::Normal,
            QIcon::Off);
        if (!pixmap.isNull()) {
            renderedThemeIcon = true;
            QImage rendered = pixmap.toImage();
            rendered.setDevicePixelRatio(1.0);
            QPainter painter(&result);
            painter.drawImage(
                (pixelSize.width() - rendered.width()) / 2,
                (pixelSize.height() - rendered.height()) / 2,
                rendered);
        }
    }

    if (!renderedThemeIcon) {
        result = builtInFallback(pixelSize, boundedDpr, symbolic);
    }
    result.setDevicePixelRatio(boundedDpr);

    if (result.sizeInBytes() <= m_renderedCache.maxCost())
        m_renderedCache.insert(key, new QImage(result), int(result.sizeInBytes()));
    return result;
}

QString IconThemeService::iconSourceForNames(
    const QStringList &names,
    int size,
    qreal devicePixelRatio) const
{
    const QStringList candidates = iconCandidatesForNames(names);
    const QByteArray encoded = QUrl::toPercentEncoding(candidates.join(QLatin1Char('|')));
    const qreal boundedDpr = std::clamp(devicePixelRatio, 0.5, 4.0);
    return QStringLiteral("image://astrea-icons/theme/")
        + QString::fromLatin1(encoded)
        + QStringLiteral("?revision=")
        + QString::number(m_revision)
        + QStringLiteral("&size=")
        + QString::number(std::clamp(size, 1, kMaxIconSize))
        + QStringLiteral("&dpr=")
        + QString::number(boundedDpr, 'g', 3);
}

QString IconThemeService::symbolicIconSourceForNames(
    const QStringList &names,
    int size,
    qreal devicePixelRatio) const
{
    const QByteArray encoded = QUrl::toPercentEncoding(
        symbolicCandidatesForNames(names).join(QLatin1Char('|')));
    const qreal boundedDpr = std::clamp(devicePixelRatio, 0.5, 4.0);
    return QStringLiteral("image://astrea-icons/theme/")
        + QString::fromLatin1(encoded)
        + QStringLiteral("?revision=")
        + QString::number(m_revision)
        + QStringLiteral("&size=")
        + QString::number(std::clamp(size, 1, kMaxIconSize))
        + QStringLiteral("&dpr=")
        + QString::number(boundedDpr, 'g', 3);
}

QString IconThemeService::emblemIconSource(
    const QString &name,
    int size,
    qreal devicePixelRatio) const
{
    const QString normalizedName = name.trimmed();
    if (normalizedName.isEmpty()) {
        return {};
    }

    const bool prefixed = normalizedName.startsWith(QStringLiteral("emblem-"));
    const QString keyword = prefixed
        ? normalizedName.mid(QStringLiteral("emblem-").size())
        : normalizedName;
    const QString emblemName = prefixed
        ? normalizedName
        : QStringLiteral("emblem-") + normalizedName;
    QStringList candidates;
    appendUnique(candidates, emblemName);
    appendUnique(candidates, prefixed ? keyword : normalizedName);
    if (!isSymbolicName(emblemName)) {
        appendUnique(candidates, emblemName + QStringLiteral("-symbolic"));
    }
    if (!isSymbolicName(keyword)) {
        appendUnique(candidates, keyword + QStringLiteral("-symbolic"));
    }
    if (candidates.isEmpty() || resolveIcon(candidates).isNull()) {
        return {};
    }

    const QByteArray encoded = QUrl::toPercentEncoding(candidates.join(QLatin1Char('|')));
    const qreal boundedDpr = std::clamp(devicePixelRatio, 0.5, 4.0);
    return QStringLiteral("image://astrea-icons/theme/")
        + QString::fromLatin1(encoded)
        + QStringLiteral("?revision=")
        + QString::number(m_revision)
        + QStringLiteral("&size=")
        + QString::number(std::clamp(size, 1, kMaxIconSize))
        + QStringLiteral("&dpr=")
        + QString::number(boundedDpr, 'g', 3);
}

QString IconThemeService::fileIconSource(
    const QString &path,
    bool isDirectory,
    bool isExecutable,
    int size,
    const QString &semanticIconName,
    qreal devicePixelRatio) const
{
    return richFileIconSource(
        path,
        isDirectory,
        isExecutable,
        size,
        semanticIconName,
        {},
        {},
        {},
        devicePixelRatio);
}

QString IconThemeService::richFileIconSource(
    const QString &path,
    bool isDirectory,
    bool isExecutable,
    int size,
    const QString &semanticIconName,
    const QStringList &iconNames,
    const QUrl &iconFileUrl,
    const QString &iconFileVersion,
    qreal devicePixelRatio) const
{
    QStringList names;
    if (!semanticIconName.trimmed().isEmpty()) {
        names.append(semanticIconName.trimmed());
    }
    names.append(iconNames);
    names.append(iconCandidatesForFile(path, isDirectory, isExecutable));

    const QStringList fallbackNames = iconCandidatesForNames(names);
    const bool hasSemanticOverride = !semanticIconName.trimmed().isEmpty();
    if (!hasSemanticOverride && iconFileUrl.isValid() && iconFileUrl.isLocalFile()) {
        const QFileInfo iconFile(iconFileUrl.toLocalFile());
        if (iconFile.isFile()) {
            const QString version = iconFileVersion.trimmed().isEmpty()
                ? QStringLiteral("0")
                : iconFileVersion.trimmed();
            const QByteArray encodedUrl = QUrl::toPercentEncoding(
                iconFileUrl.toString(QUrl::FullyEncoded));
            const QByteArray encodedFallback = QUrl::toPercentEncoding(
                fallbackNames.join(QLatin1Char('|')));
            return QStringLiteral("image://astrea-icons/file/")
                + QString::fromLatin1(encodedUrl)
                + QStringLiteral("?revision=")
                + QString::number(m_revision)
                + QStringLiteral("&version=")
                + QString::fromLatin1(QUrl::toPercentEncoding(version))
                + QStringLiteral("&fallback=")
                + QString::fromLatin1(encodedFallback)
                + QStringLiteral("&size=")
                + QString::number(std::clamp(size, 1, kMaxIconSize))
                + QStringLiteral("&dpr=")
                + QString::number(std::clamp(devicePixelRatio, 0.5, 4.0), 'g', 3);
        }
    }

    return iconSourceForNames(names, size, devicePixelRatio);
}

void IconThemeService::reloadConfig()
{
    const bool themeAssetsChanged = m_themeAssetsChanged;
    const bool themeTopologyEvent = m_themeTopologyChanged;
    const quint64 previousRevision = m_revision;
    const QString previousTopology = themeTopologySignature();
    m_catalog.invalidate();
    applyTheme(selectTheme());
    const bool topologyChanged = themeTopologyEvent
        && previousTopology != themeTopologySignature();
    if (themeAssetsChanged) {
        const QString currentTheme = QIcon::themeName();
        QIcon::setThemeName(QStringLiteral("__astrea_icon_theme_refresh__"));
        QIcon::setThemeName(currentTheme);
        // Qt's public theme-key invalidation does not evict pixmaps for a
        // replaced file path; asset changes are rare, so clear that shared
        // cache before the next QIcon render.
        QPixmapCache::clear();
    }
    if ((themeAssetsChanged || topologyChanged) && m_revision == previousRevision) {
        ++m_revision;
        clearRenderedCache();
        emit themeChanged();
    }
    m_themeAssetsChanged = false;
    m_themeTopologyChanged = false;
    updateWatcher();
}

void IconThemeService::scheduleReload(const QString &path)
{
    const QString normalizedPath = QFileInfo(path).absoluteFilePath();
    const QString configParent = QFileInfo(m_configPath).absolutePath();
    if (normalizedPath != QFileInfo(m_configPath).absoluteFilePath()
        && normalizedPath != configParent
        && m_themeWatchPaths.contains(normalizedPath)) {
        m_themeAssetsChanged = true;
    }
    if (m_themeRootWatchPaths.contains(normalizedPath)) {
        m_themeTopologyChanged = true;
    }
    if (!m_reloadTimer->isActive()) {
        m_reloadTimer->start();
    }
}

QString IconThemeService::canonicalConfigPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
        .filePath(QStringLiteral("AstreaOS/ui/theme.json"));
}

void IconThemeService::appendUnique(QStringList &names, const QString &name)
{
    const QString normalized = name.trimmed();
    if (!normalized.isEmpty() && !names.contains(normalized)) {
        names.append(normalized);
    }
}

QStringList IconThemeService::fallbackCandidates()
{
    return {
        QStringLiteral("application-x-generic"),
        QStringLiteral("text-x-generic"),
        QStringLiteral("unknown"),
    };
}

bool IconThemeService::themeIsUsable(const QString &themeName) const
{
    return m_catalog.themeExists(themeName);
}

QString IconThemeService::resolveAppearanceVariant(
    const QString &baseTheme,
    AppearanceMode appearanceMode) const
{
    const QString normalized = baseTheme.trimmed();
    if (!isValidThemeIdentifier(normalized)) {
        return {};
    }

    struct Lookup {
        const IconThemeService &service;
        const QString &base;
        QString chosen;
    } lookup{*this, normalized, {}};
    const auto available = [](void *opaque, std::uint8_t variant) {
        auto &state = *static_cast<Lookup *>(opaque);
        QString candidate = state.base;
        if (variant == AstreaIconLight)
            candidate += QStringLiteral("-light");
        else if (variant == AstreaIconDark)
            candidate += QStringLiteral("-dark");
        if (!state.service.themeIsUsable(candidate))
            return false;
        state.chosen = std::move(candidate);
        return true;
    };
    const auto name = normalized.toUtf8();
    return astrea_icon_choose_variant(name.constData(), std::size_t(name.size()),
                                     appearanceMode == AppearanceMode::Light, available, &lookup)
        ? lookup.chosen : QString();
}

IconThemeService::ThemeSelection IconThemeService::selectTheme() const
{
    struct Lookup {
        const IconThemeService &service;
        QString chosen;
        QString compatibility;
    } lookup{*this, {}, {}};
    const auto available = [](void *opaque, std::uint8_t source) {
        auto &state = *static_cast<Lookup *>(opaque);
        const auto &service = state.service;
        switch (source) {
        case AstreaIconThemeEnvironment: {
            const auto theme = qEnvironmentVariable("ASTREA_ICON_THEME").trimmed();
            if (!isValidThemeIdentifier(theme) || !service.themeIsUsable(theme))
                return false;
            state.chosen = theme;
            return true;
        }
        case AstreaIconThemeConfig:
            state.chosen = service.resolveAppearanceVariant(service.configuredBaseTheme(),
                                                            service.appearance());
            return !state.chosen.isEmpty();
        case AstreaIconThemeCompatibility:
            state.compatibility = service.resolveAppearanceVariant(QStringLiteral("MacTahoe"),
                                                                   service.appearance());
            return !state.compatibility.isEmpty();
        case AstreaIconThemePlatform:
            if (!service.themeIsUsable(service.m_platformTheme))
                return false;
            state.chosen = service.m_platformTheme;
            return true;
        default:
            return false;
        }
    };
    switch (astrea_icon_choose_theme(
        m_platformTheme.compare(QStringLiteral("hicolor"), Qt::CaseInsensitive) == 0,
        available, &lookup)) {
    case AstreaIconThemeEnvironment:
        return {lookup.chosen, QStringLiteral("environment")};
    case AstreaIconThemeConfig:
        return {lookup.chosen, QStringLiteral("config")};
    case AstreaIconThemeCompatibility:
        return {lookup.compatibility, QStringLiteral("compatibility-default")};
    case AstreaIconThemePlatform:
        return {lookup.chosen, QStringLiteral("platform")};
    default:
        return {{}, QStringLiteral("qt-freedesktop-fallback")};
    }
}

void IconThemeService::applyTheme(const ThemeSelection &selection)
{
    const AppearanceMode currentAppearance = appearance();
    const bool changed = selection.name != m_effectiveTheme
        || !m_hasAppliedAppearance
        || currentAppearance != m_appliedAppearance;
    m_appliedAppearance = currentAppearance;
    m_hasAppliedAppearance = true;
    if (!changed) {
        m_themeSource = selection.source;
        return;
    }

    m_effectiveTheme = selection.name;
    m_themeSource = selection.source;
    if (!selection.name.isEmpty()) {
        QIcon::setThemeName(selection.name);
    } else if (!m_platformTheme.isEmpty()) {
        QIcon::setThemeName(m_platformTheme);
    }
    ++m_revision;
    clearRenderedCache();
    const QString appearanceName = appearance() == AppearanceMode::Light
        ? QStringLiteral("light")
        : QStringLiteral("dark");
    qInfo().noquote() << "Astrea icon theme:"
                      << "configured=" << (configuredBaseTheme().isEmpty()
                              ? QStringLiteral("<none>")
                              : configuredBaseTheme())
                      << "appearance=" << appearanceName
                      << "effective=" << (m_effectiveTheme.isEmpty()
                              ? QStringLiteral("Qt/Freedesktop fallback")
                              : m_effectiveTheme)
                      << "source=" << m_themeSource;
    emit themeChanged();
}

void IconThemeService::updateWatcher()
{
    const QString parentPath = QFileInfo(m_configPath).absolutePath();
    for (const QString &path : m_watcher->files()) {
        m_watcher->removePath(path);
    }
    for (const QString &path : m_watcher->directories()) {
        m_watcher->removePath(path);
    }
    if (QFileInfo(m_configPath).isFile()) {
        m_watcher->addPath(m_configPath);
    }
    if (QFileInfo(parentPath).isDir()) {
        m_watcher->addPath(parentPath);
    }

    m_themeRootWatchPaths = m_catalog.searchRootPaths();
    for (const QString &path : m_themeRootWatchPaths) {
        m_watcher->addPath(path);
    }

    m_themeWatchPaths = m_catalog.watchPaths(themeWatchNames());
    for (const QString &path : m_themeWatchPaths) {
        m_watcher->addPath(path);
    }
}

QString IconThemeService::themeTopologySignature() const
{
    QStringList signature;
    const QStringList names = themeWatchNames();
    for (const QString &name : names) {
        if (!isValidThemeIdentifier(name)) {
            continue;
        }
        signature.append(name);
        signature.append(m_catalog.themeExists(name) ? QStringLiteral("present")
                                                      : QStringLiteral("missing"));
        signature.append(m_catalog.themeFamily(name).join(QLatin1Char(',')));
    }
    return signature.join(QLatin1Char('|'));
}

QStringList IconThemeService::themeWatchNames() const
{
    QStringList names {
        m_effectiveTheme,
        configuredBaseTheme(),
        qEnvironmentVariable("ASTREA_ICON_THEME").trimmed(),
        m_platformTheme,
        QIcon::fallbackThemeName(),
        QStringLiteral("MacTahoe"),
    };
    const QString configured = configuredBaseTheme();
    if (isValidThemeIdentifier(configured)) {
        names.append(
            configured + (appearance() == AppearanceMode::Light
                    ? QStringLiteral("-light")
                    : QStringLiteral("-dark")));
    }
    names.append(
        QStringLiteral("MacTahoe")
        + (appearance() == AppearanceMode::Light
                ? QStringLiteral("-light")
                : QStringLiteral("-dark")));
    return names;
}

QIcon IconThemeService::resolveIconForRender(
    const QStringList &candidates,
    const QSize &logicalSize,
    qreal devicePixelRatio) const
{
    if (!m_effectiveTheme.isEmpty()) {
        const auto asset = m_catalog.resolveIconAsset(
            m_effectiveTheme,
            candidates,
            logicalSize,
            devicePixelRatio);
        if (!asset.filePath.isEmpty()) {
            // The catalog has already selected the exact Freedesktop asset.
            // Loading this path avoids a second Qt theme hierarchy lookup.
            return QIcon(asset.filePath);
        }
    }

    for (const QString &candidate : candidates) {
        if (candidate.isEmpty()) {
            continue;
        }
        const QIcon icon = QIcon::fromTheme(candidate);
        if (!icon.isNull()) {
            return icon;
        }
    }
    return {};
}

void IconThemeService::clearRenderedCache() const
{
    m_renderedCache.clear();
}

QImage IconThemeService::builtInFallback(
    const QSize &pixelSize,
    qreal devicePixelRatio,
    bool symbolic) const
{
    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bounds = QRectF(image.rect()).adjusted(
        pixelSize.width() * 0.18,
        pixelSize.height() * 0.18,
        -pixelSize.width() * 0.18,
        -pixelSize.height() * 0.18);
    if (symbolic) {
        painter.setPen(QPen(Qt::white, std::max(1.0, pixelSize.width() * 0.10)));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(bounds, pixelSize.width() * 0.18, pixelSize.height() * 0.18);
        painter.drawLine(
            QPointF(bounds.left() + bounds.width() * 0.30, bounds.center().y()),
            QPointF(bounds.right() - bounds.width() * 0.30, bounds.center().y()));
        image.setDevicePixelRatio(devicePixelRatio);
        return image;
    }

    const QColor accent(QStringLiteral("#6f8cff"));
    const QColor shadow(QStringLiteral("#26324f"));
    const QRectF fullColorBounds = QRectF(image.rect()).adjusted(
        pixelSize.width() * 0.12,
        pixelSize.height() * 0.12,
        -pixelSize.width() * 0.12,
        -pixelSize.height() * 0.12);
    painter.setPen(QPen(shadow, std::max(1.0, pixelSize.width() * 0.06)));
    painter.setBrush(accent);
    if (pixelSize.width() >= pixelSize.height()) {
        QPainterPath folder;
        folder.moveTo(fullColorBounds.left(), fullColorBounds.top() + fullColorBounds.height() * 0.2);
        folder.lineTo(fullColorBounds.left() + fullColorBounds.width() * 0.35, fullColorBounds.top() + fullColorBounds.height() * 0.2);
        folder.lineTo(fullColorBounds.left() + fullColorBounds.width() * 0.48, fullColorBounds.top() + fullColorBounds.height() * 0.34);
        folder.lineTo(fullColorBounds.right(), fullColorBounds.top() + fullColorBounds.height() * 0.34);
        folder.lineTo(fullColorBounds.right(), fullColorBounds.bottom());
        folder.lineTo(fullColorBounds.left(), fullColorBounds.bottom());
        folder.closeSubpath();
        painter.drawPath(folder);
    } else {
        painter.drawRoundedRect(fullColorBounds, pixelSize.width() * 0.1, pixelSize.height() * 0.1);
        painter.setPen(QPen(Qt::white, std::max(1.0, pixelSize.width() * 0.06)));
        painter.drawLine(
            QPointF(fullColorBounds.left() + fullColorBounds.width() * 0.25, fullColorBounds.top() + fullColorBounds.height() * 0.42),
            QPointF(fullColorBounds.right() - fullColorBounds.width() * 0.25, fullColorBounds.top() + fullColorBounds.height() * 0.42));
        painter.drawLine(
            QPointF(fullColorBounds.left() + fullColorBounds.width() * 0.25, fullColorBounds.top() + fullColorBounds.height() * 0.62),
            QPointF(fullColorBounds.right() - fullColorBounds.width() * 0.25, fullColorBounds.top() + fullColorBounds.height() * 0.62));
    }
    image.setDevicePixelRatio(devicePixelRatio);
    return image;
}

QString IconThemeService::cacheKey(
    const QStringList &candidates,
    const QSize &pixelSize,
    qreal devicePixelRatio) const
{
    return QString::number(m_revision)
        + QLatin1Char('|')
        + candidates.join(QLatin1Char('\x1f'))
        + QLatin1Char('|')
        + QString::number(pixelSize.width())
        + QLatin1Char('x')
        + QString::number(pixelSize.height())
        + QLatin1Char('|')
        + QString::number(qRound(devicePixelRatio * 100.0));
}

} // namespace AstreaSDK
