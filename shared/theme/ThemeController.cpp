#include "theme/ThemeController.hpp"
#include "theme/Typography.hpp"
#include "theme/ThemePackCatalog.hpp"
#include "theme/IconOverrides.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QRegularExpression>
#include <QResource>
#include <QStandardPaths>
#include <QUrlQuery>

#include <cerrno>
#include <cmath>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <utility>

namespace {

// Resolve generic names through Qt's platform font matcher (Fontconfig on Linux),
// rather than treating the application's default font as a named family.
QString systemFontFamily(const QString &genericFamily)
{
    if (!qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        return {};
    return QFontInfo(QFont(genericFamily)).family();
}

class ConfigFileLock final {
public:
    explicit ConfigFileLock(const QString &configPath)
    {
        const QByteArray lockPath = QFile::encodeName(configPath + QStringLiteral(".lock"));
        m_descriptor = ::open(lockPath.constData(), O_CREAT | O_RDWR | O_CLOEXEC, 0666);
        if (m_descriptor < 0)
            return;

        int result = -1;
        do {
            result = ::flock(m_descriptor, LOCK_EX);
        } while (result < 0 && errno == EINTR);
        if (result < 0) {
            ::close(m_descriptor);
            m_descriptor = -1;
        }
    }

    ~ConfigFileLock()
    {
        if (m_descriptor >= 0) {
            ::flock(m_descriptor, LOCK_UN);
            ::close(m_descriptor);
        }
    }

    ConfigFileLock(const ConfigFileLock &) = delete;
    ConfigFileLock &operator=(const ConfigFileLock &) = delete;

    bool isLocked() const { return m_descriptor >= 0; }

private:
    int m_descriptor = -1;
};

} // namespace

ThemeController::ThemeController(const QString &configPath, QObject *parent,
                                 ColorSchemeProvider colorSchemeProvider)
    : QObject(parent)
    , m_configPath(configPath.isEmpty()
                       ? QDir::homePath() + QStringLiteral("/.config/AstreaOS/ui/theme.json")
                       : QFileInfo(configPath).absoluteFilePath())
    , m_colorSchemeProvider(std::move(colorSchemeProvider))
{
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(100);
    connect(&m_reloadTimer, &QTimer::timeout, this, &ThemeController::reload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, [this](const QString &) { scheduleReload(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, [this](const QString &) { scheduleReload(); });
    if (auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
        application && application->styleHints()) {
        connect(application->styleHints(), &QStyleHints::colorSchemeChanged,
                this, [this](Qt::ColorScheme) { handlePlatformColorSchemeChanged(); });
    }
    reload();
    updateEffectiveThemeMode();
    m_loaded = true;
}

int ThemeController::themeMode() const { return m_themeMode; }

void ThemeController::setThemeMode(int value)
{
    const int next = value == 1 ? 1 : 0;
    setThemePreference(next == 1 ? QStringLiteral("light") : QStringLiteral("dark"));
}

QString ThemeController::themePreference() const { return m_themePreference; }

void ThemeController::setThemePreference(const QString &value)
{
    const QString next = normalizedThemePreference(value);
    if (m_themePreference != next) {
        m_themePreference = next;
        emit themePreferenceChanged();
    }
    updateEffectiveThemeMode();
}

int ThemeController::shellStyle() const { return m_shellStyle; }

void ThemeController::setShellStyle(int value)
{
    const int next = value >= 0 && value <= 2 ? value : 1;
    if (m_shellStyle == next)
        return;
    m_shellStyle = next;
    emit shellStyleChanged();
}

int ThemeController::iconStyle() const { return m_iconStyle; }

void ThemeController::setIconStyle(int value)
{
    if (m_iconStyle == value)
        return;
    m_iconStyle = value;
    emit iconStyleChanged();
}

QString ThemeController::iconTheme() const { return m_iconTheme; }

void ThemeController::setIconTheme(const QString &value)
{
    if (m_iconTheme == value)
        return;
    m_iconTheme = value;
    emit iconThemeChanged();
}

QString ThemeController::iconAppearance() const { return m_iconAppearance; }

void ThemeController::setIconAppearance(const QString &value)
{
    const QString next = normalizedIconAppearance(value);
    if (m_iconAppearance == next)
        return;
    m_iconAppearance = next;
    emit iconAppearanceChanged();
}

QString ThemeController::accentHex() const { return m_accentHex; }

void ThemeController::setAccentHex(const QString &value)
{
    const QString next = value.trimmed().isEmpty() ? QStringLiteral("#0a84ff") : value.trimmed();
    if (m_accentHex == next)
        return;
    m_accentHex = next;
    emit accentHexChanged();
}

int ThemeController::audioOsdStyle() const { return m_audioOsdStyle; }

void ThemeController::setAudioOsdStyle(int value)
{
    const int next = qBound(0, value, 1);
    if (m_audioOsdStyle == next)
        return;
    m_audioOsdStyle = next;
    emit audioOsdStyleChanged();
}

QString ThemeController::configPath() const { return m_configPath; }
bool ThemeController::loaded() const { return m_loaded; }

QUrl ThemeController::themedComponent(const QString &name) const
{
    QUrl result = Astrea::ThemePacks::component(m_visualTheme, name);
    if (result.isEmpty())
        return result;
    QUrlQuery query(result);
    query.addQueryItem(QStringLiteral("astreaRevision"), QString::number(m_themeComponentsRevision));
    result.setQuery(query);
    return result;
}

void ThemeController::applyConfig(const QVariantMap &config)
{
    const auto value = [&config](const QString &key) { return config.value(key); };
    QString resolvedThemePreference = QStringLiteral("auto");
    const QString configuredPreference = value(QStringLiteral("theme_preference")).toString();
    if (isValidThemePreference(configuredPreference)) {
        resolvedThemePreference = configuredPreference;
    } else {
        const QString legacyTheme = value(QStringLiteral("theme")).toString();
        if (legacyTheme.compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0) {
            resolvedThemePreference = QStringLiteral("light");
        } else if (legacyTheme.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0) {
            resolvedThemePreference = QStringLiteral("dark");
        } else {
            bool ok = false;
            const int legacyMode = value(QStringLiteral("theme_mode")).toInt(&ok);
            if (ok && (legacyMode == 0 || legacyMode == 1))
                resolvedThemePreference = legacyMode == 1 ? QStringLiteral("light") : QStringLiteral("dark");
        }
    }
    setThemePreference(resolvedThemePreference);

    bool shellStyleOk = false;
    const int shellStyle = value(QStringLiteral("shell_style")).toInt(&shellStyleOk);
    setShellStyle(shellStyleOk ? shellStyle : 1);
    if (value(QStringLiteral("icon_style")).isValid())
        setIconStyle(value(QStringLiteral("icon_style")).toInt());
    if (value(QStringLiteral("icon_theme")).isValid())
        setIconTheme(value(QStringLiteral("icon_theme")).toString());
    setIconAppearance(value(QStringLiteral("icon_appearance")).toString());
    if (value(QStringLiteral("accent")).isValid())
        setAccentHex(value(QStringLiteral("accent")).toString());
    if (value(QStringLiteral("audio_osd_style")).isValid())
        setAudioOsdStyle(value(QStringLiteral("audio_osd_style")).toInt());
    m_legacyFontPreferences = Astrea::Typography::validatedPreferences(value(QStringLiteral("font_preferences")).toMap());

}

void ThemeController::reload()
{
    if (m_reloading)
        return;
    m_reloading = true;
    QFile file(m_configPath);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error == QJsonParseError::NoError && document.isObject())
            applyConfig(document.object().toVariantMap());
    }
    reloadTypography();
    reloadNavigationIcons();
    ++m_themeComponentsRevision;
    emit themeComponentsRevisionChanged();
    updateWatchPaths();
    m_reloading = false;
}

void ThemeController::save()
{
    if (!QDir().mkpath(QFileInfo(m_configPath).absolutePath()))
        return;

    {
        const ConfigFileLock lock(m_configPath);
        if (!lock.isLocked())
            return;

        QJsonObject object;
        if (QFileInfo::exists(m_configPath)) {
            QFile existing(m_configPath);
            if (!existing.open(QIODevice::ReadOnly))
                return;
            QJsonParseError error;
            const QJsonDocument document = QJsonDocument::fromJson(existing.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject())
                return;
            object = document.object();
        }

        // Rust owns the persisted Appearance projection. Keep those keys and
        // the legacy theme compatibility inputs untouched during shell saves.
        object.insert(QStringLiteral("shell_style"), m_shellStyle);
        object.insert(QStringLiteral("icon_style"), m_iconStyle);
        object.insert(QStringLiteral("icon_theme"), m_iconTheme);
        object.insert(QStringLiteral("audio_osd_style"), m_audioOsdStyle);

        QSaveFile file(m_configPath);
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly))
            return;
        const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
        if (file.write(bytes) != bytes.size()) {
            file.cancelWriting();
            return;
        }
        if (!file.commit())
            return;
    }
    updateWatchPaths();
}

