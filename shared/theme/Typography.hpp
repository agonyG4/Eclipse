#pragma once

#include <QString>
#include <QVariantMap>

namespace Astrea::Typography {
QStringList roleNames();
QVariantMap validatedRoles(const QVariantMap &roles);
QVariantMap validatedSurfaces(const QVariantMap &surfaces);
QVariantMap validatedPreferences(const QVariantMap &preferences);
QVariantMap resolve(const QVariantMap &theme, const QVariantMap &preferences,
                    const QString &proportional, const QString &monospace);
// Read-modify-write under the same advisory lock used by the other settings stores.
// Empty field + reset clears overrides; role fields use "role.field".
bool patchPreferences(const QString &path, const QVariantMap &legacy,
                      const QString &field, const QVariant &value, bool reset,
                      QString *error);
} // namespace Astrea::Typography
