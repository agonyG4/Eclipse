#pragma once

#include <QString>
#include <QStringList>
#include <QMutex>

class AstreaIconTheme final {
public:
    static QString resolve();
    static QString apply();
    static QString themeSource();
    static QMutex &qIconMutex();

    struct ResolveResult {
        QString theme;
        QString source;
    };
    static ResolveResult resolveWithSource();

    static QStringList searchPaths();
    static QStringList searchPathsFor(const QStringList &dataLocations,
                                      const QString &homePath);
    static bool themeExists(const QString &themeName);

private:
    static ResolveResult resolveWithSourceUnlocked();
};
