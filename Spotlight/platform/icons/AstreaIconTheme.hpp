#pragma once

#include <QString>
#include <QStringList>

class AstreaIconTheme final {
public:
    static QString resolve();
    static QString apply();
    static QString themeSource();

    struct ResolveResult {
        QString theme;
        QString source;
    };
    static ResolveResult resolveWithSource();

    static QStringList searchPaths();
    static bool themeExists(const QString &themeName);
};
