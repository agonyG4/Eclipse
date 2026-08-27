#include "core/ThemeController.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

ThemeController::ThemeController(const QString &configPath, QObject *parent)
    : QObject(parent)
    , m_configPath(configPath.isEmpty()
                       ? QDir::homePath() + QStringLiteral("/.config/AstreaOS/ui/theme.json")
                       : configPath)
{
    load();
}

int ThemeController::themeMode() const
{
    return m_themeMode;
}

void ThemeController::setThemeMode(int value)
{
    const int next = value == 1 ? 1 : 0;
    if (m_themeMode == next)
        return;
    m_themeMode = next;
    emit themeModeChanged();
}

int ThemeController::shellStyle() const
{
    return m_shellStyle;
}

void ThemeController::setShellStyle(int value)
{
    const int next = qBound(0, value, 2);
    if (m_shellStyle == next)
        return;
    m_shellStyle = next;
    emit shellStyleChanged();
}

int ThemeController::iconStyle() const
{
    return m_iconStyle;
}

void ThemeController::setIconStyle(int value)
{
    if (m_iconStyle == value)
        return;
    m_iconStyle = value;
    emit iconStyleChanged();
}

QString ThemeController::iconTheme() const
{
    return m_iconTheme;
}

void ThemeController::setIconTheme(const QString &value)
{
    if (m_iconTheme == value)
        return;
    m_iconTheme = value;
    emit iconThemeChanged();
}

QString ThemeController::accentHex() const
{
    return m_accentHex;
}

void ThemeController::setAccentHex(const QString &value)
{
    const QString next = value.trimmed().isEmpty() ? QStringLiteral("#0a84ff") : value.trimmed();
    if (m_accentHex == next)
        return;
    m_accentHex = next;
    emit accentHexChanged();
}

int ThemeController::audioOsdStyle() const
{
    return m_audioOsdStyle;
}

void ThemeController::setAudioOsdStyle(int value)
{
    const int next = qBound(0, value, 1);
    if (m_audioOsdStyle == next)
        return;
    m_audioOsdStyle = next;
    emit audioOsdStyleChanged();
}

QString ThemeController::configPath() const
{
    return m_configPath;
}

bool ThemeController::loaded() const
{
    return m_loaded;
}

void ThemeController::applyConfig(const QVariantMap &config)
{
    const auto value = [&config](const QString &key) {
        return config.value(key);
    };

    if (value(QStringLiteral("theme_mode")).isValid())
        setThemeMode(value(QStringLiteral("theme_mode")).toInt());
    if (value(QStringLiteral("theme")).toString().compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0)
        setThemeMode(1);
    else if (value(QStringLiteral("theme")).toString().compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0)
        setThemeMode(0);
    if (value(QStringLiteral("shell_style")).isValid())
        setShellStyle(value(QStringLiteral("shell_style")).toInt());
    if (value(QStringLiteral("icon_style")).isValid())
        setIconStyle(value(QStringLiteral("icon_style")).toInt());
    if (value(QStringLiteral("icon_theme")).isValid())
        setIconTheme(value(QStringLiteral("icon_theme")).toString());
    if (value(QStringLiteral("accent")).isValid())
        setAccentHex(value(QStringLiteral("accent")).toString());
    if (value(QStringLiteral("audio_osd_style")).isValid())
        setAudioOsdStyle(value(QStringLiteral("audio_osd_style")).toInt());
}

void ThemeController::save()
{
    QDir().mkpath(QFileInfo(m_configPath).absolutePath());
    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    const QJsonObject object{
        {QStringLiteral("theme"), m_themeMode == 1 ? QStringLiteral("light") : QStringLiteral("dark")},
        {QStringLiteral("theme_mode"), m_themeMode},
        {QStringLiteral("shell_style"), m_shellStyle},
        {QStringLiteral("accent"), m_accentHex},
        {QStringLiteral("icon_style"), m_iconStyle},
        {QStringLiteral("icon_theme"), m_iconTheme},
        {QStringLiteral("audio_osd_style"), m_audioOsdStyle},
    };
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

void ThemeController::load()
{
    QFile file(m_configPath);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error == QJsonParseError::NoError && document.isObject())
            applyConfig(document.object().toVariantMap());
    }
    m_loaded = true;
}
