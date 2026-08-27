#include "runtime/ShellRuntime.hpp"

#include "AltTab/core/AltTabController.hpp"
#include "AltTab/core/AltTabShortcutRouter.hpp"
#include "AltTab/platform/compositor/CompositorBackend.hpp"
#include "AltTab/platform/hyprland/HyprlandWindowSource.hpp"
#include "AltTab/platform/runtime/AltTabRuntimePaths.hpp"
#include "AltTab/services/AltTabConfigWatcher.hpp"
#include "Bar/core/BarClockService.hpp"
#include "Bar/core/BarController.hpp"
#include "Bar/core/WorkspaceModel.hpp"
#include "Dock/core/DockController.hpp"
#include "Dock/core/DockSurfaceGeometry.hpp"
#include "Dock/platform/runtime/DockRuntimePaths.hpp"
#include "Dock/services/DockConfigWatcher.hpp"
#include "Dock/services/DockConfigPersistence.hpp"
#include "core/ContextMenuController.hpp"
#include "core/ContextMenuProviders.hpp"
#include "Spotlight/core/SpotlightController.hpp"
#include "Spotlight/platform/runtime/SpotlightRuntimePaths.hpp"
#include "Spotlight/services/GameModeMonitor.hpp"
#include "Spotlight/services/SpotlightConfigWatcher.hpp"
#include "apps/DesktopEntryCatalog.hpp"
#include "launch/ApplicationLauncher.hpp"
#include "platform/ipc/ShellIpcServer.hpp"
#include "platform/shortcut/ShellShortcutDispatcher.hpp"
#include "platform/typhon/TyphonSharedConnection.hpp"
#include "platform/typhon/TyphonShortcutClient.hpp"
#include "platform/typhon/TyphonToplevelConnection.hpp"
#include "platform/typhon/TyphonWorkspaceClient.hpp"
#include "Paper/core/WallpaperPersistence.hpp"
#include "Paper/core/WallpaperCatalog.hpp"
#include "Paper/core/WallpaperResolver.hpp"
#include "Paper/core/WallpaperService.hpp"
#include "Paper/platform/ipc/WallpaperControlServer.hpp"
#include "services/AppIdentityResolver.hpp"
#include "theme/ThemeController.hpp"
#include "system/audio/AudioService.hpp"
#include "system/network/NetworkService.hpp"
#include "system/bluetooth/BluetoothService.hpp"
#include "statusnotifier/StatusNotifierService.hpp"

#if ASTREA_HAVE_TYPHON_PROTOCOL
#include "AltTab/platform/typhon/TyphonWindowSource.hpp"
#endif

#include <QDebug>
ShellRuntime::ShellRuntime(QObject *parent)
    : QObject(parent)
{
}

ShellRuntime::~ShellRuntime()
{
    stop();
}