QStringList ThemeController::availableFontFamilies() const
{
    return QFontDatabase::families();
}

QString ThemeController::typographyPath() const
{
    return QFileInfo(m_configPath).absoluteDir().filePath(QStringLiteral("typography.json"));
}

void ThemeController::reloadTypography()
{
    QVariantMap next = m_legacyFontPreferences;
    QFile file(typographyPath());
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) return;
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(file.readAll(), &error);
        const auto object = document.object();
        if (error.error != QJsonParseError::NoError || !document.isObject()
            || object.value("schema").toInt() != 1 || !object.value("preferences").isObject()) return;
        next = Astrea::Typography::validatedPreferences(object.value("preferences").toObject().toVariantMap());
    }
    if (next != m_fontPreferences) {
        m_fontPreferences = next;
        emit fontPreferencesChanged();
    }
}

void ThemeController::patchTypography(const QString &field, const QVariant &value, bool reset)
{
    QString error;
    const bool saved = Astrea::Typography::patchPreferences(typographyPath(), m_legacyFontPreferences,
                                                           field, value, reset, &error);
    if (m_typographyError != error) {
        m_typographyError = error;
        emit typographyErrorChanged();
    }
    if (saved) {
        reloadTypography();
        reloadNavigationIcons();
        updateWatchPaths();
    }
}

