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
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

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
    const QStringList allowed{
        QStringLiteral("accent"), QStringLiteral("textPrimary"),
        QStringLiteral("textSecondary"), QStringLiteral("cardBg"),
        QStringLiteral("cardBorder"), QStringLiteral("shellBackground"),
        QStringLiteral("shellSurface"), QStringLiteral("shellBorder"),
        QStringLiteral("shellTextMain"), QStringLiteral("shellIconMain")
    };
    QVariantMap cleaned;
    for (const QString &key : allowed) {
        const QString hex = colors.value(key).toString();
        if ((hex.size() == 7 || hex.size() == 9) && hex.startsWith(QLatin1Char('#'))
            && QColor(hex).isValid())
            cleaned.insert(key, hex);
    }
    const QString font = style.value(QStringLiteral("fontFamily")).toString();
    if (!font.isEmpty() && font.size() <= 80 && !font.contains(QLatin1Char('/'))
        && !font.contains(QLatin1Char('\\')))
        cleaned.insert(QStringLiteral("fontFamily"), font);
    const QJsonValue animation = style.value(QStringLiteral("animationFast"));
    if (animation.isDouble() && animation.toInt(-1) >= 0 && animation.toInt(-1) <= 2000)
        cleaned.insert(QStringLiteral("animationFast"), animation.toInt());
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
