#include "services/network/SettingsFirewallManager.hpp"

#include <QHostAddress>
#include <QRegularExpression>
#include <QProcessEnvironment>
#include <QFileInfo>

namespace {
constexpr auto kPkexec = "/usr/bin/pkexec";
constexpr auto kUfw = "/usr/bin/ufw";

QString normalizedSource(const QString &source)
{
    const QString value = source.trimmed();
    if (value.isEmpty() || value.compare(QStringLiteral("any"), Qt::CaseInsensitive) == 0
        || value.compare(QStringLiteral("Anywhere"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("any");

    const QStringList parts = value.split('/');
    if (parts.size() > 2 || parts.isEmpty())
        return {};
    QHostAddress address;
    if (!address.setAddress(parts.first()))
        return {};
    if (parts.size() == 1)
        return address.toString();

    bool ok = false;
    const int prefix = parts[1].toInt(&ok);
    const int maxPrefix = address.protocol() == QAbstractSocket::IPv4Protocol ? 32 : 128;
    if (!ok || prefix < 0 || prefix > maxPrefix || parts[1].isEmpty())
        return {};
    return address.toString() + QStringLiteral("/") + QString::number(prefix);
}

QVariantMap ruleMap(const QString &target, const QString &action, const QString &source)
{
    QVariantMap out{{QStringLiteral("target"), target},
                    {QStringLiteral("action"), action},
                    {QStringLiteral("source"), source},
                    {QStringLiteral("removable"), false}};
    const QRegularExpression portRegex(QStringLiteral(R"(^(\d{1,5})/(tcp|udp)(?: \(v6\))?$)"));
    const auto m = portRegex.match(target);
    const QString actionLower = action.toLower();
    const QString normalized = normalizedSource(source.endsWith(QStringLiteral(" (v6)"))
        ? source.left(source.size() - 5) : source);
    if (!m.hasMatch() || normalized.isEmpty()
        || (actionLower != QStringLiteral("allow") && actionLower != QStringLiteral("deny")))
        return out;
    const int port = m.captured(1).toInt();
    if (port < 1 || port > 65535)
        return out;
    out.insert(QStringLiteral("port"), port);
    out.insert(QStringLiteral("protocol"), m.captured(2));
    out.insert(QStringLiteral("sourceSpec"), normalized);
    out.insert(QStringLiteral("removable"), true);
    return out;
}
} // namespace

bool SettingsFirewallManager::knownGraphicalAgentInstalled() const
{
    // This is an advisory check for common desktop agents, not an authoritative
    // query of polkit's internal registered-agent state. Never block custom agents.
    static const char *const agents[] = {
        "/usr/bin/lxqt-policykit-agent",
        "/usr/lib/polkit-kde-authentication-agent-1",
        "/usr/libexec/polkit-kde-authentication-agent-1",
        "/usr/lib/polkit-gnome/polkit-gnome-authentication-agent-1",
        "/usr/libexec/polkit-gnome-authentication-agent-1",
        "/usr/bin/hyprpolkitagent",
        "/usr/libexec/xfce-polkit",
        "/usr/libexec/polkit-mate-authentication-agent-1",
    };
    for (const char *agent : agents) {
        if (QFileInfo(QString::fromLatin1(agent)).isExecutable())
            return true;
    }
    return false;
}

SettingsFirewallManager::SettingsFirewallManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &SettingsFirewallManager::finishProcess);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            failure(QStringLiteral("Administrative authorization could not be started."));
        }
    });
}

std::optional<QStringList> SettingsFirewallManager::policyCommand(
    const QString &direction, const QString &policy)
{
    if (direction != QStringLiteral("incoming") && direction != QStringLiteral("outgoing"))
        return std::nullopt;
    if (policy != QStringLiteral("allow") && policy != QStringLiteral("deny"))
        return std::nullopt;
    return QStringList{QStringLiteral("default"), policy, direction};
}

