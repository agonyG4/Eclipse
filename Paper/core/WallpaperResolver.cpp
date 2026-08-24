#include "WallpaperResolver.hpp"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QStandardPaths>
#include <QUrl>

namespace Paper {
namespace {

WallpaperResolution failure(const WallpaperResolutionError error, const QString &message)
{
    WallpaperResolution result;
    result.error = error;
    result.message = message;
    return result;
}

bool isResourcePath(const QString &source)
{
    return source.startsWith(QStringLiteral(":/"));
}

} // namespace

WallpaperResolver::WallpaperResolver(QString factoryDefaultSource, QString emergencySource)
    : m_factoryDefaultSource(std::move(factoryDefaultSource))
    , m_emergencySource(std::move(emergencySource))
{
}

QString WallpaperResolver::expandSource(const QString &source)
{
    QString expanded = source.trimmed();
    if (expanded == QStringLiteral("~")) {
        return QDir::homePath();
    }
    if (expanded.startsWith(QStringLiteral("~/"))) {
        return QDir::home().filePath(expanded.mid(2));
    }
    return expanded;
}

WallpaperResolution WallpaperResolver::resolve(const WallpaperDescriptor &candidate) const
{
    if (candidate.kind() != WallpaperKind::Image) {
        return failure(WallpaperResolutionError::UnsupportedKind,
                       QStringLiteral("Paper v1 accepts image wallpapers only"));
    }
    if (candidate.scope() != WallpaperScope::Global) {
        return failure(WallpaperResolutionError::UnsupportedScope,
                       QStringLiteral("Paper v1 accepts global wallpaper scope only"));
    }
    if (candidate.source().trimmed().isEmpty()) {
        return failure(WallpaperResolutionError::InvalidDescriptor,
                       QStringLiteral("Wallpaper source is empty"));
    }

    const auto source = expandSource(candidate.source());
    if (isResourcePath(source)) {
        QImageReader reader(source);
        if (!reader.canRead() || reader.read().isNull()) {
            return failure(WallpaperResolutionError::UnsupportedImage,
                           QStringLiteral("Wallpaper resource is not a readable image"));
        }
        auto result = candidate;
        result.setResolvedSource(source);
        return {result, WallpaperResolutionError::None, {}};
    }

    const QUrl url(source);
    QString localPath = source;
    if (url.isValid() && !url.scheme().isEmpty()) {
        if (url.scheme().compare(QStringLiteral("file"), Qt::CaseInsensitive) != 0) {
            return failure(WallpaperResolutionError::InvalidUri,
                           QStringLiteral("Wallpaper source URI must use file://"));
        }
        localPath = url.toLocalFile();
        if (localPath.isEmpty()) {
            return failure(WallpaperResolutionError::InvalidUri,
                           QStringLiteral("Wallpaper file URI has no local path"));
        }
    }

    return resolveLocal(candidate, localPath);
}

WallpaperResolution WallpaperResolver::resolveLocal(const WallpaperDescriptor &candidate,
                                                     const QString &path) const
{
    const QFileInfo info(path);
    if (!info.exists()) {
        return failure(WallpaperResolutionError::SourceMissing,
                       QStringLiteral("Wallpaper source does not exist"));
    }
    if (!info.isFile()) {
        return failure(WallpaperResolutionError::SourceNotRegularFile,
                       QStringLiteral("Wallpaper source is not a regular file"));
    }
    if (!info.isReadable()) {
        return failure(WallpaperResolutionError::SourceUnreadable,
                       QStringLiteral("Wallpaper source is not readable"));
    }

    QImageReader reader(info.absoluteFilePath());
    if (!reader.canRead() || reader.read().isNull()) {
        return failure(WallpaperResolutionError::UnsupportedImage,
                       QStringLiteral("Wallpaper source is not a readable image"));
    }

    const auto canonicalPath = info.canonicalFilePath();
    if (canonicalPath.isEmpty()) {
        return failure(WallpaperResolutionError::SourceUnreadable,
                       QStringLiteral("Wallpaper source cannot be canonicalized"));
    }

    auto result = candidate;
    result.setResolvedSource(canonicalPath);
    return {result, WallpaperResolutionError::None, {}};
}

QStringList WallpaperResolver::factoryCandidates() const
{
    QStringList candidates;
    if (!m_factoryDefaultSource.isEmpty()) {
        candidates.append(m_factoryDefaultSource);
        return candidates;
    }

    const auto environmentDefault = qEnvironmentVariable("ASTREA_WALLPAPER_DEFAULT");
    if (!environmentDefault.isEmpty()) {
        candidates.append(environmentDefault);
    }

    const auto installedDefault = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("AstreaOS/wallpapers/default.jpg"),
        QStandardPaths::LocateFile);
    if (!installedDefault.isEmpty()) {
        candidates.append(installedDefault);
    }

#ifdef ASTREA_PAPER_SOURCE_DIR
    candidates.append(QDir(QStringLiteral(ASTREA_PAPER_SOURCE_DIR)).filePath(
        QStringLiteral("assets/default.jpg")));
#endif

    const auto astreaRoot = qEnvironmentVariable("ASTREA_ROOT");
    if (!astreaRoot.isEmpty()) {
        candidates.append(QDir(astreaRoot).filePath(
            QStringLiteral("Features/Paper/library/landscapes/Sequoia/wallpaper.jpg")));
        candidates.append(QDir(astreaRoot).filePath(
            QStringLiteral("src/Features/Paper/library/landscapes/Sequoia/wallpaper.jpg")));
    }

    const auto dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (!dataHome.isEmpty()) {
        candidates.append(QDir(dataHome).filePath(
            QStringLiteral("AstreaOS/wallpapers/Sequoia/wallpaper.jpg")));
    }
    return candidates;
}

WallpaperResolution WallpaperResolver::factoryDefault(const WallpaperFit fit) const
{
    for (const auto &source : factoryCandidates()) {
        auto descriptor = WallpaperDescriptor::systemResource(
            QStringLiteral("astrea://wallpaper/default"), source, fit);
        const auto result = resolve(descriptor);
        if (result.ok()) {
            return result;
        }
    }

    auto emergency = WallpaperDescriptor::systemResource(
        QStringLiteral("astrea://wallpaper/emergency"), m_emergencySource, fit);
    auto result = resolve(emergency);
    if (result.ok()) {
        result.error = WallpaperResolutionError::EmergencyFallback;
        result.message = QStringLiteral("Factory artwork was unavailable; using emergency wallpaper");
        return result;
    }

    return failure(WallpaperResolutionError::FactoryDefaultUnavailable,
                   QStringLiteral("No factory or emergency wallpaper is available"));
}

} // namespace Paper
