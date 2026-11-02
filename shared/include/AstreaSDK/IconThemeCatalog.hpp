#pragma once

#include <AstreaSDK/Export.hpp>

#include <QHash>
#include <QList>
#include <QSize>
#include <QSet>
#include <QStringList>
#include <QVariant>

namespace AstreaSDK {

class ASTREA_SDK_EXPORT IconThemeCatalog final
{
public:
    struct ResolvedIconAsset final {
        QString themeName;
        QString iconName;
        QString filePath;
    };

    bool themeExists(const QString &themeName) const;
    QStringList themeFamily(const QString &themeName) const;
    ResolvedIconAsset resolveIconAsset(
        const QString &themeName,
        const QStringList &candidates,
        const QSize &logicalSize,
        qreal devicePixelRatio) const;
    QString resolveIconName(
        const QString &themeName,
        const QStringList &candidates) const;
    QStringList searchRootPaths() const;
    QStringList watchPaths(const QStringList &themeNames) const;
    void invalidate() const;

private:
    enum class DirectoryType {
        Fixed,
        Scalable,
        Threshold,
    };

    struct DirectoryMetadata final {
        QString path;
        int size = 0;
        int scale = 1;
        int minSize = 0;
        int maxSize = 0;
        int threshold = 2;
        DirectoryType type = DirectoryType::Threshold;
    };

    struct ThemeMetadata final {
        QStringList inherits;
        QList<DirectoryMetadata> directories;
        QStringList roots;
    };

    static bool isValidThemeIdentifier(const QString &themeName);
    static bool isSafeRelativePath(const QString &path);
    static bool isFilesystemPath(const QString &path);
    static bool directoryMatchesSize(
        const DirectoryMetadata &directory,
        int requestedSize,
        int requestedScale);
    static qint64 directorySizeDistance(
        const DirectoryMetadata &directory,
        int requestedSize,
        int requestedScale);
    static QStringList splitList(const QVariant &value);
    static void appendUnique(QStringList &values, const QString &value);

    void refreshSearchSnapshot() const;
    const ThemeMetadata *metadataFor(const QString &themeName) const;
    QString findIconPath(
        const QString &themeName,
        const QString &candidate,
        const QSize &logicalSize,
        qreal devicePixelRatio) const;
    bool hasIcon(const QString &themeName, const QString &candidate) const;
    void appendTheme(
        const QString &themeName,
        QSet<QString> &visited,
        QStringList &family,
        bool deferHicolor) const;
    void appendWatchPathsForTheme(
        const QString &themeName,
        QSet<QString> &visited,
        QStringList &paths) const;

    mutable QStringList m_searchPaths;
    mutable QString m_fallbackThemeName;
    mutable QHash<QString, ThemeMetadata> m_metadata;
    mutable QSet<QString> m_missingThemes;
    mutable QHash<QString, bool> m_presence;
};

} // namespace AstreaSDK
