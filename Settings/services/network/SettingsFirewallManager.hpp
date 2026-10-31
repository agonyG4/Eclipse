#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QProcess>
#include <QTimer>

#include <optional>

// Privileged UFW operations run through pkexec with fixed executable/argv.
// QML never executes commands; no shell interpolation occurs.
class SettingsFirewallManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(bool loggingEnabled READ loggingEnabled NOTIFY changed)
    Q_PROPERTY(QString incomingPolicy READ incomingPolicy NOTIFY changed)
    Q_PROPERTY(QString outgoingPolicy READ outgoingPolicy NOTIFY changed)
    Q_PROPERTY(QVariantList rules READ rules NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(bool knownGraphicalAgentInstalled READ knownGraphicalAgentInstalled CONSTANT)

public:
    struct Rule {
        QString target;
        QString action;
        QString source;
        int port = 0;
        QString protocol;
        bool removable = false;
    };
    struct Snapshot {
        bool valid = false;
        bool active = false;
        bool logging = false;
        QString incoming;
        QString outgoing;
        QVariantList rules;
    };

    explicit SettingsFirewallManager(QObject *parent = nullptr);
    bool busy() const { return m_busy; }
    bool loaded() const { return m_loaded; }
    bool enabled() const { return m_enabled; }
    bool loggingEnabled() const { return m_logging; }
    QString incomingPolicy() const { return m_incoming; }
    QString outgoingPolicy() const { return m_outgoing; }
    QVariantList rules() const { return m_rules; }
    QString errorText() const { return m_error; }
    bool knownGraphicalAgentInstalled() const;

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool setEnabled(bool enabled);
    Q_INVOKABLE bool setDefaultPolicy(const QString &direction, const QString &policy);
    Q_INVOKABLE bool setLogging(bool enabled);
    Q_INVOKABLE bool addRule(const QString &action, int port, const QString &protocol,
                             const QString &source);
    Q_INVOKABLE bool removeRule(const QString &action, int port, const QString &protocol,
                                const QString &source);

    static std::optional<QStringList> policyCommand(const QString &direction,
                                                     const QString &policy);
    static std::optional<QStringList> ruleCommand(bool remove, const QString &action, int port,
                                                   const QString &protocol,
                                                   const QString &source);
    static Snapshot parseStatus(const QString &output);

signals:
    void changed();

private:
    bool execute(QStringList arguments, bool mutation);
    void finishProcess(int exitCode, QProcess::ExitStatus exitStatus);
    void failure(const QString &reason);

    QProcess m_process;
    bool m_busy = false;
    bool m_loaded = false;
    bool m_enabled = false;
    bool m_logging = false;
    bool m_mutation = false;
    QString m_incoming;
    QString m_outgoing;
    QVariantList m_rules;
    QString m_error;
    quint64 m_generation = 0;
};
