#pragma once

#include "platform/typhon/TyphonScreenCaptureClient.hpp"
#include "platform/typhon/TyphonShortcutClient.hpp"
#include "screenshot/ScreenshotController.hpp"

#include <QJsonObject>
#include <QVariantMap>

namespace ShellRuntimeStatus {

inline QString shortcutConnectionStateName(TyphonShortcutConnectionState state)
{
    switch (state) {
    case TyphonShortcutConnectionState::Stopped:
        return QStringLiteral("stopped");
    case TyphonShortcutConnectionState::Connecting:
        return QStringLiteral("connecting");
    case TyphonShortcutConnectionState::WaitingForManager:
        return QStringLiteral("waiting-for-manager");
    case TyphonShortcutConnectionState::Registering:
        return QStringLiteral("registering");
    case TyphonShortcutConnectionState::Ready:
        return QStringLiteral("ready");
    case TyphonShortcutConnectionState::Degraded:
        return QStringLiteral("degraded");
    case TyphonShortcutConnectionState::Disconnected:
        return QStringLiteral("disconnected");
    case TyphonShortcutConnectionState::Unsupported:
        return QStringLiteral("unsupported");
    }
    return QStringLiteral("unknown");
}

inline QJsonObject screenshot(const ScreenshotController &controller,
                              const TyphonScreenCaptureClient &captureClient)
{
    return {
        {QStringLiteral("phase"), controller.phaseName()},
        {QStringLiteral("intent"), controller.intentName()},
        {QStringLiteral("pending"), controller.hasPendingOperation()},
        {QStringLiteral("deadlineRemainingMs"), controller.operationDeadlineRemainingMs()},
        {QStringLiteral("captureClientReady"), captureClient.isReady()},
        {QStringLiteral("lastFailureStage"), controller.lastFailureStage()},
        {QStringLiteral("lastFailureReason"), controller.lastFailureReason()},
        {QStringLiteral("rejectedRequests"),
         static_cast<qint64>(controller.rejectedOperationCount())},
    };
}

inline QJsonObject shortcuts(const TyphonShortcutClient &client)
{
    QJsonObject registrationHealth;
    const QVariantMap registrations = client.registrationHealth();
    for (auto it = registrations.cbegin(); it != registrations.cend(); ++it)
        registrationHealth.insert(it.key(), QJsonObject::fromVariantMap(it.value().toMap()));
    return {
        {QStringLiteral("connectionState"), shortcutConnectionStateName(client.state())},
        {QStringLiteral("ready"), client.isReady()},
        {QStringLiteral("registeredCount"), client.registeredShortcutCount()},
        {QStringLiteral("connectionGeneration"),
         static_cast<double>(client.connectionGeneration())},
        {QStringLiteral("registrations"), registrationHealth},
    };
}

} // namespace ShellRuntimeStatus
