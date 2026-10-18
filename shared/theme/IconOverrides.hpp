#pragma once

#include "theme/ThemePackCatalog.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>
#include <QString>
#include <QUrl>

namespace Astrea::IconOverrides {

struct Selection {
    QString theme;
    QString weight;
    QString icon;
    QUrl file;
};

inline bool validWeight(const QString &weight)
{
    return weight == QStringLiteral("ultralight") || weight == QStringLiteral("thin")
        || weight == QStringLiteral("light") || weight == QStringLiteral("regular")
        || weight == QStringLiteral("medium") || weight == QStringLiteral("semibold")
        || weight == QStringLiteral("bold") || weight == QStringLiteral("heavy")
        || weight == QStringLiteral("black");
}

inline QString lookupKey(const QJsonObject &object, const QString &key)
{
    if (object.contains(key))
        return key;
    if (key == QStringLiteral("wired") && object.contains(QStringLiteral("ethernet")))
        return QStringLiteral("ethernet");
    if (key == QStringLiteral("wifi-off") && object.contains(QStringLiteral("wifi")))
        return QStringLiteral("wifi");
    if (key == QStringLiteral("bluetooth-off") && object.contains(QStringLiteral("bluetooth")))
        return QStringLiteral("bluetooth");
    if (key.startsWith(QStringLiteral("volume-")) && object.contains(QStringLiteral("volume")))
        return QStringLiteral("volume");
    return {};
}

inline QJsonValue overrideFor(const QJsonObject &overrides, const QString &area,
                              const QString &key)
{
    if (!ThemePacks::validSegment(key))
        return {};
    const QJsonObject scoped = overrides.value(area).toObject();
    const QString scopedKey = lookupKey(scoped, key);
    if (!scopedKey.isEmpty())
        return scoped.value(scopedKey);
    const QString globalKey = lookupKey(overrides, key);
    return globalKey.isEmpty() ? QJsonValue{} : overrides.value(globalKey);
}

// Only relative SVG/PNG files under the user's dedicated override root are allowed.
// Canonical paths reject traversal and symlinks that escape the override directory.
inline QUrl userFile(const QString &area, const QString &name)
{
    if (area != QStringLiteral("shell") && area != QStringLiteral("settings"))
        return {};
    if (name.isEmpty() || name.contains(QLatin1Char('\\'))
        || name.startsWith(QLatin1Char('/')))
        return {};
    const QString clean = QDir::cleanPath(name);
    if (clean != name || clean == QStringLiteral("..")
        || clean.startsWith(QStringLiteral("../"))
        || !(name.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)
             || name.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)))
        return {};
    const QDir base(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                    + QStringLiteral("/AstreaOS/icon-overrides/") + area);
    const QString canonicalRoot = base.canonicalPath();
    if (canonicalRoot.isEmpty())
        return {};
    const QFileInfo file(base.filePath(name));
    const QString canonicalFile = file.canonicalFilePath();
    if (!file.isFile() || !canonicalFile.startsWith(canonicalRoot + QLatin1Char('/')))
        return {};
    return QUrl::fromLocalFile(canonicalFile);
}

inline Selection resolve(const QJsonObject &overrides, const QString &area,
                         const QString &key, const QString &defaultTheme,
                         const QString &defaultWeight)
{
    Selection result{defaultTheme, defaultWeight, key, {}};
    if (!ThemePacks::validSegment(key))
        return result;
    const QJsonValue item = overrideFor(overrides, area, key);
    if (item.isString()) {
        const QString value = item.toString();
        if (validWeight(value))
            result.weight = value;
        return result;
    }
    if (!item.isObject())
        return result;
    const QJsonObject custom = item.toObject();
    const QString weight = custom.value(QStringLiteral("weight")).toString();
    if (validWeight(weight))
        result.weight = weight;
    const QString theme = custom.value(QStringLiteral("theme")).toString();
    if (theme == QStringLiteral("lucide") || theme == QStringLiteral("apple"))
        result.theme = theme;
    const QString icon = custom.value(QStringLiteral("icon")).toString();
    if (ThemePacks::validSegment(icon))
        result.icon = icon;
    result.file = userFile(area, custom.value(QStringLiteral("file")).toString());
    return result;
}

} // namespace Astrea::IconOverrides
