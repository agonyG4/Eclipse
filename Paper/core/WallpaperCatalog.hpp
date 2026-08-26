#pragma once

#include "WallpaperDescriptor.hpp"
#include "WallpaperResolver.hpp"

#include <QVector>

#include <optional>

namespace Paper {

class WallpaperCatalog final
{
public:
    explicit WallpaperCatalog(WallpaperResolver resolver = WallpaperResolver(),
                              QString userDirectory = {},
                              QString systemDirectory = {});

    void refresh();
    const QVector<WallpaperDescriptor> &list() const;
    std::optional<WallpaperDescriptor> resolve(const QString &logicalId) const;
    bool contains(const QString &logicalId) const;
    std::optional<WallpaperDescriptor> importWallpaper(const QString &source,
                                                       const QString &displayName,
                                                       QString *errorMessage = nullptr);
    std::optional<WallpaperDescriptor> importWallpaper(const QString &source,
                                                       QString *errorMessage = nullptr)
    {
        return importWallpaper(source, {}, errorMessage);
    }

    QString userDirectory() const;
    QString systemDirectory() const;

private:
    static QString defaultUserDirectory();
    static QString defaultSystemDirectory();
    static QString localPathFor(const QString &source);
    static bool isSupportedImage(const QString &path,
                                 QString *errorMessage,
                                 bool decode = true);
    static QString contentDigest(const QString &path, QString *errorMessage);
    static QString metadataPathFor(const QString &directory, const QString &digest);
    static QString readDisplayName(const QString &path);
    static bool writeDisplayName(const QString &path,
                                 const QString &displayName,
                                 QString *errorMessage);
    static bool normalizeDisplayName(const QString &displayName,
                                     QString *normalized,
                                     QString *errorMessage = nullptr);
    void addDescriptor(WallpaperDescriptor descriptor);
    void scanDirectory(const QString &directory, WallpaperOrigin origin);

    WallpaperResolver m_resolver;
    QString m_userDirectory;
    QString m_systemDirectory;
    QVector<WallpaperDescriptor> m_entries;
};

} // namespace Paper

Q_DECLARE_METATYPE(Paper::WallpaperCatalog)
