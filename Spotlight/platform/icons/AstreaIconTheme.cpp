#include "platform/icons/AstreaIconTheme.hpp"

#include <QIcon>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QDebug>

QStringList AstreaIconTheme::searchPaths() {
    QSet<QString> seen;
    QStringList ordered;

    // XDG_DATA_HOME (~/.local/share/icons)
    const QStringList dataHome = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const auto &d : dataHome) {
        QString iconsDir = d + QStringLiteral("/icons");
        const QString clean = QDir::cleanPath(iconsDir);
        if (!QDir(clean).exists() || seen.contains(clean))
            continue;
        seen.insert(clean);
        ordered.append(clean);
    }

    // $HOME/.icons
    const QString dotIcons = QDir::homePath() + QStringLiteral("/.icons");
    if (QDir(dotIcons).exists()) {
        const QString clean = QDir::cleanPath(dotIcons);
        if (!seen.contains(clean)) {
            seen.insert(clean);
            ordered.append(clean);
        }
    }

    // Flatpak exports
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

    // /usr/share/pixmaps is not an icon theme dir, handled separately in provider

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

    // 1. ASTREA_ICON_THEME
    QString env = qEnvironmentVariable("ASTREA_ICON_THEME").trimmed();
    if (!env.isEmpty() && themeExists(env)) {
        result.theme = env;
        result.source = QStringLiteral("ASTREA_ICON_THEME");
        return result;
    }
    if (!env.isEmpty()) {
        qWarning("ASTREA_ICON_THEME='%s' set but theme not found, falling through",
                 qPrintable(env));
    }

    // 2. QS_ICON_THEME
    env = qEnvironmentVariable("QS_ICON_THEME").trimmed();
    if (!env.isEmpty() && themeExists(env)) {
        result.theme = env;
        result.source = QStringLiteral("QS_ICON_THEME");
        return result;
    }
    if (!env.isEmpty()) {
        qWarning("QS_ICON_THEME='%s' set but theme not found, falling through",
                 qPrintable(env));
    }

    // 3. qt6ct config
    const QStringList configDirs = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation);
    for (const auto &configDir : configDirs) {
        const QString qt6ctPath = configDir + QStringLiteral("/qt6ct/qt6ct.conf");
        if (QFileInfo::exists(qt6ctPath)) {
            QSettings settings(qt6ctPath, QSettings::IniFormat);
            settings.beginGroup(QStringLiteral("Appearance"));
            QString qt6ctTheme = settings.value(QStringLiteral("icon_theme")).toString().trimmed();
            if (!qt6ctTheme.isEmpty() && themeExists(qt6ctTheme)) {
                result.theme = qt6ctTheme;
                result.source = QStringLiteral("qt6ct");
                return result;
            }
            break;
        }
    }

    // 4. Platform theme from QIcon (skip "hicolor" — it is the universal final
    //    fallback and has already been set as fallbackThemeName; accepting it
    //    here would make the compatibility fallback in step 5 unreachable).
    QString platformTheme = QIcon::themeName().trimmed();
    if (!platformTheme.isEmpty() && platformTheme != QStringLiteral("hicolor")) {
        if (themeExists(platformTheme)) {
            result.theme = platformTheme;
            result.source = QStringLiteral("platform");
            return result;
        }
        qWarning("Platform icon theme '%s' not found in any search path, "
                 "falling through", qPrintable(platformTheme));
    }

    // 5. Compatibility fallback - WhiteSur-dark
    if (themeExists(QStringLiteral("WhiteSur-dark"))) {
        result.theme = QStringLiteral("WhiteSur-dark");
        result.source = QStringLiteral("compatibility");
        return result;
    }

    // 6. Final fallback
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

    qInfo().noquote()
        << QStringLiteral("Astrea Spotlight icon theme: %1 (source: %2) fallback: hicolor")
               .arg(theme, resolved.source);

    return theme;
}
