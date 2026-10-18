#include "services/SpotlightApplicationLauncherAdapter.hpp"

#include "launch/ApplicationLauncher.hpp"

SpotlightApplicationLauncherAdapter::SpotlightApplicationLauncherAdapter(
    ApplicationLauncher &launcher, QObject *parent)
    : SpotlightLauncher(parent)
    , m_launcher(launcher)
{
    connect(&m_launcher, &ApplicationLauncher::launchFailed,
            this, &SpotlightLauncher::launchFailed);
    connect(&m_launcher, &ApplicationLauncher::launchTimedOut,
            this, &SpotlightLauncher::launchTimedOut);
}

void SpotlightApplicationLauncherAdapter::launch(const SpotlightLaunchRequest &request)
{
    m_launcher.launchDesktop(ApplicationLaunchRequest{
        request.desktopId,
        request.desktopFileName,
        request.exec,
        request.appName,
        request.iconName,
        request.desktopFilePath,
    });
}
