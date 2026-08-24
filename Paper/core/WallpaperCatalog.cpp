#include "WallpaperCatalog.hpp"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

namespace Paper {
namespace {

constexpr qint64 kMaxImportBytes = 64 * 1024 * 1024;
constexpr qint64 kMaxImagePixels = 32 * 1024 * 1024;
constexpr int kMaxImageDimension = 8192;

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

bool isImageFile(const QFileInfo &info)
{
    const auto suffix = info.suffix().toLower();
    return suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")
        || suffix == QStringLiteral("png") || suffix == QStringLiteral("webp")
        || suffix == QStringLiteral("bmp") || suffix == QStringLiteral("gif");
}

} // namespace

WallpaperCatalog::WallpaperCatalog(WallpaperResolver resolver,
                                   QString userDirectory,
                                   QString systemDirectory)
    : m_resolver(std::move(resolver))
    , m_userDirectory(userDirectory.isEmpty() ? defaultUserDirectory()
                                               : std::move(userDirectory))
    , m_systemDirectory(systemDirectory.isEmpty() ? defaultSystemDirectory()
                                                    : std::move(systemDirectory))
{
    refresh();
}

void WallpaperCatalog::refresh()
{
    m_entries.clear();

    const auto factory = m_resolver.factoryDefault();
    if (factory.ok()) {
        auto descriptor = factory.descriptor;
        descriptor.setOrigin(WallpaperOrigin::System);
        descriptor.setDisplayName(QStringLiteral("Astrea Default"));
        addDescriptor(std::move(descriptor));
    }

    scanDirectory(m_systemDirectory, WallpaperOrigin::System);
    scanDirectory(m_userDirectory, WallpaperOrigin::User);

    std::sort(m_entries.begin(), m_entries.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.logicalId() < rhs.logicalId();
    });
}

const QVector<WallpaperDescriptor> &WallpaperCatalog::list() const
{
    return m_entries;
}

std::optional<WallpaperDescriptor> WallpaperCatalog::resolve(const QString &logicalId) const
{
    const auto match = std::find_if(m_entries.cbegin(), m_entries.cend(),
                                    [&logicalId](const auto &entry) {
                                        return entry.logicalId() == logicalId;
                                    });
    if (match == m_entries.cend()) {
        return std::nullopt;
    }
    return *match;
}

bool WallpaperCatalog::contains(const QString &logicalId) const
{
    return resolve(logicalId).has_value();
}

std::optional<WallpaperDescriptor> WallpaperCatalog::importWallpaper(const QString &source,
                                                                       QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    const auto path = localPathFor(source);
    if (path.isEmpty()) {
        setError(errorMessage, QStringLiteral("Wallpaper import requires a local file"));
        return std::nullopt;
    }
    const QFileInfo sourceInfo(path);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        setError(errorMessage, QStringLiteral("Wallpaper import source is not a regular file"));
        return std::nullopt;
    }
    if (!sourceInfo.isReadable()) {
        setError(errorMessage, QStringLiteral("Wallpaper import source is not readable"));
        return std::nullopt;
    }
    if (sourceInfo.size() <= 0 || sourceInfo.size() > kMaxImportBytes) {
        setError(errorMessage, QStringLiteral("Wallpaper import source exceeds the size limit"));
        return std::nullopt;
    }
    const auto canonical = sourceInfo.canonicalFilePath();
    if (canonical.isEmpty() || !isSupportedImage(canonical, errorMessage)) {
        return std::nullopt;
    }

    const auto digest = contentDigest(canonical, errorMessage);
    if (digest.isEmpty()) {
        return std::nullopt;
    }
    const auto suffix = QFileInfo(canonical).suffix().toLower();
    const auto extension = suffix.isEmpty() ? QStringLiteral("png") : suffix;
    const auto finalPath = QDir(m_userDirectory).filePath(digest + QStringLiteral(".") + extension);
    const auto logicalId = QStringLiteral("astrea://wallpaper/user/") + digest;

    if (const auto existing = resolve(logicalId)) {
        return existing;
    }

    if (!QDir().mkpath(m_userDirectory)) {
        setError(errorMessage, QStringLiteral("Could not create the Paper user wallpaper directory"));
        return std::nullopt;
    }

    const auto temporaryPath = finalPath + QStringLiteral(".tmp-")
        + QString::number(QCoreApplication::applicationPid()) + QStringLiteral("-")
        + QString::number(QDateTime::currentMSecsSinceEpoch());
    QFile::remove(temporaryPath);
    if (!QFile::copy(canonical, temporaryPath)) {
        setError(errorMessage, QStringLiteral("Could not copy wallpaper into the Paper library"));
        return std::nullopt;
    }
    if (!isSupportedImage(temporaryPath, errorMessage)) {
        QFile::remove(temporaryPath);
        return std::nullopt;
    }
    if (QFileInfo::exists(finalPath) || !QFile::rename(temporaryPath, finalPath)) {
        QFile::remove(temporaryPath);
        if (QFileInfo::exists(finalPath)) {
            refresh();
            return resolve(logicalId);
        }
        setError(errorMessage, QStringLiteral("Could not atomically publish wallpaper import"));
        return std::nullopt;
    }

    QFile::setPermissions(finalPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    WallpaperDescriptor descriptor = WallpaperDescriptor::externalFile(finalPath,
                                                                         WallpaperFit::Cover);
    descriptor.setLogicalId(logicalId);
    descriptor.setOrigin(WallpaperOrigin::User);
    descriptor.setDisplayName(QFileInfo(canonical).completeBaseName());
    descriptor.setResolvedSource(QFileInfo(finalPath).canonicalFilePath());
    addDescriptor(descriptor);
    return descriptor;
}

