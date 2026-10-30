#pragma once

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <optional>
#include <cmath>

// Lightweight, declarative, cross-process icon-pack discovery.
// A pack may supply its own Settings and/or Shell SVGs without changing
// the built-in Eclipse or Apple resources. Executable QML is never loaded
// from theme packages.
namespace Astrea::ThemePacks {

struct Pack {
    QString id;
    QString name;
    QString directory;
    QString settingsIcons;
    QString shellIcons;
};

inline bool validId(const QString &value)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z][a-z0-9-]{0,63}$"));
    return pattern.match(value).hasMatch();
}

inline bool validSegment(const QString &value)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9-]+$"));
    return pattern.match(value).hasMatch();
}

inline bool validRelativeDirectory(const QString &value)
{
    if (value.isEmpty() || QDir::isAbsolutePath(value)
        || value.contains(QLatin1Char('\\')))
        return false;
    const QString normalized = QDir::cleanPath(value);
    return normalized != QStringLiteral(".") && normalized != QStringLiteral("..")
        && !normalized.startsWith(QStringLiteral("../")) && normalized == value;
}

inline std::optional<Pack> readPack(const QString &base, const QString &id)
{
    if (!validId(id) || id == QStringLiteral("astrea") || id == QStringLiteral("lucide")
        || id == QStringLiteral("apple") || id == QStringLiteral("eclipse"))
        return std::nullopt;
    const QString folder = QDir(base).filePath(QStringLiteral("AstreaOS/themes/") + id);
    QFile manifest(QDir(folder).filePath(QStringLiteral("theme.json")));
    if (!manifest.open(QIODevice::ReadOnly) || manifest.size() > 65536)
        return std::nullopt;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(manifest.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    const QJsonObject json = document.object();
    if (json.value(QStringLiteral("schema")).toInt() != 1
        || json.value(QStringLiteral("id")).toString() != id)
        return std::nullopt;
    const QJsonObject icons = json.value(QStringLiteral("icons")).toObject();
    const QString settingsDir = icons.value(QStringLiteral("settings")).toString();
    const QString shellDir = icons.value(QStringLiteral("shell")).toString();
    if ((!settingsDir.isEmpty() && !validRelativeDirectory(settingsDir))
        || (!shellDir.isEmpty() && !validRelativeDirectory(shellDir))
        || (settingsDir.isEmpty() && shellDir.isEmpty()
            && json.value(QStringLiteral("style")).toObject().isEmpty()))
        return std::nullopt;
    const QFileInfo canonicalRoot(folder);
    if (!canonicalRoot.isDir())
        return std::nullopt;
    Pack pack{id, json.value(QStringLiteral("name")).toString(id),
              canonicalRoot.canonicalFilePath(), settingsDir, shellDir};
    if (pack.name.trimmed().isEmpty() || pack.name.size() > 80)
        pack.name = id;
    return pack;
}

inline std::optional<Pack> find(const QString &id)
{
    if (!validId(id))
        return std::nullopt;
    for (const QString &base : QStandardPaths::standardLocations(
             QStandardPaths::GenericDataLocation)) {
        if (auto pack = readPack(base, id))
            return pack;
    }
    return std::nullopt;
}

inline QVariantList list()
{
    // Icon selection is intentionally limited to the two supported families.
    // Visual themes remain a separate extensibility surface.
    return {
        QVariantMap{{QStringLiteral("id"), QStringLiteral("lucide")},
                    {QStringLiteral("name"), QStringLiteral("Lucide")},
                    {QStringLiteral("description"), QStringLiteral("Default open-source icons")}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("apple")},
                    {QStringLiteral("name"), QStringLiteral("AppleOS")},
                    {QStringLiteral("description"), QStringLiteral("Local SF Symbols SVGs")}}
    };
}

// Visual skins and icon packs are independent choices, though a package can
// contain both. An installed package may define colors, typography and timings.
inline QVariantList visualThemes()
{
    QVariantList result{QVariantMap{
        {QStringLiteral("id"), QStringLiteral("astrea")},
        {QStringLiteral("name"), QStringLiteral("Astrea Default")}
    }};
    QSet<QString> seen{QStringLiteral("astrea")};
    for (const QString &base : QStandardPaths::standardLocations(
             QStandardPaths::GenericDataLocation)) {
        QDir dir(QDir(base).filePath(QStringLiteral("AstreaOS/themes")));
        for (const QString &id : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                                               QDir::Name)) {
            if (seen.contains(id))
                continue;
            if (auto pack = readPack(base, id)) {
                seen.insert(id);
                result.append(QVariantMap{
                    {QStringLiteral("id"), id}, {QStringLiteral("name"), pack->name}
                });
            }
        }
    }
    return result;
}

