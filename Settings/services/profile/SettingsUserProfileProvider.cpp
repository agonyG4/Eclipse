#include "services/profile/SettingsUserProfileProvider.hpp"

#include <QFileInfo>

#include <utility>

SettingsUserProfileProvider::SettingsUserProfileProvider(AdminGroupDetector detector)
    : m_detector(std::move(detector))
{
}

SettingsUserProfile SettingsUserProfileProvider::currentProfile() const
{
    const QString userName = resolveUserName();
    return {
        userName,
        resolveAvatarUrl(userName),
        m_detector.isCurrentUserAdministrator(),
    };
}

QString SettingsUserProfileProvider::resolveUserName()
{
    QString userName = qEnvironmentVariable("USER").trimmed();
    if (userName.isEmpty())
        userName = qEnvironmentVariable("LOGNAME").trimmed();
    if (userName.isEmpty())
        userName = QStringLiteral("User");
    return userName;
}

QUrl SettingsUserProfileProvider::resolveAvatarUrl(const QString &userName)
{
    if (userName.isEmpty())
        return {};

    const QString path = QStringLiteral("/var/lib/AccountsService/icons/%1").arg(userName);
    const QFileInfo avatar(path);
    return avatar.isReadable() && avatar.isFile() ? QUrl::fromLocalFile(path) : QUrl();
}
