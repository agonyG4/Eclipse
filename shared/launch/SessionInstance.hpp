#pragma once

#include <QString>
#include <QtGlobal>

namespace astrea {

// Typhon exports ASTREA_SESSION_INSTANCE only for a secondary session (a
// second Typhon on another VT). Per-user endpoints such as the shell IPC
// socket and the Paper control socket take it as a suffix so two sessions do
// not collide; the primary session keeps the unsuffixed names.
inline QString sessionInstance()
{
    QString instance = qEnvironmentVariable("ASTREA_SESSION_INSTANCE").trimmed();
    for (QChar &character : instance) {
        if (!character.isLetterOrNumber() && character != QLatin1Char('-')
            && character != QLatin1Char('_')) {
            character = QLatin1Char('_');
        }
    }
    return instance;
}

inline QString sessionScopedName(const QString &base)
{
    const QString instance = sessionInstance();
    return instance.isEmpty() ? base : base + QLatin1Char('-') + instance;
}

} // namespace astrea
