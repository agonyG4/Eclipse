#include "core/BarController.hpp"

#include "apps/DesktopEntryCatalog.hpp"
#include "launch/ApplicationLauncher.hpp"
#include "core/WorkspaceModel.hpp"
#include "Spotlight/core/SpotlightController.hpp"

BarController::BarController(DesktopEntryCatalog *catalog, ApplicationLauncher *launcher,
                             SpotlightController *spotlight, WorkspaceModel *workspaceModel,
                             QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_launcher(launcher)
    , m_spotlight(spotlight)
    , m_workspaceModel(workspaceModel)
{
    if (m_catalog) {
        connect(m_catalog, &DesktopEntryCatalog::indexUpdated,
                this, &BarController::capabilitiesChanged);
    }
    if (m_spotlight) {
        connect(m_spotlight, &SpotlightController::componentEnabledChanged,
                this, &BarController::capabilitiesChanged);
    }
}

bool BarController::searchAvailable() const
{
    return m_spotlight != nullptr && m_spotlight->componentEnabled();
}

void BarController::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
}

bool BarController::findSettingsEntry() const
{
    if (!m_catalog)
        return false;
    return m_catalog->findByDesktopFileName(QStringLiteral("astrea-settings.desktop")).has_value()
        || m_catalog->findByDesktopId(QStringLiteral("astrea-settings")).has_value();
}

bool BarController::settingsAvailable() const
{
    return m_launcher != nullptr && findSettingsEntry();
}

bool BarController::showSearch()
{
    if (!m_enabled || !m_spotlight || !searchAvailable())
        return false;
    m_spotlight->show();
    return true;
}

bool BarController::launchSettings()
{
    if (!m_enabled || !m_launcher || !m_catalog)
        return false;

    const auto byFileName = m_catalog->findByDesktopFileName(
        QStringLiteral("astrea-settings.desktop"));
    const auto entry = byFileName.has_value()
        ? byFileName
        : m_catalog->findByDesktopId(QStringLiteral("astrea-settings"));
    if (!entry.has_value())
        return false;

    const DesktopEntryRecord &record = entry.value();
    m_launcher->launchDesktop(ApplicationLaunchRequest{
        record.id,
        record.desktopFileName,
        record.exec,
        record.name,
        record.icon,
        record.sourceFilePath,
    });
    return true;
}
