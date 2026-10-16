#pragma once

#include <QString>
#include <QStringList>

#include "core/DockPinStore.hpp"
#include "dock/DockConfig.hpp"

class DockConfigPersistence final : public DockPinStore {
public:
    explicit DockConfigPersistence(const QString &configPath);
    ~DockConfigPersistence() override = default;

    QString configPath() const { return m_configPath; }
    bool writePins(const QStringList &pins, QString *errorOut = nullptr) override;
    bool writeConfig(const DockConfig &config, QString *errorOut = nullptr);

private:
    QString m_configPath;
};