inline QVariantMap styleTokens(const QString &id)
{
    const auto pack = find(id);
    if (!pack)
        return {};
    QFile manifest(QDir(pack->directory).filePath(QStringLiteral("theme.json")));
    if (!manifest.open(QIODevice::ReadOnly) || manifest.size() > 65536)
        return {};
    const QJsonObject style = QJsonDocument::fromJson(manifest.readAll())
                                  .object().value(QStringLiteral("style")).toObject();
    const QJsonObject colors = style.value(QStringLiteral("colors")).toObject();
    static const QStringList allowedColors{
        QStringLiteral("accent"), QStringLiteral("accentForeground"),
        QStringLiteral("textPrimary"), QStringLiteral("textSecondary"),
        QStringLiteral("textTertiary"), QStringLiteral("cardBg"),
        QStringLiteral("cardBorder"), QStringLiteral("popupBg"),
        QStringLiteral("windowBackground"), QStringLiteral("windowBorder"),
        QStringLiteral("windowWash"), QStringLiteral("errorColor"),
        QStringLiteral("warningColor"), QStringLiteral("successColor"),
        QStringLiteral("shellBackground"), QStringLiteral("shellSurface"),
        QStringLiteral("shellSurfaceElevated"), QStringLiteral("shellBorder"),
        QStringLiteral("shellBorderHover"), QStringLiteral("shellHover"),
        QStringLiteral("shellPressed"), QStringLiteral("shellActive"),
        QStringLiteral("shellTextMain"), QStringLiteral("shellTextSecondary"),
        QStringLiteral("shellTextLight"), QStringLiteral("shellTextDim"),
        QStringLiteral("shellTextActive"), QStringLiteral("shellIconMain"),
        QStringLiteral("shellIconActive"), QStringLiteral("shellIconMuted"),
        QStringLiteral("shellIconWarning"), QStringLiteral("shellIconAccent"),
        QStringLiteral("shellSeparator"), QStringLiteral("shellShadow"),
        QStringLiteral("tooltipSurface"), QStringLiteral("tooltipBorder"),
        QStringLiteral("tooltipShadow"), QStringLiteral("workspaceActive"),
        QStringLiteral("workspaceInactive")
    };
    QVariantMap cleaned;
    for (const QString &key : allowedColors) {
        const QString hex = colors.value(key).toString();
        if ((hex.size() == 7 || hex.size() == 9) && hex.startsWith(QLatin1Char('#'))
            && QColor(hex).isValid())
            cleaned.insert(key, hex);
    }

    static const QStringList metrics{
        QStringLiteral("radiusSmall"), QStringLiteral("radiusMedium"),
        QStringLiteral("radiusLarge"), QStringLiteral("cornerRadiusSmall"),
        QStringLiteral("cornerRadius"), QStringLiteral("cornerRadiusLarge"),
        QStringLiteral("cardRadius"), QStringLiteral("controlRadius"),
        QStringLiteral("shellRadiusSmall"), QStringLiteral("shellRadiusMedium"),
        QStringLiteral("shellRadiusLarge"), QStringLiteral("shellControlRadius"),
        QStringLiteral("shellTileRadius"), QStringLiteral("shellPillRadius"),
        QStringLiteral("spacingTiny"), QStringLiteral("spacingMicro"),
        QStringLiteral("spacingSmall"), QStringLiteral("spacing"),
        QStringLiteral("spacingMedium"), QStringLiteral("spacingLarge"),
        QStringLiteral("spacingXLarge"), QStringLiteral("pageMargin"),
        QStringLiteral("dockRadius"), QStringLiteral("spotlightRadius"),
        QStringLiteral("altTabRadius"), QStringLiteral("contextMenuRadius")
    };
    const auto copyNumbers = [&cleaned](const QJsonObject &object,
                                        const QStringList &names, double minimum,
                                        double maximum, bool integerOnly) {
        for (const QString &key : names) {
            const QJsonValue value = object.value(key);
            if (!value.isDouble())
                continue;
            const double number = value.toDouble();
            if (!std::isfinite(number) || number < minimum || number > maximum
                || (integerOnly && std::floor(number) != number))
                continue;
            cleaned.insert(key, integerOnly ? QVariant(static_cast<int>(number))
                                            : QVariant(number));
        }
    };
    copyNumbers(style.value(QStringLiteral("metrics")).toObject(), metrics, 0, 256, false);

    static const QStringList fontFamilies{
        QStringLiteral("fontFamily"), QStringLiteral("monoFontFamily"),
        QStringLiteral("fontFamilyDisplay"), QStringLiteral("fontFamilyText"),
        QStringLiteral("iconFontFamily")
    };
    static const QStringList fontSizes{
        QStringLiteral("fontSizeHero"), QStringLiteral("fontSizeHeader"),
        QStringLiteral("fontSizeIconLarge"), QStringLiteral("fontSizeAvatar"),
        QStringLiteral("fontSizeSubtitle"), QStringLiteral("fontSizeTitle"),
        QStringLiteral("fontSizeLarge"), QStringLiteral("fontSizeNormal"),
        QStringLiteral("fontSizeSmall"), QStringLiteral("fontSizeTiny"),
        QStringLiteral("fontSizeMicro"), QStringLiteral("fontSizeBody"),
        QStringLiteral("fontSizeCaption"), QStringLiteral("fontSizeExtraSmall"),
        QStringLiteral("fontSizeIcon"), QStringLiteral("tooltipFontSize")
    };
    const auto validFamily = [](const QString &font) {
        if (font.trimmed().isEmpty() || font.size() > 80
            || font.contains(QLatin1Char('/')) || font.contains(QLatin1Char('\\')))
            return false;
        for (const QChar character : font) {
            if (character.category() == QChar::Other_Control)
                return false;
        }
        return true;
    };
    const QString legacyFont = style.value(QStringLiteral("fontFamily")).toString();
    if (validFamily(legacyFont))
        cleaned.insert(QStringLiteral("fontFamily"), legacyFont);
    const QJsonObject typography = style.value(QStringLiteral("typography")).toObject();
    for (const QString &key : fontFamilies) {
        const QString font = typography.value(key).toString();
        if (validFamily(font))
            cleaned.insert(key, font);
    }
    copyNumbers(typography, fontSizes, 1, 96, true);

    const QJsonValue legacyAnimation = style.value(QStringLiteral("animationFast"));
    if (legacyAnimation.isDouble()) {
        const double animation = legacyAnimation.toDouble();
        if (std::isfinite(animation) && animation >= 0 && animation <= 2000
            && std::floor(animation) == animation)
            cleaned.insert(QStringLiteral("animationFast"), static_cast<int>(animation));
    }
    static const QStringList durations{
        QStringLiteral("animationMicro"), QStringLiteral("animationQuick"),
        QStringLiteral("animationFast"), QStringLiteral("animationNormal"),
        QStringLiteral("animationSlow"), QStringLiteral("animationPopover"),
        QStringLiteral("animationHover"), QStringLiteral("animationSubtle"),
        QStringLiteral("animationStandard"), QStringLiteral("animationInstant"),
        QStringLiteral("animationSlider"), QStringLiteral("animationPulse"),
        QStringLiteral("animationSpin")
    };
    copyNumbers(style.value(QStringLiteral("durations")).toObject(), durations, 0, 2000, true);
    return cleaned;
}