QString WallpaperCatalog::userDirectory() const
{
    return m_userDirectory;
}

QString WallpaperCatalog::systemDirectory() const
{
    return m_systemDirectory;
}

QString WallpaperCatalog::defaultUserDirectory()
{
    auto dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (dataHome.isEmpty()) {
        dataHome = QDir::homePath() + QStringLiteral("/.local/share");
    }
    return QDir(dataHome).filePath(QStringLiteral("AstreaOS/Paper/wallpapers"));
}

QString WallpaperCatalog::defaultSystemDirectory()
{
    const auto dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return dataHome.isEmpty() ? QString() : QDir(dataHome).filePath(QStringLiteral("AstreaOS/wallpapers"));
}

QString WallpaperCatalog::localPathFor(const QString &source)
{
    const auto expanded = WallpaperResolver::expandSource(source);
    const QUrl url(expanded);
    if (url.isValid() && url.scheme().compare(QStringLiteral("file"), Qt::CaseInsensitive) == 0) {
        return QFileInfo(url.toLocalFile()).absoluteFilePath();
    }
    if (!url.scheme().isEmpty() || expanded.startsWith(QStringLiteral(":"))) {
        return {};
    }
    return QFileInfo(expanded).absoluteFilePath();
}

bool WallpaperCatalog::isSupportedImage(const QString &path,
                                         QString *errorMessage,
                                         const bool decode)
{
    QImageReader reader(path);
    const auto size = reader.size();
    if (!reader.canRead() || (size.isValid()
                              && (size.width() <= 0 || size.height() <= 0
                                  || size.width() > kMaxImageDimension
                                  || size.height() > kMaxImageDimension
                                  || static_cast<qint64>(size.width()) * size.height()
                                      > kMaxImagePixels))) {
        setError(errorMessage, QStringLiteral("Wallpaper import image is invalid or too large"));
        return false;
    }
    if (decode && reader.read().isNull()) {
        setError(errorMessage, QStringLiteral("Wallpaper import image cannot be decoded"));
        return false;
    }
    return true;
}

QString WallpaperCatalog::contentDigest(const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QStringLiteral("Could not read wallpaper import source"));
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const auto chunk = file.read(1024 * 1024);
        if (chunk.isEmpty() && !file.atEnd()) {
            setError(errorMessage, QStringLiteral("Could not hash wallpaper import source"));
            return {};
        }
        hash.addData(chunk);
    }
    return QString::fromLatin1(hash.result().toHex());
}

void WallpaperCatalog::addDescriptor(WallpaperDescriptor descriptor)
{
    const auto match = std::find_if(m_entries.begin(), m_entries.end(),
                                    [&descriptor](const auto &entry) {
                                        return entry.logicalId() == descriptor.logicalId();
                                    });
    if (match != m_entries.end()) {
        *match = std::move(descriptor);
        return;
    }
    m_entries.append(std::move(descriptor));
}

void WallpaperCatalog::scanDirectory(const QString &directory, const WallpaperOrigin origin)
{
    if (directory.isEmpty() || !QFileInfo(directory).isDir()) {
        return;
    }
    QDir root(directory);
    const auto entries = root.entryInfoList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                                            QDir::Name);
    for (const auto &info : entries) {
        if (!isImageFile(info) || !isSupportedImage(info.absoluteFilePath(), nullptr, false)) {
            continue;
        }
        const auto canonical = info.canonicalFilePath();
        if (canonical.isEmpty()) {
            continue;
        }
        const auto prefix = origin == WallpaperOrigin::System
            ? QStringLiteral("astrea://wallpaper/system/")
            : QStringLiteral("astrea://wallpaper/user/");
        const auto baseId = prefix + info.completeBaseName();
        if (origin == WallpaperOrigin::System
            && canonical == m_resolver.factoryDefault().descriptor.resolvedSource()) {
            continue;
        }
        auto descriptor = WallpaperDescriptor::externalFile(canonical, WallpaperFit::Cover);
        descriptor.setLogicalId(baseId);
        descriptor.setOrigin(origin);
        if (origin == WallpaperOrigin::System) {
            descriptor.setSourceKind(WallpaperSourceKind::SystemResource);
        }
        descriptor.setDisplayName(info.completeBaseName());
        descriptor.setResolvedSource(canonical);
        addDescriptor(std::move(descriptor));
    }
}

} // namespace Paper
