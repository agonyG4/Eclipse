#pragma once
#include <QUrl>
#include <QVariantMap>
namespace ProfileBackend
{
QVariantMap snapshot();
QString validateName(const QString &name);
void setName(const QString &name);
void setAvatar(const QUrl &file);
void setAutomaticLogin(bool enabled);
} // namespace ProfileBackend
