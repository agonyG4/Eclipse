#include "services/DockConfigPersistence.hpp"

#include <QFileInfo>
#include "dock/DockConfigStore.hpp"

DockConfigPersistence::DockConfigPersistence(const QString &configPath)
    : m_configPath(QFileInfo(configPath).absoluteFilePath())
{
}

bool DockConfigPersistence::writePins(const QStringList &pins, QString *errorOut)
{
    return DockConfigStore(m_configPath).writePins(pins, errorOut);
}

bool DockConfigPersistence::writeConfig(const DockConfig &config, QString *errorOut)
{
    return DockConfigStore(m_configPath).writeConfig(config, errorOut);
}
