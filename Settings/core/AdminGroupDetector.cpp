#include "core/AdminGroupDetector.hpp"

#include "core/SettingsGroupMembership.hpp"

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include <utility>
#include <vector>

namespace {

constexpr int kInitialGroupCount = 16;
constexpr int kMaxEnumerationAttempts = 8;

std::optional<AdminGroupUser> nativeCurrentUser()
{
    const passwd *entry = getpwuid(getuid());
    if (!entry || !entry->pw_name)
        return std::nullopt;

    return AdminGroupUser{QByteArray(entry->pw_name), entry->pw_gid};
}

int nativeGetGroupList(const QByteArray &userName, gid_t primaryGroup, gid_t *groups, int *count)
{
    return getgrouplist(userName.constData(), primaryGroup, groups, count);
}

std::optional<QString> nativeGroupName(gid_t groupId)
{
    const group *entry = getgrgid(groupId);
    if (!entry || !entry->gr_name)
        return std::nullopt;

    return QString::fromLocal8Bit(entry->gr_name);
}

AdminGroupDetector::NativeApi nativeApi()
{
    return {
        nativeCurrentUser,
        nativeGetGroupList,
        nativeGroupName,
    };
}

} // namespace

AdminGroupDetector::AdminGroupDetector()
    : m_api(nativeApi())
{
}

AdminGroupDetector::AdminGroupDetector(NativeApi api)
    : m_api(std::move(api))
{
}

bool AdminGroupDetector::isCurrentUserAdministrator() const
{
    if (!m_api.currentUser || !m_api.getGroupList || !m_api.groupName)
        return false;

    const std::optional<AdminGroupUser> user = m_api.currentUser();
    if (!user || user->name.isEmpty())
        return false;

    int groupCount = kInitialGroupCount;
    std::vector<gid_t> groupIds;
    bool enumerated = false;
    for (int attempt = 0; attempt < kMaxEnumerationAttempts; ++attempt) {
        groupIds.resize(static_cast<size_t>(groupCount));
        int requiredCount = groupCount;
        const int result = m_api.getGroupList(user->name, user->primaryGroup,
                                               groupIds.data(), &requiredCount);
        if (result >= 0) {
            if (result > groupCount)
                return false;
            groupIds.resize(static_cast<size_t>(result));
            enumerated = true;
            break;
        }
        if (requiredCount <= groupCount)
            return false;
        groupCount = requiredCount;
    }

    if (!enumerated)
        return false;

    QStringList groupNames;
    groupNames.reserve(static_cast<qsizetype>(groupIds.size()));
    for (const gid_t groupId : groupIds) {
        const std::optional<QString> name = m_api.groupName(groupId);
        if (!name)
            return false;
        groupNames.append(*name);
    }

    return hasAdministrativeGroup(groupNames);
}

bool isCurrentUserAdministrator()
{
    return AdminGroupDetector().isCurrentUserAdministrator();
}
