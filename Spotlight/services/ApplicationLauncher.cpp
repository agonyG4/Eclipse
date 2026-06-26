#include "services/ApplicationLauncher.hpp"
#include <QJsonArray>
#include <QJsonDocument>

static QStringList tokenizeDesktopExec(const QString &exec)
{
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
                current.clear();
            }
            continue;
        }
        current.append(ch);
    }

    if (!current.isEmpty())
        tokens.append(current);
    return tokens;
}

static QStringList expandDesktopExec(const QString &exec, const QString &appName,
                                    const QString &iconName, const QString &desktopFilePath)
{
    QStringList argv;
    const QStringList tokens = tokenizeDesktopExec(exec);
    for (const QString &token : tokens) {
        QString expanded;
        for (int i = 0; i < token.size(); ++i) {
            const QChar ch = token.at(i);
            if (ch == QLatin1Char('%') && i + 1 < token.size()) {
                const QChar code = token.at(++i);
                switch (code.unicode()) {
                case '%': expanded += QLatin1Char('%'); break;
                case 'i':
                    if (!iconName.isEmpty()) {
                        argv << QStringLiteral("--icon") << iconName;
                    }
                    break;
                case 'c':
                    expanded += appName;
                    break;
                case 'k':
                    expanded += desktopFilePath;
                    break;
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

ApplicationLauncher::ApplicationLauncher(const QString &astreaLaunchPath, QObject *parent)
    : QObject(parent), m_launchPath(astreaLaunchPath) {}

bool ApplicationLauncher::isRunning() const {
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

void ApplicationLauncher::runSupervised(const QString &desktopId, const QStringList &args) {
    m_currentDesktopId = desktopId;
    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(10000);
    m_proc = new QProcess(this);

    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        m_timeout->stop();
        QString msg;
        switch (err) {
        case QProcess::FailedToStart: msg = QStringLiteral("Failed to start astrea-launch"); break;
        case QProcess::Crashed: msg = QStringLiteral("astrea-launch crashed"); break;
        case QProcess::Timedout: msg = QStringLiteral("astrea-launch timed out"); break;
        default: msg = QStringLiteral("Launch process error"); break;
        }
        emit launchFailed(m_currentDesktopId, msg);
        cleanupProc();
    });

    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
        m_timeout->stop();
        if (status == QProcess::NormalExit && exitCode == 0) {
            emit launchSucceeded(m_currentDesktopId);
        } else {
            QString err = QString::fromUtf8(m_proc->readAllStandardError());
            emit launchFailed(m_currentDesktopId, err.isEmpty()
                ? QStringLiteral("exit code %1").arg(exitCode) : err);
        }
        cleanupProc();
    });

    connect(m_timeout, &QTimer::timeout, this, [this] {
        if (m_proc) m_proc->kill();
        emit launchTimedOut(m_currentDesktopId);
        cleanupProc();
    });

    m_proc->setProgram(m_launchPath);
    m_proc->setArguments(args);
    m_proc->start();
    m_timeout->start();
}

void ApplicationLauncher::launchDesktop(const QString &desktopId,
                                          const QString &desktopFileName,
                                          const QString &exec,
                                          const QString &appName,
                                          const QString &iconName,
                                          const QString &desktopFilePath) {
    if (isRunning()) {
        emit launchFailed(desktopId, QStringLiteral("A launch is already in progress"));
        return;
    }

    cleanupProc();

    if (!desktopId.isEmpty()) {
        runSupervised(desktopId, {QStringLiteral("--desktop"), desktopId});
    } else if (!desktopFileName.isEmpty()) {
        runSupervised(desktopFileName, {QStringLiteral("--desktop"), desktopFileName});
    } else if (!exec.isEmpty()) {
        const QStringList argv = expandDesktopExec(exec, appName, iconName, desktopFilePath);
        if (argv.isEmpty()) {
            emit launchFailed(exec, QStringLiteral("Empty desktop Exec command"));
            return;
        }
        QJsonArray argvJson;
        for (const QString &arg : argv)
            argvJson.append(arg);
        QJsonDocument doc(argvJson);
        runSupervised(exec, {QStringLiteral("--argv-json"), QString::fromUtf8(doc.toJson(QJsonDocument::Compact))});
    } else {
        emit launchFailed(desktopId, QStringLiteral("No desktop ID available"));
    }
}

void ApplicationLauncher::cleanupProc() {
    m_currentDesktopId.clear();
    if (m_proc) {
        disconnect(m_proc, nullptr, this, nullptr);
        disconnect(m_timeout, nullptr, this, nullptr);
        m_proc->deleteLater();
        m_proc = nullptr;
        m_timeout->deleteLater();
        m_timeout = nullptr;
    }
}
