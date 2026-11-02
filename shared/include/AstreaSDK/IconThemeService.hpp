#pragma once

#include <AstreaSDK/Export.hpp>

#include <QImage>
#include <QIcon>
#include <QCache>
#include <QObject>
#include <QSize>
#include <QStringList>
#include <QUrl>

#include <AstreaSDK/IconThemeCatalog.hpp>

class QFileSystemWatcher;
class QTimer;

namespace AstreaSDK {

class ASTREA_SDK_EXPORT IconThemeService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString configuredBaseTheme READ configuredBaseTheme NOTIFY themeChanged)
    Q_PROPERTY(QString effectiveTheme READ effectiveTheme NOTIFY themeChanged)
    Q_PROPERTY(AppearanceMode appearance READ appearance NOTIFY themeChanged)
    Q_PROPERTY(quint64 revision READ revision NOTIFY themeChanged)

public:
    enum class AppearanceMode {
        Light,
        Dark,
    };
    Q_ENUM(AppearanceMode)

    explicit IconThemeService(QObject *parent = nullptr);
    explicit IconThemeService(const QString &configPath, QObject *parent = nullptr);

    QString configuredBaseTheme() const;
    QString effectiveTheme() const;
    AppearanceMode appearance() const;
    quint64 revision() const;

    QString resolveAppearanceVariant(
        const QString &baseTheme,
        AppearanceMode appearanceMode) const;

    static bool isValidThemeIdentifier(const QString &themeName);

    QStringList iconCandidatesForFile(
        const QString &path,
        bool isDirectory,
        bool isExecutable) const;
    QStringList iconCandidatesForNames(const QStringList &names) const;
    QStringList symbolicCandidatesForNames(const QStringList &names) const;
    QIcon resolveIcon(const QStringList &candidates) const;
    QImage renderIcon(
        const QStringList &candidates,
        const QSize &logicalSize,
        qreal devicePixelRatio = 1.0) const;

    Q_INVOKABLE QString iconSourceForNames(
        const QStringList &names,
        int size,
        qreal devicePixelRatio = 1.0) const;
    Q_INVOKABLE QString symbolicIconSourceForNames(
        const QStringList &names,
        int size,
        qreal devicePixelRatio = 1.0) const;
    Q_INVOKABLE QString emblemIconSource(
        const QString &name,
        int size,
        qreal devicePixelRatio = 1.0) const;
    Q_INVOKABLE QString fileIconSource(
        const QString &path,
        bool isDirectory,
        bool isExecutable,
        int size,
        const QString &semanticIconName = QString(),
        qreal devicePixelRatio = 1.0) const;
    Q_INVOKABLE QString richFileIconSource(
        const QString &path,
        bool isDirectory,
        bool isExecutable,
        int size,
        const QString &semanticIconName,
        const QStringList &iconNames,
        const QUrl &iconFileUrl,
        const QString &iconFileVersion,
        qreal devicePixelRatio = 1.0) const;

signals:
    void themeChanged();

private slots:
    void reloadConfig();
    void scheduleReload(const QString &path);

private:
    struct ThemeSelection final {
        QString name;
        QString source;
    };

    static QString canonicalConfigPath();
    static void appendUnique(QStringList &names, const QString &name);
    static QStringList fallbackCandidates();

    bool themeIsUsable(const QString &themeName) const;
    ThemeSelection selectTheme() const;
    void applyTheme(const ThemeSelection &selection);
    void updateWatcher();
    QString themeTopologySignature() const;
    QStringList themeWatchNames() const;
    QIcon resolveIconForRender(
        const QStringList &candidates,
        const QSize &logicalSize,
        qreal devicePixelRatio) const;
    void clearRenderedCache() const;
    QImage builtInFallback(
        const QSize &pixelSize,
        qreal devicePixelRatio,
        bool symbolic) const;
    QString cacheKey(
        const QStringList &candidates,
        const QSize &pixelSize,
        qreal devicePixelRatio) const;

    QString m_configPath;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_reloadTimer = nullptr;
    QString m_platformTheme;
    QString m_effectiveTheme;
    QString m_themeSource;
    quint64 m_revision = 0;
    IconThemeCatalog m_catalog;
    QStringList m_themeWatchPaths;
    QStringList m_themeRootWatchPaths;
    bool m_themeAssetsChanged = false;
    bool m_themeTopologyChanged = false;
    bool m_hasAppliedAppearance = false;
    AppearanceMode m_appliedAppearance = AppearanceMode::Dark;
    mutable QCache<QString, QImage> m_renderedCache{8 * 1024 * 1024};
};

} // namespace AstreaSDK
