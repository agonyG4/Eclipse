#include "services/assets/SettingsIconResolver.hpp"
#include "theme/ThemePackCatalog.hpp"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QResource>
#include <QStandardPaths>

QUrl SettingsIconResolver::resolve(const QString &iconKey, const QString &iconTheme) const
{
    return resolve(iconKey, iconTheme, QStringLiteral("regular"));
}

QUrl SettingsIconResolver::resolve(const QString &iconKey, const QString &iconTheme,
                                   const QString &weight) const
{
    static const QRegularExpression validName(QStringLiteral("^[a-z0-9-]+$"));
    if (!validName.match(iconKey).hasMatch() || !validName.match(iconTheme).hasMatch()
        || !validName.match(weight).hasMatch())
        return {};
    if (iconTheme != QStringLiteral("lucide") && iconTheme != QStringLiteral("apple"))
        return {};
    if (iconTheme == QStringLiteral("apple")) {
        // Personal SF Symbols files never enter the open-source resource bundle.
        const QString local = QStandardPaths::writableLocation(
            QStandardPaths::GenericDataLocation)
            + QStringLiteral("/AstreaOS/Themes/AppleOS/") + iconKey
            + QStringLiteral(".svg");
        const QFileInfo file(local);
        if (file.isFile())
            return QUrl::fromLocalFile(file.absoluteFilePath());
    }
    // Default and missing-Apple-icon fallback: first-party Lucide geometry.
    const QString prefix =
        QStringLiteral(":/Astrea/Settings/assets/icons/navigation/lucide/");
    const QString weighted = prefix + QStringLiteral("weights/") + weight
                             + QLatin1Char('/') + iconKey + QStringLiteral(".svg");
    if (QResource(weighted).isValid())
        return QUrl(QStringLiteral("qrc") + weighted);
    const QString master = prefix + iconKey + QStringLiteral(".svg");
    return QResource(master).isValid() ? QUrl(QStringLiteral("qrc") + master) : QUrl{};

}
