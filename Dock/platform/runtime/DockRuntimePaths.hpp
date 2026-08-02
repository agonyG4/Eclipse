#pragma once

#include <QString>

class DockRuntimePaths {
public:
    static DockRuntimePaths fromEnvironment();

    QString astreaRoot() const { return m_astreaRoot; }
    QString astreaLaunch() const { return m_astreaRoot + QStringLiteral("/bin/astrea-launch"); }
    QString dockConfigPath() const;
    QString componentsConfigPath() const;

private:
    explicit DockRuntimePaths(const QString &root) : m_astreaRoot(root) {}
    QString m_astreaRoot;
};
