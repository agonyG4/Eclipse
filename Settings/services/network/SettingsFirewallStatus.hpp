#pragma once

#include <QObject>
#include <QTimer>

// Read-only systemd service state of ufw.service, not a firewall ruleset audit.
class SettingsFirewallStatus final : public QObject {
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool known READ known NOTIFY stateChanged)
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)

public:
    enum State { Unknown, Active, Inactive };
    Q_ENUM(State)
    explicit SettingsFirewallStatus(QObject *parent = nullptr);

    State state() const { return m_state; }
    bool known() const { return m_state != Unknown; }
    bool active() const { return m_state == Active; }
    Q_INVOKABLE void refresh();
    static State classifyUnitState(const QString &activeState);

signals:
    void stateChanged();

private:
    void setState(State state);
    State m_state = Unknown;
    QTimer m_refreshTimer;
    quint64 m_generation = 0;
};
