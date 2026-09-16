#pragma once

#include <QMetaType>
#include <QString>

struct WindowIdentityInput {
    QString address;
    qint64 pid = 0;
    QString className;
    QString initialClass;
    QString title;
    QString initialTitle;
    int workspaceId = -1;
    quint64 openGeneration = 0;
    QString metadataFingerprint;
    int themeRevision = 0;
    int desktopIndexRevision = 0;
    int steamIndexRevision = 0;
};

struct AppIdentity {
    QString stableKey;
    QString windowId;
    qint64 pid = 0;
    quint64 openGeneration = 0;
    QString metadataFingerprint;
    int themeRevision = 0;
    int desktopIndexRevision = 0;
    int steamIndexRevision = 0;
    QString displayName;
    QString desktopId;
    QString iconName;
    QString iconPath;
    bool iconPending = false;
    bool showFallbackText = true;
    QString source;
};

Q_DECLARE_METATYPE(WindowIdentityInput)
Q_DECLARE_METATYPE(AppIdentity)