void ThemeController::setFontFamilyPreference(const QString &key, const QString &family)
{
    if (!QStringList{"fontFamily", "fontFamilyText", "fontFamilyDisplay", "monoFontFamily"}.contains(key)
        || (!family.isEmpty() && !QFontDatabase::families().contains(family))) return;
    patchTypography(key, family.isEmpty() ? QVariant{} : QVariant(family));
}

void ThemeController::setFontSizePreference(const QString &key, int size)
{
    if (key != "fontSizeNormal" && key != "fontSizeTitle") return;
    if (size != 0 && (size < 9 || size > 96)) return;
    patchTypography(key, size == 0 ? QVariant{} : QVariant(size));
}

void ThemeController::setTextScale(double scale)
{
    if (!std::isfinite(scale) || scale < 0.75 || scale > 2.0) return;
    patchTypography("textScale", scale == 1.0 ? QVariant{} : QVariant(scale));
}

void ThemeController::setTypographyPreference(const QString &role, const QString &field, const QVariant &value)
{
    if (!Astrea::Typography::roleNames().contains(role)
        || !QStringList{"family", "pixelSize", "weight", "letterSpacing", "lineHeight"}.contains(field)) return;
    const bool clear = !value.isValid() || value.isNull() || (value.metaType().id() == QMetaType::QString && value.toString().isEmpty());
    if (!clear && !Astrea::Typography::validatedRoles({{role, QVariantMap{{field, value}}}})
            .value(role).toMap().contains(field)) return;
    patchTypography(role + "." + field, clear ? QVariant{} : value);
}

void ThemeController::resetFontPreferences()
{
    patchTypography({}, {}, true);
}

QString ThemeController::normalizedThemePreference(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return isValidThemePreference(normalized) ? normalized : QStringLiteral("auto");
}

bool ThemeController::isValidThemePreference(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("auto")
        || normalized == QStringLiteral("light")
        || normalized == QStringLiteral("dark");
}

QString ThemeController::normalizedIconAppearance(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return isValidIconAppearance(normalized) ? normalized : QStringLiteral("default");
}

bool ThemeController::isValidIconAppearance(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("default")
        || normalized == QStringLiteral("monochrome")
        || normalized == QStringLiteral("tinted");
}

void ThemeController::updateEffectiveThemeMode()
{
    int next = 0;
    if (m_themePreference == QStringLiteral("light")) {
        next = 1;
    } else if (m_themePreference == QStringLiteral("auto")) {
        next = platformColorScheme() == Qt::ColorScheme::Light ? 1 : 0;
    }
    setEffectiveThemeMode(next);
}

