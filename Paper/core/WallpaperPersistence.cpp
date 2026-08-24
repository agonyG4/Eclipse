#include "WallpaperPersistence.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace Paper {
namespace {

QByteArray encode(const QString &value)
{
    return value.toUtf8().toPercentEncoding();
}

QString decode(const QString &value)
{
    return QString::fromUtf8(QByteArray::fromPercentEncoding(value.toUtf8()));
}

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

} // namespace

std::optional<WallpaperSelection> WallpaperPersistence::loadSelection(
    QString *errorMessage) const
{
    const auto descriptor = load(errorMessage);
    if (!descriptor) {
        return std::nullopt;
    }
    return WallpaperSelection{descriptor->logicalId(), descriptor->fit()};
}

bool WallpaperPersistence::saveSelection(const WallpaperSelection &selection,
                                          QString *errorMessage)
{
    auto descriptor = WallpaperDescriptor::externalFile(selection.wallpaperId, selection.fit);
    descriptor.setLogicalId(selection.wallpaperId);
    return save(descriptor, errorMessage);
}

XdgWallpaperPersistence::XdgWallpaperPersistence(QString path, QString legacyPath)
    : m_path(path.isEmpty() ? defaultPath() : std::move(path))
    , m_legacyPath(legacyPath.isEmpty()
                       ? QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
                             .filePath(QStringLiteral("AstreaOS/user/paper/wallpaper/wallpaper.jpg"))
                       : std::move(legacyPath))
{
}

QString XdgWallpaperPersistence::defaultPath()
{
    auto configHome = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (configHome.isEmpty()) {
        configHome = QDir::homePath() + QStringLiteral("/.config");
    }
    return QDir(configHome).filePath(QStringLiteral("AstreaOS/paper.ini"));
}

std::optional<WallpaperDescriptor> XdgWallpaperPersistence::load(QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!QFileInfo::exists(m_path)) {
        return std::nullopt;
    }

    QSettings settings(m_path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("wallpaper"));
    const auto source = decode(settings.value(QStringLiteral("source")).toString());
    if (source.isEmpty()) {
        settings.endGroup();
        if (settings.status() != QSettings::NoError) {
            setError(errorMessage, QStringLiteral("Could not read Paper configuration"));
        }
        return std::nullopt;
    }

    auto descriptor = WallpaperDescriptor::externalFile(
        source, wallpaperFitFromString(settings.value(QStringLiteral("fit"),
                                                       QStringLiteral("cover"))
                                           .toString()));
    descriptor.setKind(wallpaperKindFromString(
        settings.value(QStringLiteral("kind"), QStringLiteral("image")).toString()));
    descriptor.setSourceKind(wallpaperSourceKindFromString(
        settings.value(QStringLiteral("sourceKind"), QStringLiteral("external-file"))
            .toString()));
    descriptor.setScope(wallpaperScopeFromString(
        settings.value(QStringLiteral("scope"), QStringLiteral("global")).toString()));
    descriptor.setLogicalId(decode(settings.value(QStringLiteral("logicalId"), source).toString()));
    settings.endGroup();

    if (settings.status() != QSettings::NoError) {
        setError(errorMessage, QStringLiteral("Could not read Paper configuration"));
        return std::nullopt;
    }
    return descriptor;
}

std::optional<WallpaperSelection> XdgWallpaperPersistence::loadSelection(
    QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!QFileInfo::exists(m_path)) {
        return std::nullopt;
    }

    QSettings settings(m_path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("wallpaper"));
    const auto logicalId = decode(settings.value(QStringLiteral("logicalId")).toString());
    const auto source = decode(settings.value(QStringLiteral("source")).toString());
    const auto id = logicalId.isEmpty() ? source : logicalId;
    const auto fit = wallpaperFitFromString(settings.value(QStringLiteral("fit"),
                                                             QStringLiteral("cover"))
                                                .toString());
    settings.endGroup();
    if (settings.status() != QSettings::NoError) {
        setError(errorMessage, QStringLiteral("Could not read Paper configuration"));
        return std::nullopt;
    }
    if (id.isEmpty()) {
        return std::nullopt;
    }
    return WallpaperSelection{id, fit};
}

