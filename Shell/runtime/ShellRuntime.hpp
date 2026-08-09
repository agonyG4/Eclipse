#pragma once

#include <QObject>
#include <QString>

#include <memory>

class AltTabConfigWatcher;
class AltTabController;
class AppIdentityResolver;
class ApplicationLauncher;
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
    CompositorBackend *windowBackend() const { return m_windowBackend.get(); }
    DockController *dockController() const { return m_dockController.get(); }
    AltTabController *altTabController() const { return m_altTabController.get(); }
    SpotlightController *spotlightController() const { return m_spotlightController.get(); }
    ShellShortcutDispatcher *shortcutDispatcher() const { return m_shortcutDispatcher.get(); }
    GameModeMonitor *gameModeMonitor() const { return m_gameMode.get(); }
    ShellIpcServer *ipcServer() const { return m_ipcServer.get(); }
    DockConfigWatcher *dockConfig() const { return m_dockConfig.get(); }
    AltTabConfigWatcher *altTabConfig() const { return m_altTabConfig.get(); }
    SpotlightConfigWatcher *spotlightConfig() const { return m_spotlightConfig.get(); }

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
    std::unique_ptr<CompositorBackend> m_windowBackend;
    std::unique_ptr<DockController> m_dockController;
    std::unique_ptr<AltTabController> m_altTabController;
    std::unique_ptr<SpotlightController> m_spotlightController;
    std::unique_ptr<ShellShortcutDispatcher> m_shortcutDispatcher;
    std::unique_ptr<ShellIpcServer> m_ipcServer;
    std::unique_ptr<DockConfigWatcher> m_dockConfig;
    std::unique_ptr<AltTabConfigWatcher> m_altTabConfig;
    std::unique_ptr<SpotlightConfigWatcher> m_spotlightConfig;
    std::unique_ptr<GameModeMonitor> m_gameMode;
    bool m_initialized = false;
    bool m_started = false;
};
