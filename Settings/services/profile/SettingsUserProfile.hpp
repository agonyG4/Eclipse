#pragma once

#include <QUrl>
#include <QString>

struct SettingsUserProfile {
    QString userName;
    QUrl avatarUrl;
    bool administrator = false;
};
