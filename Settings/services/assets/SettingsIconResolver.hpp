#pragma once

#include <QUrl>

class QString;

class SettingsIconResolver final {
public:
    QUrl resolve(const QString &iconKey, const QString &iconTheme) const;
    QUrl resolve(const QString &iconKey, const QString &iconTheme, const QString &weight) const;
};
