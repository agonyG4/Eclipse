#include "services/appidentity/ProcessInspector.hpp"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

ProcessInspector::ProcInfo ProcessInspector::inspectProcess(qint64 pid, const QString &procRoot) {
    ProcInfo info;
    info.pid = pid;

    if (pid <= 0)
        return info;

    QString pidDir = procRoot + QLatin1Char('/') + QString::number(pid);
    if (!QDir(pidDir).exists())
        return info;

    // 1. PPid from status
    QFile statusFile(pidDir + QStringLiteral("/status"));
    if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream ts(&statusFile);
        while (!ts.atEnd()) {
            QString line = ts.readLine();
            if (line.startsWith(QStringLiteral("PPid:"))) {
                info.ppid = line.mid(5).trimmed().toLongLong();
                break;
            }
        }
        statusFile.close();
    }

    // 2. Cmdline
    QFile cmdlineFile(pidDir + QStringLiteral("/cmdline"));
    if (cmdlineFile.open(QIODevice::ReadOnly)) {
        QByteArray data = cmdlineFile.read(4096); // Bounded size
        cmdlineFile.close();
        if (!data.isEmpty()) {
            // Replace null bytes with space
            QByteArray clean = data;
            clean.replace('\0', ' ');
            info.cmdline = QString::fromUtf8(clean).trimmed();
        }
    }

    // 3. Env
    QFile envFile(pidDir + QStringLiteral("/environ"));
    if (envFile.open(QIODevice::ReadOnly)) {
        QByteArray data = envFile.read(32768); // Bounded size
        envFile.close();
        int start = 0;
        for (int i = 0; i < data.size(); ++i) {
            if (data.at(i) == '\0') {
                QByteArray entry = data.mid(start, i - start);
                start = i + 1;
                if (!entry.isEmpty()) {
                    int eq = entry.indexOf('=');
                    if (eq > 0) {
                        QString key = QString::fromUtf8(entry.left(eq));
                        QString val = QString::fromUtf8(entry.mid(eq + 1));
                        info.env.insert(key, val);
                    }
                }
            }
        }
    }

    // 4. Cwd symlink target
    QFileInfo cwdLink(pidDir + QStringLiteral("/cwd"));
    if (cwdLink.exists() && cwdLink.isSymLink()) {
        info.cwd = cwdLink.symLinkTarget();
    }

    return info;
}

QVector<qint64> ProcessInspector::getAncestors(qint64 pid, int maxDepth, const QString &procRoot) {
    QVector<qint64> list;
    qint64 current = pid;
    for (int i = 0; i < maxDepth; ++i) {
        ProcInfo info = inspectProcess(current, procRoot);
        if (info.ppid <= 0 || info.ppid == current)
            break;
        list.append(info.ppid);
        current = info.ppid;
    }
    return list;
}
