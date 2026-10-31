#include "services/network/SettingsFirewallStatus.hpp"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

namespace {
constexpr auto kService = "org.freedesktop.systemd1";
constexpr auto kManagerPath = "/org/freedesktop/systemd1";
constexpr auto kManagerInterface = "org.freedesktop.systemd1.Manager";
constexpr auto kUnitInterface = "org.freedesktop.systemd1.Unit";
constexpr auto kPropertiesInterface = "org.freedesktop.DBus.Properties";
}

SettingsFirewallStatus::SettingsFirewallStatus(QObject *parent)
    : QObject(parent)
{
    m_refreshTimer.setInterval(15000);
    connect(&m_refreshTimer, &QTimer::timeout, this, &SettingsFirewallStatus::refresh);
    m_refreshTimer.start();
    QTimer::singleShot(0, this, &SettingsFirewallStatus::refresh);
}

SettingsFirewallStatus::State SettingsFirewallStatus::classifyUnitState(
    const QString &activeState)
{
    if (activeState == QStringLiteral("active"))
        return Active;
    if (activeState == QStringLiteral("inactive") || activeState == QStringLiteral("failed"))
        return Inactive;
    return Unknown;
}

void SettingsFirewallStatus::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void SettingsFirewallStatus::refresh()
{
    const quint64 generation = ++m_generation;
    const QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        setState(Unknown);
        return;
    }

    QDBusInterface manager(QString::fromLatin1(kService),
                           QString::fromLatin1(kManagerPath),
                           QString::fromLatin1(kManagerInterface), bus);
    if (!manager.isValid()) {
        setState(Unknown);
        return;
    }

    auto *unitWatcher = new QDBusPendingCallWatcher(
        manager.asyncCall(QStringLiteral("GetUnit"), QStringLiteral("ufw.service")), this);
    connect(unitWatcher, &QDBusPendingCallWatcher::finished, this,
            [this, bus, generation](QDBusPendingCallWatcher *finished) {
        QDBusPendingReply<QDBusObjectPath> unitReply = *finished;
        finished->deleteLater();
        if (generation != m_generation)
            return;
        if (unitReply.isError() || unitReply.value().path().isEmpty()) {
            setState(Unknown);
            return;
        }
        QDBusInterface properties(QString::fromLatin1(kService), unitReply.value().path(),
                                  QString::fromLatin1(kPropertiesInterface), bus);
        if (!properties.isValid()) {
            setState(Unknown);
            return;
        }
        auto *stateWatcher = new QDBusPendingCallWatcher(
            properties.asyncCall(QStringLiteral("Get"),
                                 QString::fromLatin1(kUnitInterface),
                                 QStringLiteral("ActiveState")), this);
        connect(stateWatcher, &QDBusPendingCallWatcher::finished, this,
                [this, generation](QDBusPendingCallWatcher *completed) {
            QDBusPendingReply<QDBusVariant> reply = *completed;
            completed->deleteLater();
            if (generation != m_generation)
                return;
            setState(reply.isError() ? Unknown
                     : classifyUnitState(reply.value().variant().toString()));
        });
    });
}
