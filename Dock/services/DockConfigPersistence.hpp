#pragma once

#include <QString>
#include <QStringList>

#include "dock/DockConfig.hpp"

class DockConfigPersistence {
public:
    explicit DockConfigPersistence(const QString &configPath);
    virtual ~DockConfigPersistence() = default;

    QString configPath() const { return m_configPath; }
    virtual bool writePins(const QStringList &pins, QString *errorOut = nullptr);
    virtual bool writeConfig(const DockConfig &config, QString *errorOut = nullptr);

private:
    QString m_configPath;
};
