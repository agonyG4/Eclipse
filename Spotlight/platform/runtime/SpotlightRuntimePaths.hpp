#pragma once

#include "core/SpotlightControllerPaths.hpp"

#include <QString>
#include <QDir>

class SpotlightRuntimePaths {
public:
    static SpotlightRuntimePaths fromEnvironment();

    QString astreaRoot() const { return m_astreaRoot; }
    QString astreaLaunch() const { return m_astreaRoot + QStringLiteral("/bin/astrea-launch"); }
    QString weatherCli() const { return m_astreaRoot + QStringLiteral("/bin/weather-cli"); }
    QString configPath() const;
    QString componentsConfigPath() const;
    QString i18nDir() const;
    QString weatherAssetDir() const;
    SpotlightControllerPaths controllerPaths() const;

private:
    explicit SpotlightRuntimePaths(const QString &root) : m_astreaRoot(root) {}
    QString m_astreaRoot;
};
