#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

class ApplicationLauncher : public QObject {
    Q_OBJECT
public:
    explicit ApplicationLauncher(const QString &astreaLaunchPath, QObject *parent = nullptr);

    void launchDesktop(const QString &desktopId, const QString &desktopFileName, const QString &exec,
                       const QString &appName = {}, const QString &iconName = {},
                       const QString &desktopFilePath = {});
    bool isRunning() const;

signals:
    void launchSucceeded(const QString &desktopId);
    void launchFailed(const QString &desktopId, const QString &error);
    void launchTimedOut(const QString &desktopId);

private:
    void cleanupProc();
    void runSupervised(const QString &desktopId, const QStringList &args);

    QString m_launchPath;
    QProcess *m_proc = nullptr;
    QTimer *m_timeout = nullptr;
    QString m_currentDesktopId;
};