inline QUrl icon(const QString &packId, const QString &area, const QString &key,
                 const QString &weight)
{
    if (!validSegment(key) || !validSegment(weight))
        return {};
    const auto pack = find(packId);
    if (!pack)
        return {};
    const QString subdirectory = area == QStringLiteral("shell")
        ? pack->shellIcons : area == QStringLiteral("settings") ? pack->settingsIcons
                                                                 : QString();
    if (subdirectory.isEmpty())
        return {};
    const QDir source(QDir(pack->directory).filePath(subdirectory));
    const QString root = source.canonicalPath();
    if (root.isEmpty() || !(root == pack->directory
        || root.startsWith(pack->directory + QLatin1Char('/'))))
        return {};
    const QStringList variants{
        QStringLiteral("weights/") + weight + QLatin1Char('/') + key
            + QStringLiteral(".svg"),
        key + QStringLiteral(".svg"),
        QStringLiteral("weights/") + weight + QLatin1Char('/') + key
            + QStringLiteral(".png"),
        key + QStringLiteral(".png")
    };
    for (const QString &variant : variants) {
        const QFileInfo file(source.filePath(variant));
        if (file.isFile() && file.canonicalFilePath().startsWith(root + QLatin1Char('/')))
            return QUrl::fromLocalFile(file.canonicalFilePath());
    }
    return {};
}

} // namespace Astrea::ThemePacks
