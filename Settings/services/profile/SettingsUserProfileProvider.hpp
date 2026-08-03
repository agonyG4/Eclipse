#pragma once

#include "platform/linux/AdminGroupDetector.hpp"
#include "services/profile/SettingsUserProfile.hpp"

class SettingsUserProfileProvider final {
public:
    explicit SettingsUserProfileProvider(AdminGroupDetector detector = {});

    SettingsUserProfile currentProfile() const;

private:
    static QString resolveUserName();
    static QUrl resolveAvatarUrl(const QString &userName);

    AdminGroupDetector m_detector;
};
