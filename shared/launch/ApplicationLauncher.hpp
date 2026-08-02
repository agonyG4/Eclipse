#pragma once

#include <QObject>
#include <QHash>
#include <QProcess>
#include <QString>
#include <QTimer>

struct ApplicationLaunchRequest {
    QString desktopId;
    QString desktopFileName;
    QString exec;
    QString appName;
    QString iconName;
    QString desktopFilePath;
};

class ApplicationLauncher : public QObject {
    Q_OBJECT

public:
    explicit ApplicationLauncher(const QString &astreaLaunchPath, QObject *parent = nullptr);

    virtual void launchDesktop(const ApplicationLaunchRequest &request);
    void launchDesktop(const QString &desktopId, const QString &desktopFileName,
                       const QString &exec, const QString &appName = {},
                       const QString &iconName = {}, const QString &desktopFilePath = {});
    bool isRunning() const;

signals:
    void launchAccepted(const QString &desktopId);
    void launchSucceeded(const QString &desktopId);
    void launchFailed(const QString &desktopId, const QString &error);
    void launchTimedOut(const QString &desktopId);
    void launchCompleted(const QString &desktopId, bool success);

protected:
    void runSupervised(const QString &desktopId, const QStringList &args);

private:
    struct PendingLaunch {
        QString desktopId;
        QProcess *process = nullptr;
        QTimer *timeout = nullptr;
    };

    void complete(QProcess *process, bool success, const QString &error = {}, bool timedOut = false);

    QString m_launchPath;
    QHash<QProcess *, PendingLaunch> m_pending;
    static constexpr int kTimeoutMs = 10000;
};
