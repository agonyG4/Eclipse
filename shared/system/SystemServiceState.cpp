#include "system/SystemServiceState.hpp"

namespace Astrea::System {

QString systemServiceStateName(SystemServiceState state)
{
    switch (state) {
    case SystemServiceState::Stopped: return QStringLiteral("stopped");
    case SystemServiceState::Starting: return QStringLiteral("starting");
    case SystemServiceState::Ready: return QStringLiteral("ready");
    case SystemServiceState::Unavailable: return QStringLiteral("unavailable");
    case SystemServiceState::Degraded: return QStringLiteral("degraded");
    }
    return QStringLiteral("unknown");
}

QJsonObject serviceHealthJson(SystemServiceState state, bool available, bool ready,
                              const QString &errorString)
{
    QJsonObject result{
        {QStringLiteral("state"), systemServiceStateName(state)},
        {QStringLiteral("available"), available},
        {QStringLiteral("ready"), ready},
    };
    if (!errorString.isEmpty())
        result.insert(QStringLiteral("error"), errorString);
    return result;
}

} // namespace Astrea::System
