#pragma once

#include <QString>

struct DockAppInfo {
    QString desktopFileName;
    QString desktopId;
    QString displayName;
    QString iconName;
    QString iconPath;
    QString iconUrl;
    bool resolved = false;
    bool launching = false;
    QString launchError;
    bool pinned = true;
    bool runtimeKnown = false;
    bool running = false;
    bool active = false;
    int windowCount = 0;
};

inline bool operator==(const DockAppInfo &left, const DockAppInfo &right)
{
    return left.desktopFileName == right.desktopFileName
        && left.desktopId == right.desktopId
        && left.displayName == right.displayName
        && left.iconName == right.iconName
        && left.iconPath == right.iconPath
        && left.iconUrl == right.iconUrl
        && left.resolved == right.resolved
        && left.launching == right.launching
        && left.launchError == right.launchError
        && left.pinned == right.pinned
        && left.runtimeKnown == right.runtimeKnown
        && left.running == right.running
        && left.active == right.active
        && left.windowCount == right.windowCount;
}
