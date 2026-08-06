#pragma once

#include "core/CompositorTypes.hpp"
#include <QString>
#include <QJsonObject>

struct WindowInfo {
    WindowId windowId;
    qint64 pid = 0;
    QString appId;
    QString desktopId;
    QString className;
    QString initialClass;
    QString title;
    QString initialTitle;
    QString displayName;
    WorkspaceId workspaceId;
    QString workspaceName;
    OutputId outputId;
    bool isActive = false;
    bool isHidden = false;
    bool isMinimized = false;
    bool isSpecial = false;
    bool skipSwitcher = false;
    int focusHistoryId = 999999;

    QString iconName;
    QString iconPath;
    bool iconPending = false;
    bool showFallbackText = true;
    quint64 backendGeneration = 0;

    // Backward compatible getters / helpers
    QString address() const { return windowId.value; }
    int workspaceIdInt() const { return workspaceId.value.toInt(); }

    QString stableKey() const {
        return windowId.value;
    }

    QString metaKey() const {
        return QString(className + QLatin1Char('|') + initialClass + QLatin1Char('|')
                       + title + QLatin1Char('|') + initialTitle).toLower();
    }

    bool needsDeepIcon() const;

    static WindowInfo fromJson(const QJsonObject &obj);
    static QString normalizeAddress(const QString &addr);
    static QString displayNameFromMetadata(const QString &className, const QString &title);
};

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
