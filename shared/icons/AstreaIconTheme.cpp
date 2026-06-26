#include "icons/AstreaIconTheme.hpp"

#include <QIcon>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QSet>

QStringList AstreaIconTheme::searchPaths() {
    QSet<QString> seen;
    QStringList ordered;

    const QStringList dataHome = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const auto &d : dataHome) {
        const QString iconsDir = d + QStringLiteral("/icons");
        const QString clean = QDir::cleanPath(iconsDir);
        if (!QDir(clean).exists() || seen.contains(clean))
            continue;
        seen.insert(clean);
        ordered.append(clean);
    }

    const QString dotIcons = QDir::homePath() + QStringLiteral("/.icons");
    if (QDir(dotIcons).exists()) {
        const QString clean = QDir::cleanPath(dotIcons);
        if (!seen.contains(clean)) {
            seen.insert(clean);
            ordered.append(clean);
        }
    }

    QStringList flatpakDirs = {
        QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share/icons"),
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
    const QStringList dirs = searchPaths();
    for (const auto &dir : dirs) {
        if (QFileInfo::exists(dir + QStringLiteral("/") + themeName + QStringLiteral("/index.theme")))
            return true;
    }
    return false;
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

    if (themeExists(QStringLiteral("WhiteSur-dark"))) {
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

    return theme;
}
