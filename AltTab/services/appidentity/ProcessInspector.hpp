#pragma once
#include <QString>
#include <QHash>
#include <QVector>

class ProcessInspector {
public:
    struct ProcInfo {
        qint64 pid = 0;
        qint64 ppid = 0;
        QString cmdline;
        QString cwd;
        QHash<QString, QString> env;
    };

    static ProcInfo inspectProcess(qint64 pid, const QString &procRoot = QStringLiteral("/proc"));
    static QVector<qint64> getAncestors(qint64 pid, int maxDepth = 5, const QString &procRoot = QStringLiteral("/proc"));
};
