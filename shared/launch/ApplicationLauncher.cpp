#include "launch/ApplicationLauncher.hpp"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcessEnvironment>

#include <utility>

namespace {

bool usesTyphonShellControlBridge(const QProcessEnvironment &environment)
{
    const bool isTyphon = environment.value(QStringLiteral("ASTREA_COMPOSITOR"))
                              .compare(QStringLiteral("TYPHON"), Qt::CaseInsensitive)
            == 0
        || environment.value(QStringLiteral("ASTREA_COMPOSITOR_BACKEND"))
                   .compare(QStringLiteral("typhon"), Qt::CaseInsensitive)
            == 0;
    return isTyphon && !environment.value(QStringLiteral("ASTREA_SHELL_CONTROL_BRIDGE")).isEmpty();
}

bool synchronizeSystemdActivationEnvironment(QString *errorOut)
{
    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    QStringList arguments{QStringLiteral("--systemd")};
    for (const QString &name : {QStringLiteral("WAYLAND_DISPLAY"),
                                QStringLiteral("XDG_RUNTIME_DIR")}) {
        if (environment.contains(name))
            arguments.append(name + QLatin1Char('=') + environment.value(name));
    }
    if (arguments.size() == 1)
        return true;

    QProcess process;
    process.start(QStringLiteral("dbus-update-activation-environment"), arguments);
    if (!process.waitForStarted(5000)) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to start activation environment updater: %1")
                            .arg(process.errorString());
        return false;
    }
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished();
        if (errorOut)
            *errorOut = QStringLiteral("Timed out updating systemd activation environment");
        return false;
    }

    const QString standardError = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorOut) {
            *errorOut = standardError.isEmpty()
                ? QStringLiteral("Activation environment updater exited with code %1")
                      .arg(process.exitCode())
                : standardError;
        }
        return false;
    }
    return true;
}

bool validDesktopFileName(const QString &fileName)
{
    return !fileName.isEmpty() && fileName.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)
        && !fileName.contains(QLatin1Char('/'))
        && !fileName.contains(QLatin1Char('\\'))
        && !fileName.contains(QChar::Null)
        && !fileName.contains(QStringLiteral(".."));
}

QString launchIdentity(const ApplicationLaunchRequest &request)
{
    if (validDesktopFileName(request.desktopFileName))
        return request.desktopFileName;
    if (!request.desktopId.isEmpty())
        return request.desktopId;
    return request.exec;
}

QStringList tokenizeDesktopExec(const QString &exec, QString *errorOut)
{
    if (exec.size() > 4096) {
        if (errorOut)
            *errorOut = QStringLiteral("Desktop Exec command is too long");
        return {};
    }
    QStringList tokens;
    QString current;
    bool inSingle = false;
    bool inDouble = false;
    bool escaping = false;

    for (const QChar ch : exec) {
        if (escaping) {
            current.append(ch);
            escaping = false;
            continue;
        }
        if (ch == QLatin1Char('\\')) {
            escaping = true;
            continue;
        }
        if (ch == QLatin1Char('\'') && !inDouble) {
            inSingle = !inSingle;
            continue;
        }
        if (ch == QLatin1Char('"') && !inSingle) {
            inDouble = !inDouble;
            continue;
        }
        if (ch.isSpace() && !inSingle && !inDouble) {
            if (!current.isEmpty()) {
                tokens.append(current);
                if (tokens.size() > 128) {
                    if (errorOut)
                        *errorOut = QStringLiteral("Desktop Exec command has too many arguments");
                    return {};
                }
                current.clear();
            }
            continue;
        }
        current.append(ch);
        if (current.size() > 1024) {
            if (errorOut)
                *errorOut = QStringLiteral("Desktop Exec argument is too long");
            return {};
        }
    }

    if (escaping || inSingle || inDouble) {
        if (errorOut)
            *errorOut = QStringLiteral("Malformed quoting in Desktop Exec command");
        return {};
    }
    if (!current.isEmpty()) {
        tokens.append(current);
        if (tokens.size() > 128) {
            if (errorOut)
                *errorOut = QStringLiteral("Desktop Exec command has too many arguments");
            return {};
        }
    }
    return tokens;
}

QStringList expandDesktopExecInternal(const ApplicationLaunchRequest &request, QString *errorOut)
{
    QStringList argv;
    for (const QString &token : tokenizeDesktopExec(request.exec, errorOut)) {
        QString expanded;
        for (int i = 0; i < token.size(); ++i) {
            const QChar ch = token.at(i);
            if (ch == QLatin1Char('%') && i + 1 < token.size()) {
                const QChar code = token.at(++i);
                switch (code.unicode()) {
                case '%': expanded += QLatin1Char('%'); break;
                case 'i':
                    if (!request.iconName.isEmpty())
                        argv << QStringLiteral("--icon") << request.iconName;
                    break;
                case 'c': expanded += request.appName; break;
                case 'k': expanded += request.desktopFilePath; break;
                case 'f':
                case 'F':
                case 'u':
                case 'U':
                    break;
                default:
                    expanded += QLatin1Char('%');
                    expanded += code;
                    break;
                }
            } else {
                expanded += ch;
            }
        }
        if (!expanded.isEmpty())
            argv << expanded;
    }
    return argv;
}

} // namespace

