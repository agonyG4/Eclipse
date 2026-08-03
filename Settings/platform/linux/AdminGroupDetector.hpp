#pragma once

#include <QByteArray>
#include <QString>

#include <functional>
#include <optional>

#include <sys/types.h>

struct AdminGroupUser {
    QByteArray name;
    gid_t primaryGroup = 0;
};

class AdminGroupDetector final {
public:
    struct NativeApi {
        std::function<std::optional<AdminGroupUser>()> currentUser;
        std::function<int(const QByteArray &, gid_t, gid_t *, int *)> getGroupList;
        std::function<std::optional<QString>(gid_t)> groupName;
    };

    AdminGroupDetector();
    explicit AdminGroupDetector(NativeApi api);

    bool isCurrentUserAdministrator() const;

private:
    NativeApi m_api;
};

bool isCurrentUserAdministrator();
