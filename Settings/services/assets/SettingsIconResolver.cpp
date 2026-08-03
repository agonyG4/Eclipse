#include "services/assets/SettingsIconResolver.hpp"

#include <QString>

QUrl SettingsIconResolver::resolve(const QString &iconKey, const QString &iconTheme) const
{
    if (iconKey.isEmpty())
        return {};

    const QString base = QStringLiteral("qrc:/Astrea/Settings/assets/icons/settings/");
    if (!iconTheme.isEmpty())
        return QUrl(base + QStringLiteral("themes/") + iconTheme + QStringLiteral("/")
                    + iconKey + QStringLiteral(".svg"));
    return QUrl(base + iconKey + QStringLiteral(".svg"));
}