bool ShellRuntime::initialize(const QString &backendName, QString *errorOut)
{
    if (m_initialized)
        return true;

    m_catalog = std::make_unique<DesktopEntryCatalog>();
    m_catalog->initialize();

    const SpotlightRuntimePaths spotlightPaths = SpotlightRuntimePaths::fromEnvironment();
    m_launcher = std::make_unique<ApplicationLauncher>(spotlightPaths.astreaLaunch());
    m_identityResolver = std::make_unique<AppIdentityResolver>();
    m_identityResolver->initialize(m_catalog.get());

    m_typhonSession = std::make_unique<TyphonSharedConnection>();
    m_shortcutClient = std::make_unique<TyphonShortcutClient>(m_typhonSession.get());
    m_workspaceClient = std::make_unique<TyphonWorkspaceClient>(m_typhonSession.get());
    m_workspaceController = std::make_unique<TyphonWorkspaceController>(m_workspaceClient.get());

    if (!createBackend(backendName, errorOut)) {
        m_shortcutClient.reset();
        m_workspaceController.reset();
        m_workspaceClient.reset();
        m_typhonSession.reset();
        m_identityResolver.reset();
        m_launcher.reset();
        m_catalog.reset();
        return false;
    }

    const DockRuntimePaths dockPaths = DockRuntimePaths::fromEnvironment();
    m_dockPersistence = std::make_unique<DockConfigPersistence>(dockPaths.dockConfigPath());
    m_dockController = std::make_unique<DockController>(m_launcher.get(), m_catalog.get(),
                                                         m_dockPersistence.get());
    m_dockSurfaceGeometry = std::make_unique<DockSurfaceGeometry>();
    m_altTabController = std::make_unique<AltTabController>(m_windowBackend.get(),
                                                              m_identityResolver.get());
    m_spotlightController = std::make_unique<SpotlightController>(
        spotlightPaths, m_catalog.get(), m_launcher.get());
    m_workspaceModel = std::make_unique<WorkspaceModel>();
    connect(m_workspaceClient.get(), &TyphonWorkspaceClient::snapshotChanged, this,
            [this](QVector<TyphonWorkspaceRecord> workspaces) {
        QVector<WorkspaceItem> items;
        items.reserve(workspaces.size());
        for (const TyphonWorkspaceRecord &workspace : workspaces) {
            items.append({workspace.name, workspace.active, false, workspace.urgent, {},
                          workspace.id});
        }
        m_workspaceModel->replaceWorkspaces(std::move(items));
    });
    m_barClock = std::make_unique<BarClockService>();
    m_themeController = std::make_unique<ThemeController>();
    m_barController = std::make_unique<BarController>(m_catalog.get(), m_launcher.get(),
                                                       m_spotlightController.get(),
                                                       m_workspaceModel.get());
    m_barController->setWorkspaceController(m_workspaceController.get());
    m_shortcutDispatcher = std::make_unique<ShellShortcutDispatcher>(m_altTabController.get(),
                                                                       m_spotlightController.get());
    m_ipcServer = std::make_unique<ShellIpcServer>();
    const auto wallpaperResolver = Paper::WallpaperResolver();
    auto wallpaperCatalog = std::make_shared<Paper::WallpaperCatalog>(wallpaperResolver);
    m_wallpaperService = std::make_unique<Paper::WallpaperService>(
        wallpaperResolver,
        std::make_unique<Paper::XdgWallpaperPersistence>(),
        std::move(wallpaperCatalog));
    m_wallpaperService->initialize();
    m_wallpaperControlServer = std::make_unique<Paper::WallpaperControlServer>(
        m_wallpaperService.get());
    QString wallpaperControlError;
    if (!m_wallpaperControlServer->listen(&wallpaperControlError)) {
        if (errorOut)
            *errorOut = wallpaperControlError;
        return false;
    }
    m_gameMode = std::make_unique<GameModeMonitor>();
    m_audioService = std::make_unique<Astrea::System::AudioService>();
    m_networkService = std::make_unique<Astrea::System::NetworkService>();
    m_bluetoothService = std::make_unique<Astrea::System::BluetoothService>();
    m_statusNotifier = std::make_unique<Astrea::StatusNotifier::StatusNotifierService>();
    m_statusNotifier->initialize();
    m_contextMenuController = std::make_unique<Astrea::Shell::ContextMenuController>();
    m_desktopContextMenuProvider = std::make_unique<Astrea::Shell::DesktopContextMenuProvider>(
        m_launcher.get(), m_catalog.get());
    m_dockContextMenuProvider = std::make_unique<Astrea::Shell::DockContextMenuProvider>(
        m_dockController.get(), m_catalog.get(), m_launcher.get());
    m_trayContextMenuAdapter = std::make_unique<Astrea::Shell::TrayContextMenuAdapter>(
        m_statusNotifier.get());
    m_contextMenuController->setDesktopProvider(m_desktopContextMenuProvider.get());
    m_contextMenuController->setDockProvider(m_dockContextMenuProvider.get());
    m_contextMenuController->setTrayProvider(m_trayContextMenuAdapter.get());

    const AltTabRuntimePaths altTabPaths = AltTabRuntimePaths::fromEnvironment();
    m_dockConfig = std::make_unique<DockConfigWatcher>(dockPaths.dockConfigPath(),
                                                        dockPaths.componentsConfigPath());
    m_altTabConfig = std::make_unique<AltTabConfigWatcher>(altTabPaths.alttabConfigPath(),
                                                            altTabPaths.componentsConfigPath());
    m_spotlightConfig = std::make_unique<SpotlightConfigWatcher>(
        spotlightPaths.configPath(), spotlightPaths.componentsConfigPath());

#if ASTREA_HAVE_TYPHON_PROTOCOL
    m_dockController->attachTyphonConnection(m_typhonToplevel.get());
#endif

    m_dockController->applyConfig(m_dockConfig->config());
    m_dockController->setComponentEnabled(m_dockConfig->componentEnabled());
    m_dockController->setCatalogSnapshot(m_catalog->snapshot());
    m_shortcutDispatcher->setAltTabEnabled(m_altTabConfig->componentEnabled());
    m_shortcutDispatcher->setSpotlightEnabled(m_spotlightConfig->componentEnabled());
    m_spotlightController->setComponentEnabled(m_spotlightConfig->componentEnabled());
    m_spotlightController->applyConfig(m_spotlightConfig->spotlightConfig());

    QString spotlightError;
    if (!m_spotlightController->init(QStringLiteral("en_US"), &spotlightError)) {
        if (errorOut)
            *errorOut = spotlightError;
        return false;
    }

    connectServices();
    m_initialized = true;
    return true;
}

bool ShellRuntime::createBackend(const QString &backendName, QString *errorOut)
{
    const QString requested = backendName.trimmed().toLower();
#if ASTREA_HAVE_TYPHON_PROTOCOL
    const bool useTyphon = requested.isEmpty() || requested == QStringLiteral("auto")
        || requested == QStringLiteral("typhon");
#else
    const bool useTyphon = false;
#endif

    if (useTyphon) {
#if ASTREA_HAVE_TYPHON_PROTOCOL
        m_typhonToplevel = std::make_unique<TyphonToplevelConnection>(m_typhonSession.get());
        m_typhonToplevel->setParent(this);
        m_windowBackend = std::make_unique<TyphonWindowSource>(m_typhonToplevel.get());
        return true;
#endif
    }

    if (requested == QStringLiteral("typhon")) {
        if (errorOut)
            *errorOut = QStringLiteral("Typhon backend is not compiled in this build");
        return false;
    }

    if (requested == QStringLiteral("auto") || requested == QStringLiteral("hyprland")) {
        m_windowBackend = std::make_unique<HyprlandWindowSource>();
        return true;
    }

    if (errorOut)
        *errorOut = QStringLiteral("Unsupported shell window backend: %1").arg(backendName);
    return false;
}

