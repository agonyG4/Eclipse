#pragma once
#include <QString>

namespace Astrea::SystemFontConfig {
// Update only the owned block in the user's fonts.conf; an empty pair removes it.
bool apply(const QString &path, const QString &primary, const QString &mono, QString *error);
}
