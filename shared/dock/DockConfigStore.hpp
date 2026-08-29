#pragma once

#include "dock/DockConfig.hpp"

class DockConfigStore final {
public:
    explicit DockConfigStore(const QString &configPath);

    QString configPath() const { return m_configPath; }
    bool writePins(const QStringList &pins, QString *errorOut = nullptr) const;
    bool writeConfig(const DockConfig &config, QString *errorOut = nullptr) const;
    // Updates canonical personalization fields while preserving the raw pins
    // value for callers that do not own pin migration.
    bool writePersonalization(const DockConfig &config, QString *errorOut = nullptr) const;

private:
    bool writeObject(const QJsonObject &object, QString *errorOut) const;
    QString m_configPath;
};
