#include "platform/typhon/RuntimeTaskIdentityTracker.hpp"

#include "platform/typhon/TyphonAppMatcher.hpp"

#include <QSet>

using namespace Astrea::Typhon;

namespace {

QString normalizedAppId(const Toplevel &window)
{
    return window.appId.trimmed().toCaseFolded();
}

QString initialTaskKeyFor(const Toplevel &window, const TyphonAppMatch &match)
{
    if (!match.desktopFileName.isEmpty())
        return QStringLiteral("desktop:") + match.desktopFileName;

    const QString appId = window.appId.trimmed();
    if (!appId.isEmpty())
        return QStringLiteral("app:") + appId.toCaseFolded();

    return QStringLiteral("window:") + window.id;
}

} // namespace

QHash<QString, QString> RuntimeTaskIdentityTracker::update(
    const Snapshot &snapshot,
    const std::shared_ptr<const DesktopEntrySnapshot> &desktopEntries)
{
    if (!m_generation.has_value() || m_generation.value() != snapshot.connectionGeneration) {
        reset();
        m_generation = snapshot.connectionGeneration;
    }

    QSet<QString> liveWindowIds;
    for (const Toplevel &window : snapshot.windows)
        liveWindowIds.insert(window.id);

    for (auto it = m_windowTaskKeys.begin(); it != m_windowTaskKeys.end();) {
        if (!liveWindowIds.contains(it.key()))
            it = m_windowTaskKeys.erase(it);
        else
            ++it;
    }

    QSet<QString> liveTaskKeys;
    for (auto it = m_windowTaskKeys.cbegin(); it != m_windowTaskKeys.cend(); ++it)
        liveTaskKeys.insert(it.value());
    for (auto it = m_appTaskKeys.begin(); it != m_appTaskKeys.end();) {
        if (!liveTaskKeys.contains(it.value()))
            it = m_appTaskKeys.erase(it);
        else
            ++it;
    }

    TyphonAppMatcher matcher(desktopEntries);
    QHash<QString, QString> assignments;
    for (const Toplevel &window : snapshot.windows) {
        QString taskKey;
        const auto existing = m_windowTaskKeys.constFind(window.id);
        if (existing != m_windowTaskKeys.constEnd()) {
            taskKey = existing.value();
            const QString appId = normalizedAppId(window);
            if (!appId.isEmpty() && !m_appTaskKeys.contains(appId)) {
                // The first live task to claim an app ID owns the alias. A
                // conflicting live claim must not move either existing task.
                m_appTaskKeys.insert(appId, taskKey);
            }
        } else {
            const QString appId = normalizedAppId(window);
            if (!appId.isEmpty())
                taskKey = m_appTaskKeys.value(appId);
            if (taskKey.isEmpty()) {
                const TyphonAppMatch match = matcher.match(
                    {window.appId, window.title, window.pid, window.kind});
                taskKey = initialTaskKeyFor(window, match);
                if (!appId.isEmpty())
                    m_appTaskKeys.insert(appId, taskKey);
            }
            m_windowTaskKeys.insert(window.id, taskKey);
        }
        assignments.insert(window.id, taskKey);
    }
    return assignments;
}

void RuntimeTaskIdentityTracker::reset()
{
    m_generation.reset();
    m_windowTaskKeys.clear();
    m_appTaskKeys.clear();
}
