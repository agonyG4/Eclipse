#pragma once

#include "core/SpotlightLauncher.hpp"

class ApplicationLauncher;

class SpotlightApplicationLauncherAdapter final : public SpotlightLauncher {
public:
    explicit SpotlightApplicationLauncherAdapter(ApplicationLauncher &launcher,
                                                 QObject *parent = nullptr);

    void launch(const SpotlightLaunchRequest &request) override;

private:
    ApplicationLauncher &m_launcher;
};