void ThemeController::setEffectiveThemeMode(int value)
{
    const int next = value == 1 ? 1 : 0;
    if (m_themeMode == next)
        return;
    m_themeMode = next;
    emit themeModeChanged();
}

Qt::ColorScheme ThemeController::platformColorScheme() const
{
    if (m_colorSchemeProvider)
        return m_colorSchemeProvider();

    auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    if (!application || !application->styleHints())
        return Qt::ColorScheme::Unknown;
    return application->styleHints()->colorScheme();
}

void ThemeController::handlePlatformColorSchemeChanged()
{
    if (m_themePreference == QStringLiteral("auto"))
        updateEffectiveThemeMode();
}

void ThemeController::scheduleReload()
{
    m_reloadTimer.start();
}

QUrl ThemeController::themedNavigationIcon(const QString &key) const
{
    static const QRegularExpression valid(QStringLiteral("^[a-z0-9-]+$"));
    if (!valid.match(key).hasMatch())
        return {};
    const auto selected = Astrea::IconOverrides::resolve(
        m_navigationIconOverrides, QStringLiteral("shell"), key,
        m_navigationIconTheme, m_navigationIconWeight);
    if (selected.file.isValid())
        return selected.file;
    if (selected.theme != QStringLiteral("apple")
        && selected.theme != QStringLiteral("lucide"))
        return {};
    if (selected.theme == QStringLiteral("apple")) {
        const QString local = QStandardPaths::writableLocation(
            QStandardPaths::GenericDataLocation)
            + QStringLiteral("/AstreaOS/Themes/AppleOS/bar-")
            + selected.icon + QStringLiteral(".svg");
        const QFileInfo file(local);
        if (file.isFile())
            return QUrl::fromLocalFile(file.absoluteFilePath());
    }
    // Lucide is always available as the default and as Apple's local fallback.
    const QString prefix = QStringLiteral(":/qt/qml/Astrea/Shell/Bar/icons/lucide/");
    const QString weighted = prefix + QStringLiteral("weights/")
        + selected.weight + QLatin1Char('/') + selected.icon + QStringLiteral(".svg");
    if (QResource(weighted).isValid())
        return QUrl(QStringLiteral("qrc") + weighted);
    const QString master = prefix + selected.icon + QStringLiteral(".svg");
    return QResource(master).isValid()
        ? QUrl(QStringLiteral("qrc") + master) : QUrl{};

}

void ThemeController::reloadNavigationIcons()
{
    const QString path = QFileInfo(m_configPath).absoluteDir()
        .filePath(QStringLiteral("navigation-icons.json"));
    QFile config(path);
    QString next = QStringLiteral("lucide");
    if (config.open(QIODevice::ReadOnly)) {
        const QString requested = QJsonDocument::fromJson(config.readAll())
                                      .object().value(QStringLiteral("theme")).toString();
        if (requested == QStringLiteral("apple"))
            next = requested;
    }
    if (next != m_navigationIconTheme) {
        m_navigationIconTheme = next;
        emit navigationIconThemeChanged();
    }
    QFile weightConfig(path);
    QString nextWeight = QStringLiteral("regular");
    if (weightConfig.open(QIODevice::ReadOnly)) {
        const QString stored = QJsonDocument::fromJson(weightConfig.readAll())
            .object().value(QStringLiteral("weight")).toString();
        static const QStringList validWeights{
            QStringLiteral("ultralight"), QStringLiteral("thin"),
            QStringLiteral("light"), QStringLiteral("regular"),
            QStringLiteral("medium"), QStringLiteral("semibold"),
            QStringLiteral("bold"), QStringLiteral("heavy"), QStringLiteral("black")
        };
        if (validWeights.contains(stored))
            nextWeight = stored;
    }
    if (nextWeight != m_navigationIconWeight) {
        m_navigationIconWeight = nextWeight;
        emit navigationIconWeightChanged();
    }
    QFile overridesConfig(path);
    QJsonObject nextOverrides;
    if (overridesConfig.open(QIODevice::ReadOnly))
        nextOverrides = QJsonDocument::fromJson(overridesConfig.readAll())
            .object().value(QStringLiteral("overrides")).toObject();
    if (nextOverrides != m_navigationIconOverrides) {
        m_navigationIconOverrides = nextOverrides;
        ++m_navigationIconOverridesRevision;
        emit navigationIconOverridesChanged();
    }
    QFile visualConfig(path);
    QString requestedVisualTheme = QStringLiteral("astrea");
    if (visualConfig.open(QIODevice::ReadOnly)) {
        requestedVisualTheme = QJsonDocument::fromJson(visualConfig.readAll())
            .object().value(QStringLiteral("visual_theme")).toString();
        if (requestedVisualTheme.isEmpty())
            requestedVisualTheme = QStringLiteral("astrea");
    }
    m_requestedVisualTheme = requestedVisualTheme;
    const QString nextVisualTheme = Astrea::ThemePacks::find(requestedVisualTheme).has_value()
        ? requestedVisualTheme : QStringLiteral("astrea");
    QVariantMap nextTokens = Astrea::ThemePacks::styleTokens(nextVisualTheme);
    nextTokens = Astrea::Typography::resolve(nextTokens, m_fontPreferences,
        systemFontFamily(QStringLiteral("sans-serif")), systemFontFamily(QStringLiteral("monospace")));
    if (nextVisualTheme != m_visualTheme || nextTokens != m_styleTokens) {
        m_visualTheme = nextVisualTheme;
        m_styleTokens = nextTokens;
        emit visualThemeChanged();
    }
}