void ShellRuntime::connectServices()
{
    connect(m_dockController.get(), &DockController::modelChanged, this, [this] {
        if (!m_contextMenuController || !m_contextMenuController->hasActivePresentation()
            || m_contextMenuController->target().kind
                   != Astrea::Shell::ContextMenuTarget::Kind::DockApplication)
            return;
        if (m_dockController->appModel()->rowForDesktopFileName(
                m_contextMenuController->target().identity) < 0)
            m_contextMenuController->invalidateTarget();
    });
    connect(m_statusNotifier.get(), &Astrea::StatusNotifier::StatusNotifierService::itemRemoved,
            this, [this](const QString &itemKey) {
        if (m_contextMenuController && m_contextMenuController->hasActivePresentation()
            && m_contextMenuController->target().kind
                   == Astrea::Shell::ContextMenuTarget::Kind::TrayItem
            && m_contextMenuController->target().identity == itemKey)
            m_contextMenuController->invalidateTarget();
    });
    connect(m_dockConfig.get(), &DockConfigWatcher::configChanged, this, [this] {
        m_dockController->applyConfig(m_dockConfig->config());
    });
    connect(m_dockConfig.get(), &DockConfigWatcher::componentToggled, this,
            [this](bool enabled) { m_dockController->setComponentEnabled(enabled); });

    connect(m_altTabConfig.get(), &AltTabConfigWatcher::componentToggled, this,
            [this](bool enabled) {
        m_shortcutDispatcher->setAltTabEnabled(enabled);
        if (!enabled)
            m_altTabController->cancel();
    });

    connect(m_spotlightConfig.get(), &SpotlightConfigWatcher::componentToggled, this,
            [this](bool enabled) {
        m_shortcutDispatcher->setSpotlightEnabled(enabled);
        m_spotlightController->setComponentEnabled(enabled);
    });
    connect(m_spotlightConfig.get(), &SpotlightConfigWatcher::configChanged, this,
            [this] { m_spotlightController->applyConfig(m_spotlightConfig->spotlightConfig()); });

    connect(m_gameMode.get(), &GameModeMonitor::gameModeChanged, this,
            [this] { m_spotlightController->setGameModeActive(m_gameMode->gameModeActive()); });

    connect(m_shortcutClient.get(), &TyphonShortcutClient::shortcutEvent, this,
            [this](const QString &namespaceName, const QString &name,
                   TyphonShortcutPhase phase, std::uint32_t, std::uint32_t) {
        m_shortcutDispatcher->dispatch(namespaceName, name, phase);
    });
}

void ShellRuntime::reloadCatalog()
{
    if (m_catalog)
        m_catalog->initialize();
}

void ShellRuntime::reloadDockConfig()
{
    if (m_dockConfig)
        m_dockConfig->refresh();
}

void ShellRuntime::reloadAltTabConfig()
{
    if (m_altTabConfig)
        m_altTabConfig->refresh();
}

void ShellRuntime::reloadSpotlightConfig()
{
    if (m_spotlightConfig)
        m_spotlightConfig->refresh();
}

void ShellRuntime::start()
{
    if (!m_initialized || m_started)
        return;
    m_started = true;
#if ASTREA_HAVE_TYPHON_PROTOCOL
    m_typhonSession->start();
    m_workspaceClient->start();
#endif
    if (m_windowBackend)
        m_windowBackend->start();
    m_shortcutClient->start();
    m_gameMode->start();
    m_barClock->start();
    m_audioService->start();
    m_networkService->start();
    m_bluetoothService->start();
    m_statusNotifier->start();
    emit started();
}

void ShellRuntime::stop()
{
    if (!m_started && !m_initialized)
        return;
    m_started = false;
    if (m_contextMenuController)
        m_contextMenuController->shutdown();
    if (m_bluetoothService)
        m_bluetoothService->stop();
    if (m_statusNotifier)
        m_statusNotifier->stop();
    if (m_networkService)
        m_networkService->stop();
    if (m_audioService)
        m_audioService->stop();
    m_gameMode->stop();
    m_shortcutClient->stop();
    m_workspaceClient->stop();
    if (m_barClock)
        m_barClock->stop();
    if (m_windowBackend)
        m_windowBackend->stop();
#if ASTREA_HAVE_TYPHON_PROTOCOL
    m_typhonSession->stop();
#endif
    if (m_altTabController)
        m_altTabController->cancel();
    if (m_spotlightController)
        m_spotlightController->close();
    emit stopped();
}