ApplicationLauncher::ApplicationLauncher(const QString &astreaLaunchPath, QObject *parent)
    : QObject(parent)
    , m_launchPath(resolveLauncherPath(astreaLaunchPath,
                                       QProcessEnvironment::systemEnvironment()))
    , m_needsSystemdActivationEnvironmentSync(
          !usesTyphonShellControlBridge(QProcessEnvironment::systemEnvironment()))
{
}

QStringList ApplicationLauncher::expandDesktopExec(const ApplicationLaunchRequest &request,
                                                    QString *errorOut)
{
    if (errorOut)
        errorOut->clear();
    return expandDesktopExecInternal(request, errorOut);
}

QString ApplicationLauncher::resolveLauncherPath(const QString &fallbackPath,
                                                  const QProcessEnvironment &environment)
{
    if (usesTyphonShellControlBridge(environment))
        return environment.value(QStringLiteral("ASTREA_SHELL_CONTROL_BRIDGE"));
    return fallbackPath;
}

bool ApplicationLauncher::isRunning() const
{
    return !m_pending.isEmpty();
}

void ApplicationLauncher::launchDesktop(const QString &desktopId, const QString &desktopFileName,
                                         const QString &exec, const QString &appName,
                                         const QString &iconName, const QString &desktopFilePath)
{
    launchDesktop(ApplicationLaunchRequest{desktopId, desktopFileName, exec, appName,
                                           iconName, desktopFilePath});
}

void ApplicationLauncher::launchDesktop(const ApplicationLaunchRequest &request)
{
    const QString identity = launchIdentity(request);
    if (validDesktopFileName(request.desktopFileName)) {
        runSupervised(identity, {QStringLiteral("--desktop"), request.desktopFileName});
        return;
    }
    if (!request.desktopId.isEmpty()) {
        runSupervised(identity, {QStringLiteral("--desktop"), request.desktopId});
        return;
    }
    if (!request.exec.isEmpty()) {
        QString expansionError;
        const QStringList argv = expandDesktopExec(request, &expansionError);
        if (argv.isEmpty()) {
            emit launchFailed(request.exec, expansionError.isEmpty()
                                           ? QStringLiteral("Empty desktop Exec command")
                                           : expansionError);
            emit launchCompleted(request.exec, false);
            return;
        }
        QJsonArray argvJson;
        for (const QString &arg : argv)
            argvJson.append(arg);
        runSupervised(identity,
                      {QStringLiteral("--argv-json"),
                       QString::fromUtf8(QJsonDocument(argvJson).toJson(QJsonDocument::Compact))});
        return;
    }

    emit launchFailed(identity, QStringLiteral("No launch target available"));
    emit launchCompleted(identity, false);
}

void ApplicationLauncher::runSupervised(const QString &desktopId, const QStringList &args)
{
    if (m_needsSystemdActivationEnvironmentSync) {
        QString error;
        if (!synchronizeSystemdActivationEnvironment(&error)) {
            emit launchFailed(desktopId, error);
            emit launchCompleted(desktopId, false);
            return;
        }
    }

    auto *process = new QProcess(this);
    auto *timeout = new QTimer(this);
    timeout->setSingleShot(true);
    timeout->setInterval(kTimeoutMs);
    m_pending.insert(process, PendingLaunch{desktopId, process, timeout});

    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (!m_pending.contains(process))
            return;
        QString message;
        switch (error) {
        case QProcess::FailedToStart: message = QStringLiteral("Failed to start launch helper"); break;
        case QProcess::Crashed: message = QStringLiteral("Launch helper crashed"); break;
        case QProcess::Timedout: message = QStringLiteral("Launch helper timed out"); break;
        default: message = QStringLiteral("Launch process error"); break;
        }
        complete(process, false, message);
    });
    connect(process, &QProcess::finished, this,
            [this, process](int exitCode, QProcess::ExitStatus status) {
        if (!m_pending.contains(process))
            return;
        if (status == QProcess::NormalExit && exitCode == 0) {
            complete(process, true);
        } else {
            const QString stderr = QString::fromUtf8(process->readAllStandardError()).trimmed();
            complete(process, false, stderr.isEmpty()
                                          ? QStringLiteral("exit code %1").arg(exitCode)
                                          : stderr);
        }
    });
    connect(timeout, &QTimer::timeout, this, [this, process] {
        if (!m_pending.contains(process))
            return;
        process->kill();
        complete(process, false, QStringLiteral("Launch timed out"), true);
    });

    emit launchAccepted(desktopId);
    process->setProgram(m_launchPath);
    process->setArguments(args);
    process->start();
    timeout->start();
}

void ApplicationLauncher::complete(QProcess *process, bool success, const QString &error, bool timedOut)
{
    const auto it = m_pending.find(process);
    if (it == m_pending.end())
        return;

    const PendingLaunch pending = std::move(it.value());
    m_pending.erase(it);
    pending.timeout->stop();
    disconnect(process, nullptr, this, nullptr);
    disconnect(pending.timeout, nullptr, this, nullptr);

    if (timedOut)
        emit launchTimedOut(pending.desktopId);
    else if (success)
        emit launchSucceeded(pending.desktopId);
    else
        emit launchFailed(pending.desktopId, error);
    emit launchCompleted(pending.desktopId, success);

    process->deleteLater();
    pending.timeout->deleteLater();
}
