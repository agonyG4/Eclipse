#pragma once

#include <QObject>
#include <QString>

#include <memory>

class AltTabConfigWatcher;
class AltTabController;
class AppIdentityResolver;
class ApplicationLauncher;
class BarClockService;
class BarController;
class CompositorBackend;
class DesktopEntryCatalog;
class DockConfigWatcher;
class DockController;
class GameModeMonitor;
class SpotlightConfigWatcher;
class SpotlightController;
class ShellIpcServer;
class ShellShortcutDispatcher;
class TyphonShortcutClient;
class TyphonSharedConnection;
class TyphonToplevelConnection;
class TyphonWorkspaceClient;
class TyphonWorkspaceController;
class ThemeController;
class WorkspaceModel;

namespace Astrea::System {
class AudioService;
class BluetoothService;
class NetworkService;
}

class ShellRuntime final : public QObject {
    Q_OBJECT

public:
    explicit ShellRuntime(QObject *parent = nullptr);
    ~ShellRuntime() override;

    bool initialize(const QString &backendName = QStringLiteral("auto"),
                    QString *errorOut = nullptr);
    void start();
    void stop();
    bool isInitialized() const { return m_initialized; }

    DesktopEntryCatalog *catalog() const { return m_catalog.get(); }
    AppIdentityResolver *identityResolver() const { return m_identityResolver.get(); }
    ApplicationLauncher *launcher() const { return m_launcher.get(); }
    TyphonSharedConnection *typhonSession() const { return m_typhonSession.get(); }
    TyphonToplevelConnection *typhonToplevelConnection() const { return m_typhonToplevel.get(); }
    TyphonShortcutClient *shortcutClient() const { return m_shortcutClient.get(); }
    TyphonWorkspaceClient *workspaceClient() const { return m_workspaceClient.get(); }
    TyphonWorkspaceController *workspaceController() const { return m_workspaceController.get(); }
    CompositorBackend *windowBackend() const { return m_windowBackend.get(); }
    DockController *dockController() const { return m_dockController.get(); }
    AltTabController *altTabController() const { return m_altTabController.get(); }
    SpotlightController *spotlightController() const { return m_spotlightController.get(); }
    BarController *barController() const { return m_barController.get(); }
    BarClockService *barClock() const { return m_barClock.get(); }
    ThemeController *themeController() const { return m_themeController.get(); }
    WorkspaceModel *workspaceModel() const { return m_workspaceModel.get(); }
    ShellShortcutDispatcher *shortcutDispatcher() const { return m_shortcutDispatcher.get(); }
    GameModeMonitor *gameModeMonitor() const { return m_gameMode.get(); }
    ShellIpcServer *ipcServer() const { return m_ipcServer.get(); }
    DockConfigWatcher *dockConfig() const { return m_dockConfig.get(); }
    AltTabConfigWatcher *altTabConfig() const { return m_altTabConfig.get(); }
    SpotlightConfigWatcher *spotlightConfig() const { return m_spotlightConfig.get(); }
    Astrea::System::AudioService *audioService() const { return m_audioService.get(); }
    Astrea::System::NetworkService *networkService() const { return m_networkService.get(); }
    Astrea::System::BluetoothService *bluetoothService() const
    { return m_bluetoothService.get(); }

    void reloadCatalog();
    void reloadDockConfig();
    void reloadAltTabConfig();
    void reloadSpotlightConfig();

signals:
    void started();
    void stopped();

private:
    bool createBackend(const QString &backendName, QString *errorOut);
    void connectServices();

    std::unique_ptr<DesktopEntryCatalog> m_catalog;
    std::unique_ptr<ApplicationLauncher> m_launcher;
    std::unique_ptr<AppIdentityResolver> m_identityResolver;
    std::unique_ptr<TyphonSharedConnection> m_typhonSession;
    std::unique_ptr<TyphonToplevelConnection> m_typhonToplevel;
    std::unique_ptr<TyphonShortcutClient> m_shortcutClient;
    std::unique_ptr<TyphonWorkspaceClient> m_workspaceClient;
    std::unique_ptr<TyphonWorkspaceController> m_workspaceController;
    std::unique_ptr<CompositorBackend> m_windowBackend;
    std::unique_ptr<DockController> m_dockController;
    std::unique_ptr<AltTabController> m_altTabController;
    std::unique_ptr<SpotlightController> m_spotlightController;
    std::unique_ptr<BarController> m_barController;
    std::unique_ptr<BarClockService> m_barClock;
    std::unique_ptr<ThemeController> m_themeController;
    std::unique_ptr<WorkspaceModel> m_workspaceModel;
    std::unique_ptr<ShellShortcutDispatcher> m_shortcutDispatcher;
    std::unique_ptr<ShellIpcServer> m_ipcServer;
    std::unique_ptr<DockConfigWatcher> m_dockConfig;
    std::unique_ptr<AltTabConfigWatcher> m_altTabConfig;
    std::unique_ptr<SpotlightConfigWatcher> m_spotlightConfig;
    std::unique_ptr<GameModeMonitor> m_gameMode;
    std::unique_ptr<Astrea::System::AudioService> m_audioService;
    std::unique_ptr<Astrea::System::NetworkService> m_networkService;
    std::unique_ptr<Astrea::System::BluetoothService> m_bluetoothService;
    bool m_initialized = false;
    bool m_started = false;
};
