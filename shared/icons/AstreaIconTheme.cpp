#include "icons/AstreaIconTheme.hpp"

#include <QIcon>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QSet>

namespace {

QStringList mergePaths(const QStringList &preferred, const QStringList &existing)
{
    QSet<QString> seen;
    QStringList merged;
    for (const QStringList &paths : {preferred, existing}) {
        for (const QString &path : paths) {
            if (path.isEmpty() || seen.contains(path))
                continue;
            seen.insert(path);
            merged.append(path);
        }
    }
    return merged;
}

} // namespace

QStringList AstreaIconTheme::searchPaths() {
    return searchPathsFor(
        QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation),
        QDir::homePath());
}

QStringList AstreaIconTheme::searchPathsFor(const QStringList &dataLocations,
                                            const QString &homePath) {
    QSet<QString> seen;
    QStringList ordered;

    for (const auto &d : dataLocations) {
        const QString iconsDir = d + QStringLiteral("/icons");
        const QString clean = QDir::cleanPath(iconsDir);
        if (!QDir(clean).exists() || seen.contains(clean))
            continue;
        seen.insert(clean);
        ordered.append(clean);
    }

    const QString dotIcons = QDir::cleanPath(homePath + QStringLiteral("/.icons"));
    if (QDir(dotIcons).exists()) {
        const QString clean = QDir::cleanPath(dotIcons);
        if (!seen.contains(clean)) {
            seen.insert(clean);
            ordered.append(clean);
        }
    }

    QStringList flatpakDirs = {
        QDir::cleanPath(homePath + QStringLiteral(
            "/.local/share/flatpak/exports/share/icons")),
        QStringLiteral("/var/lib/flatpak/exports/share/icons")
    };
    for (const auto &d : flatpakDirs) {
        const QString clean = QDir::cleanPath(d);
        if (!QDir(clean).exists() || seen.contains(clean))
            continue;
        seen.insert(clean);
        ordered.append(clean);
    }

    return ordered;
}

bool AstreaIconTheme::themeExists(const QString &themeName) {
    if (themeName.isEmpty()) return false;
    for (const auto &dir : searchPaths()) {
        if (QFileInfo::exists(dir + QStringLiteral("/") + themeName + QStringLiteral("/index.theme")))
            return true;
    }
    return false;
}

static bool themeExistsInHighestPriorityDataHome(const QString &themeName) {
    const QString dataHome = qEnvironmentVariable(
        "XDG_DATA_HOME", QDir::homePath() + QStringLiteral("/.local/share"));
    return QFileInfo::exists(QDir(dataHome).filePath(
        QStringLiteral("icons/") + themeName + QStringLiteral("/index.theme")));
}

AstreaIconTheme::ResolveResult AstreaIconTheme::resolveWithSource() {
    ResolveResult result;

    QString env = qEnvironmentVariable("ASTREA_ICON_THEME").trimmed();
    if (!env.isEmpty() && themeExists(env)) {
        result.theme = env;
        result.source = QStringLiteral("ASTREA_ICON_THEME");
        return result;
    }

    env = qEnvironmentVariable("QS_ICON_THEME").trimmed();
    if (!env.isEmpty() && themeExists(env)) {
        result.theme = env;
        result.source = QStringLiteral("QS_ICON_THEME");
        return result;
    }

    const QStringList configDirs = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation);
    for (const auto &configDir : configDirs) {
        const QString qt6ctPath = configDir + QStringLiteral("/qt6ct/qt6ct.conf");
        if (QFileInfo::exists(qt6ctPath)) {
            QSettings settings(qt6ctPath, QSettings::IniFormat);
            settings.beginGroup(QStringLiteral("Appearance"));
            const QString qt6ctTheme = settings.value(QStringLiteral("icon_theme")).toString().trimmed();
            if (!qt6ctTheme.isEmpty() && themeExists(qt6ctTheme)) {
                result.theme = qt6ctTheme;
                result.source = QStringLiteral("qt6ct");
                return result;
            }
            break;
        }
    }

    QString platformTheme = QIcon::themeName().trimmed();
    if (!platformTheme.isEmpty() && platformTheme != QStringLiteral("hicolor")) {
        if (themeExists(platformTheme)) {
            result.theme = platformTheme;
            result.source = QStringLiteral("platform");
            return result;
        }
    }

    if (themeExistsInHighestPriorityDataHome(QStringLiteral("WhiteSur-dark"))) {
        result.theme = QStringLiteral("WhiteSur-dark");
        result.source = QStringLiteral("compatibility");
        return result;
    }

    result.theme = QStringLiteral("hicolor");
    result.source = QStringLiteral("fallback");
    return result;
}

QString AstreaIconTheme::resolve() {
    return resolveWithSource().theme;
}

QString AstreaIconTheme::themeSource() {
    return resolveWithSource().source;
}

QString AstreaIconTheme::apply() {
    const auto resolved = resolveWithSource();
    const QString theme = resolved.theme;

    QIcon::setThemeName(theme);
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));
    // Install paths after selecting the name. Qt resets its default resource
    // path when a theme name is selected, so merging before this call loses
    // the discovered XDG roots.
    QIcon::setThemeSearchPaths(mergePaths(searchPaths(), QIcon::themeSearchPaths()));
    QIcon::setFallbackSearchPaths(mergePaths(searchPaths(), QIcon::fallbackSearchPaths()));

    return theme;
}
