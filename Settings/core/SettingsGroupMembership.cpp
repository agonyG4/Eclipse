#include "core/SettingsGroupMembership.hpp"

#include <algorithm>

bool hasAdministrativeGroup(const QStringList &groupNames)
{
    return std::any_of(groupNames.cbegin(), groupNames.cend(), [](const QString &groupName) {
        return groupName == QStringLiteral("wheel") || groupName == QStringLiteral("sudo");
    });
}
