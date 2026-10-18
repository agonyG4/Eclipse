#pragma once

#include <QObject>
#include <QString>

struct SpotlightLaunchRequest {
    QString desktopId;
    QString desktopFileName;
    QString exec;
    QString appName;
    QString iconName;
    QString desktopFilePath;
};

class SpotlightLauncher : public QObject {
    Q_OBJECT

public:
    explicit SpotlightLauncher(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~SpotlightLauncher() override = default;

    virtual void launch(const SpotlightLaunchRequest &request) = 0;

signals:
    void launchFailed(const QString &desktopId, const QString &error);
    void launchTimedOut(const QString &desktopId);
};
