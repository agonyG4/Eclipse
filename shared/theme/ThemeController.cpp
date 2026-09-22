#include "theme/ThemeController.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <utility>

namespace {

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

void ThemeController::updateWatchPaths()
{
    for (const QString &path : m_watcher.files())
        m_watcher.removePath(path);
    for (const QString &path : m_watcher.directories())
        m_watcher.removePath(path);

    const QFileInfo fileInfo(m_configPath);
    if (fileInfo.exists() && fileInfo.isFile())
        m_watcher.addPath(fileInfo.absoluteFilePath());

    QDir directory = fileInfo.absoluteDir();
    while (!directory.exists() && directory.cdUp()) {
    }
    if (directory.exists())
        m_watcher.addPath(directory.absolutePath());
}
