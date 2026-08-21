#pragma once

#include "core/WorkspaceModel.hpp"

#include <QObject>

class ApplicationLauncher;
class BarClockService;
class DesktopEntryCatalog;
class SpotlightController;
class BarController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool searchAvailable READ searchAvailable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool settingsAvailable READ settingsAvailable NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool aboutAvailable READ aboutAvailable CONSTANT)
    Q_PROPERTY(bool forceQuitAvailable READ forceQuitAvailable CONSTANT)
    Q_PROPERTY(bool lockscreenAvailable READ lockscreenAvailable CONSTANT)
    Q_PROPERTY(bool powerAvailable READ powerAvailable CONSTANT)
    Q_PROPERTY(bool notificationHistoryAvailable READ notificationHistoryAvailable CONSTANT)
    Q_PROPERTY(WorkspaceModel *workspaceModel READ workspaceModel CONSTANT)
    Q_PROPERTY(QObject *workspaceController READ workspaceController WRITE setWorkspaceController
                   NOTIFY workspaceControllerChanged)

public:
    BarController(DesktopEntryCatalog *catalog, ApplicationLauncher *launcher,
                  SpotlightController *spotlight, WorkspaceModel *workspaceModel = nullptr,
                  QObject *parent = nullptr);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    bool searchAvailable() const;
    bool settingsAvailable() const;
    bool aboutAvailable() const { return false; }
    bool forceQuitAvailable() const { return false; }
    bool lockscreenAvailable() const { return false; }
    bool powerAvailable() const { return false; }
    bool notificationHistoryAvailable() const { return false; }
    WorkspaceModel *workspaceModel() const { return m_workspaceModel; }
    QObject *workspaceController() const { return m_workspaceController; }
    void setWorkspaceController(QObject *controller);

    Q_INVOKABLE bool showSearch();
    Q_INVOKABLE bool launchSettings();

signals:
    void enabledChanged();
    void capabilitiesChanged();
    void workspaceControllerChanged();

private:
    bool findSettingsEntry() const;

    DesktopEntryCatalog *m_catalog = nullptr;
    ApplicationLauncher *m_launcher = nullptr;
    SpotlightController *m_spotlight = nullptr;
    WorkspaceModel *m_workspaceModel = nullptr;
    QObject *m_workspaceController = nullptr;
    bool m_enabled = true;
};
