#include "core/BarController.hpp"

#include "apps/DesktopEntryCatalog.hpp"
#include "launch/ApplicationLauncher.hpp"
#include "core/WorkspaceModel.hpp"

BarController::BarController(DesktopEntryCatalog *catalog, ApplicationLauncher *launcher,
                             WorkspaceModel *workspaceModel, QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_launcher(launcher)
    , m_workspaceModel(workspaceModel)
{
    if (m_catalog) {
        connect(m_catalog, &DesktopEntryCatalog::indexUpdated,
                this, &BarController::capabilitiesChanged);
    }
}

void BarController::setWorkspaceController(QObject *controller)
{
    if (m_workspaceController == controller)
        return;
    m_workspaceController = controller;
    emit workspaceControllerChanged();
}

void BarController::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
}

void BarController::setSearchAvailable(bool available)
{
    if (m_searchAvailable == available)
        return;
    m_searchAvailable = available;
    emit capabilitiesChanged();
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
    if (!m_enabled || !m_searchAvailable)
        return false;
    emit searchRequested();
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
