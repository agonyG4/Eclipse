#pragma once

#include <QString>
#include <QDir>

class AltTabRuntimePaths {
public:
    static AltTabRuntimePaths fromEnvironment();

    QString astreaRoot() const { return m_astreaRoot; }
    QString alttabConfigPath() const;
    QString componentsConfigPath() const;

private:
    explicit AltTabRuntimePaths(const QString &root) : m_astreaRoot(root) {}
    QString m_astreaRoot;
};