std::optional<QStringList> SettingsFirewallManager::ruleCommand(
    bool remove, const QString &action, int port, const QString &protocol,
    const QString &source)
{
    if (action != QStringLiteral("allow") && action != QStringLiteral("deny"))
        return std::nullopt;
    if (port < 1 || port > 65535
        || (protocol != QStringLiteral("tcp") && protocol != QStringLiteral("udp")))
        return std::nullopt;
    const QString normalized = normalizedSource(source);
    if (normalized.isEmpty())
        return std::nullopt;

    QStringList command;
    if (remove)
        command.append({QStringLiteral("--force"), QStringLiteral("delete")});
    command.append(action);
    if (normalized == QStringLiteral("any")) {
        command.append(QString::number(port) + QStringLiteral("/") + protocol);
    } else {
        command.append({QStringLiteral("from"), normalized, QStringLiteral("to"),
                        QStringLiteral("any"), QStringLiteral("port"),
                        QString::number(port), QStringLiteral("proto"), protocol});
    }
    return command;
}

SettingsFirewallManager::Snapshot SettingsFirewallManager::parseStatus(const QString &output)
{
    Snapshot result;
    const QStringList lines = output.split('\n');
    const QRegularExpression statusRe(QStringLiteral(R"(^Status:\s+(active|inactive)\s*$)"),
                                      QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression policyRe(QStringLiteral(
        R"(^Default:\s+(allow|deny)\s+\(incoming\),\s+(allow|deny)\s+\(outgoing\))"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression loggingRe(QStringLiteral(R"(^Logging:\s+(on|off)\b)"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression ruleRe(QStringLiteral(
        R"(^(.+?)\s{2,}(ALLOW|DENY|LIMIT|REJECT) IN\s{2,}(.+)$)"),
        QRegularExpression::CaseInsensitiveOption);

    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (auto match = statusRe.match(line); match.hasMatch()) {
            result.valid = true;
            result.active = match.captured(1).toLower() == QStringLiteral("active");
        }
        if (auto match = policyRe.match(line); match.hasMatch()) {
            result.incoming = match.captured(1).toLower();
            result.outgoing = match.captured(2).toLower();
        }
        if (auto match = loggingRe.match(line); match.hasMatch())
            result.logging = match.captured(1).toLower() == QStringLiteral("on");
        if (auto match = ruleRe.match(line); match.hasMatch()) {
            const QString target = match.captured(1).trimmed();
            const QString action = match.captured(2).toLower();
            const QString source = match.captured(3).trimmed();
            const QVariantMap candidate = ruleMap(target, action, source);
            // UFW prints separate IPv4 and IPv6 entries for a single generic
            // port rule. Only show one delete action for that paired rule.
            bool duplicate = false;
            for (const QVariant &existingEntry : result.rules) {
                const QVariantMap existing = existingEntry.toMap();
                if (candidate.value(QStringLiteral("removable")).toBool()
                    && existing.value(QStringLiteral("removable")).toBool()
                    && existing.value(QStringLiteral("action")) == candidate.value(QStringLiteral("action"))
                    && existing.value(QStringLiteral("port")) == candidate.value(QStringLiteral("port"))
                    && existing.value(QStringLiteral("protocol")) == candidate.value(QStringLiteral("protocol"))
                    && existing.value(QStringLiteral("sourceSpec")) == candidate.value(QStringLiteral("sourceSpec"))) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate)
                result.rules.append(candidate);
        }
    }
    return result;
}

void SettingsFirewallManager::failure(const QString &reason)
{
    m_busy = false;
    m_loaded = false; // A failed read or mutation must not leave stale controls actionable.
    m_error = reason.left(320);
    emit changed();
}

bool SettingsFirewallManager::execute(QStringList arguments, bool mutation)
{
    if (m_busy)
        return false;
    if (!QFileInfo::exists(QString::fromLatin1(kPkexec))
        || !QFileInfo::exists(QString::fromLatin1(kUfw))) {
        failure(QStringLiteral("UFW or Polkit is not installed."));
        return false;
    }
    m_busy = true;
    m_error.clear();
    m_mutation = mutation;
    emit changed();

    arguments.prepend(QString::fromLatin1(kUfw));
    m_process.setProgram(QString::fromLatin1(kPkexec));
    m_process.setArguments(arguments);
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    m_process.setProcessEnvironment(env);
    m_process.start();

    const auto generation = ++m_generation;
    QTimer::singleShot(60000, this, [this, generation] {
        if (generation == m_generation && m_busy && m_process.state() != QProcess::NotRunning) {
            m_process.kill();
            failure(QStringLiteral("Authorization or firewall operation timed out."));
        }
    });
    return true;
}

void SettingsFirewallManager::finishProcess(int exitCode, QProcess::ExitStatus exitStatus)
{
    const QString stdoutText = QString::fromUtf8(m_process.readAllStandardOutput());
    const QString stderrText = QString::fromUtf8(m_process.readAllStandardError()).trimmed();
    if (!m_busy)
        return; // Timed out, already reported.
    const bool mutation = m_mutation;
    m_busy = false;
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        // A failed pkexec request may mean a cancelled prompt, a missing
        // authentication agent, or a policy denial. Never claim which without
        // stronger evidence; surface the actual error with a useful next step.
        const QString detail = stderrText.isEmpty()
            ? QStringLiteral("Authorization was cancelled or the request was denied.")
            : stderrText;
        failure(!knownGraphicalAgentInstalled()
            ? QStringLiteral("No common graphical Polkit agent is installed. Install and start "
                             "one in your desktop session (e.g. lxqt-policykit). %1").arg(detail)
            : QStringLiteral("%1 Check that your Polkit authentication agent is running.")
                .arg(detail));
        return;
    }
    if (mutation) {
        ++m_generation;
        m_loaded = false; // Never leave stale values actionable during verification.
        emit changed();
        refresh(); // Re-read authoritative state after each completed mutation.
        return;
    }
    const Snapshot state = parseStatus(stdoutText);
    if (!state.valid) {
        failure(QStringLiteral("UFW returned an unrecognized status response."));
        return;
    }
    m_loaded = true;
    m_enabled = state.active;
    m_logging = state.logging;
    m_incoming = state.incoming;
    m_outgoing = state.outgoing;
    m_rules = state.rules;
    m_error.clear();
    emit changed();
}

bool SettingsFirewallManager::refresh()
{
    return execute({QStringLiteral("status"), QStringLiteral("verbose")}, false);
}

bool SettingsFirewallManager::setEnabled(bool enabled)
{
    if (!m_loaded)
        return false;
    return execute({QStringLiteral("--force"), enabled ? QStringLiteral("enable")
                                                     : QStringLiteral("disable")}, true);
}

bool SettingsFirewallManager::setDefaultPolicy(const QString &direction, const QString &policy)
{
    if (!m_loaded)
        return false;
    const auto command = policyCommand(direction, policy);
    return command ? execute(*command, true) : false;
}

bool SettingsFirewallManager::setLogging(bool enabled)
{
    if (!m_loaded)
        return false;
    return execute({QStringLiteral("logging"),
                    enabled ? QStringLiteral("on") : QStringLiteral("off")}, true);
}

bool SettingsFirewallManager::addRule(const QString &action, int port,
                                      const QString &protocol, const QString &source)
{
    if (!m_loaded)
        return false;
    const auto command = ruleCommand(false, action, port, protocol, source);
    return command ? execute(*command, true) : false;
}

bool SettingsFirewallManager::removeRule(const QString &action, int port,
                                         const QString &protocol, const QString &source)
{
    if (!m_loaded)
        return false;
    const auto command = ruleCommand(true, action, port, protocol, source);
    if (!command)
        return false;
    // The rule must exist in the last authenticated snapshot.
    for (const QVariant &entry : m_rules) {
        const auto rule = entry.toMap();
        if (rule.value(QStringLiteral("removable")).toBool()
            && rule.value(QStringLiteral("action")).toString() == action
            && rule.value(QStringLiteral("port")).toInt() == port
            && rule.value(QStringLiteral("protocol")).toString() == protocol
            && rule.value(QStringLiteral("sourceSpec")).toString() == normalizedSource(source))
            return execute(*command, true);
    }
    return false;
}