bool XdgWallpaperPersistence::save(const WallpaperDescriptor &descriptor,
                                    QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }

    const QFileInfo pathInfo(m_path);
    if (!QDir().mkpath(pathInfo.absolutePath())) {
        setError(errorMessage, QStringLiteral("Could not create Paper configuration directory"));
        return false;
    }

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(errorMessage, QStringLiteral("Could not open Paper configuration for writing"));
        return false;
    }

    const auto logicalId = descriptor.logicalId().isEmpty() ? descriptor.source()
                                                              : descriptor.logicalId();
    const QByteArray contents = QByteArrayLiteral("[wallpaper]\n")
        + QByteArrayLiteral("version=1\n")
        + QByteArrayLiteral("source=") + encode(descriptor.source()) + QByteArrayLiteral("\n")
        + QByteArrayLiteral("logicalId=") + encode(logicalId) + QByteArrayLiteral("\n")
        + QByteArrayLiteral("kind=") + wallpaperKindToString(descriptor.kind()).toUtf8()
        + QByteArrayLiteral("\n")
        + QByteArrayLiteral("sourceKind=")
        + wallpaperSourceKindToString(descriptor.sourceKind()).toUtf8() + QByteArrayLiteral("\n")
        + QByteArrayLiteral("fit=") + wallpaperFitToString(descriptor.fit()).toUtf8()
        + QByteArrayLiteral("\n")
        + QByteArrayLiteral("scope=") + wallpaperScopeToString(descriptor.scope()).toUtf8()
        + QByteArrayLiteral("\n");
    if (file.write(contents) != contents.size() || !file.commit()) {
        setError(errorMessage, QStringLiteral("Could not atomically write Paper configuration"));
        return false;
    }

    QFile::setPermissions(m_path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

bool XdgWallpaperPersistence::saveSelection(const WallpaperSelection &selection,
                                             QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!selection.isValid()) {
        setError(errorMessage, QStringLiteral("Wallpaper selection ID is empty"));
        return false;
    }

    const QFileInfo pathInfo(m_path);
    if (!QDir().mkpath(pathInfo.absolutePath())) {
        setError(errorMessage, QStringLiteral("Could not create Paper configuration directory"));
        return false;
    }
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(errorMessage, QStringLiteral("Could not open Paper configuration for writing"));
        return false;
    }
    const QByteArray contents = QByteArrayLiteral("[wallpaper]\n")
        + QByteArrayLiteral("version=2\n")
        + QByteArrayLiteral("logicalId=") + encode(selection.wallpaperId) + QByteArrayLiteral("\n")
        + QByteArrayLiteral("fit=") + wallpaperFitToString(selection.fit).toUtf8()
        + QByteArrayLiteral("\n");
    if (file.write(contents) != contents.size() || !file.commit()) {
        setError(errorMessage, QStringLiteral("Could not atomically write Paper selection"));
        return false;
    }
    QFile::setPermissions(m_path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

bool XdgWallpaperPersistence::clear(QString *errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
    if (!QFileInfo::exists(m_path)) {
        return true;
    }
    if (!QFile::remove(m_path)) {
        setError(errorMessage, QStringLiteral("Could not clear Paper configuration"));
        return false;
    }
    return true;
}

QString XdgWallpaperPersistence::location() const
{
    return m_path;
}

std::optional<WallpaperDescriptor> XdgWallpaperPersistence::migrateLegacy() const
{
    if (QFileInfo::exists(m_path)) {
        return std::nullopt;
    }
    const QFileInfo legacyInfo(m_legacyPath);
    if (!legacyInfo.exists() || !legacyInfo.isSymLink()) {
        return std::nullopt;
    }
    const auto target = legacyInfo.symLinkTarget();
    const QFileInfo targetInfo(target);
    if (target.isEmpty() || !targetInfo.exists() || !targetInfo.isFile()) {
        return std::nullopt;
    }
    auto descriptor = WallpaperDescriptor::externalFile(targetInfo.canonicalFilePath(),
                                                        WallpaperFit::Cover);
    descriptor.setLogicalId(QStringLiteral("legacy:astreaos-paper"));
    return descriptor;
}

} // namespace Paper