void ThemeController::updateWatchPaths()
{
    for (const QString &path : m_watcher.files())
        m_watcher.removePath(path);
    for (const QString &path : m_watcher.directories())
        m_watcher.removePath(path);

    const QFileInfo fileInfo(m_configPath);
    if (fileInfo.exists() && fileInfo.isFile())
        m_watcher.addPath(fileInfo.absoluteFilePath());
    if (QFileInfo::exists(typographyPath()))
        m_watcher.addPath(typographyPath());
    const QString iconsPath = fileInfo.absoluteDir()
        .filePath(QStringLiteral("navigation-icons.json"));
    if (QFileInfo::exists(iconsPath))
        m_watcher.addPath(iconsPath);

    QDir directory = fileInfo.absoluteDir();
    while (!directory.exists() && directory.cdUp()) {
    }
    if (directory.exists())
        m_watcher.addPath(directory.absolutePath());

    if (Astrea::ThemePacks::validId(m_requestedVisualTheme)
        && m_requestedVisualTheme != QStringLiteral("astrea")) {
        for (const QString &base : QStandardPaths::standardLocations(
                 QStandardPaths::GenericDataLocation)) {
            const QString themesPath = QDir(base).filePath(QStringLiteral("AstreaOS/themes"));
            const QString packagePath = QDir(themesPath).filePath(m_requestedVisualTheme);
            const QString manifestPath = QDir(packagePath).filePath(QStringLiteral("theme.json"));
            if (QFileInfo(manifestPath).isFile())
                m_watcher.addPath(manifestPath);
            if (QFileInfo(packagePath).isDir())
                m_watcher.addPath(packagePath);

            for (const QString &componentName : {QStringLiteral("spotlightPanel"),
                                                  QStringLiteral("altTabPanel")}) {
                const QString configuredPath = Astrea::ThemePacks::configuredComponentPath(
                    m_requestedVisualTheme, componentName);
                if (!configuredPath.isEmpty()) {
                    QDir nearest = QFileInfo(configuredPath).absoluteDir();
                    while (!nearest.exists() && nearest.cdUp()) {
                    }
                    if (nearest.exists())
                        m_watcher.addPath(nearest.absolutePath());
                }
                const QUrl componentUrl = Astrea::ThemePacks::component(
                    m_requestedVisualTheme, componentName, QStringLiteral("1"));
                if (componentUrl.isLocalFile()) {
                    const QString componentPath = componentUrl.toLocalFile();
                    m_watcher.addPath(componentPath);
                    const QFileInfo componentInfo(componentPath);
                    if (componentInfo.dir().exists())
                        m_watcher.addPath(componentInfo.absolutePath());
                }
            }

            QDir existing(themesPath);
            while (!existing.exists() && existing.cdUp()) {
            }
            if (existing.exists())
                m_watcher.addPath(existing.absolutePath());
        }
    }
}
