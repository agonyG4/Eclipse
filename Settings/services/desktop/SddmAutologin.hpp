#pragma once
#include <QVariantMap>
namespace SddmAutologin
{
QVariantMap read(const QString &etcRoot = "/etc",
                 const QString &vendorRoot = "/usr/lib/sddm/sddm.conf.d");
void write(const QString &user, bool enabled, const QString &session,
           const QString &etcRoot = "/etc");
} // namespace SddmAutologin
