#include "runtime/ShellRuntime.hpp"

#include "AltTab/core/AltTabController.hpp"
#include "AltTab/core/AltTabShortcutRouter.hpp"
#include "AltTab/platform/compositor/CompositorBackend.hpp"
#include "AltTab/platform/hyprland/HyprlandWindowSource.hpp"
#include "AltTab/platform/runtime/AltTabRuntimePaths.hpp"
#include "AltTab/services/AltTabConfigWatcher.hpp"
#include "Dock/core/DockController.hpp"
#include "Dock/platform/runtime/DockRuntimePaths.hpp"
#include "Dock/services/DockConfigWatcher.hpp"
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
#include "services/AppIdentityResolver.hpp"

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

    if (!createBackend(backendName, errorOut)) {
        m_shortcutClient.reset();
        m_typhonSession.reset();
        m_identityResolver.reset();
        m_launcher.reset();
        m_catalog.reset();
        return false;
    }

    m_dockController = std::make_unique<DockController>(m_launcher.get(), m_catalog.get());
    m_altTabController = std::make_unique<AltTabController>(m_windowBackend.get(),
                                                              m_identityResolver.get());
    m_spotlightController = std::make_unique<SpotlightController>(
        spotlightPaths, m_catalog.get(), m_launcher.get());
    m_shortcutDispatcher = std::make_unique<ShellShortcutDispatcher>(m_altTabController.get(),
                                                                       m_spotlightController.get());
    m_ipcServer = std::make_unique<ShellIpcServer>();
    m_gameMode = std::make_unique<GameModeMonitor>();

    const DockRuntimePaths dockPaths = DockRuntimePaths::fromEnvironment();
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
#endif
    if (m_windowBackend)
        m_windowBackend->start();
    m_shortcutClient->start();
    m_gameMode->start();
    emit started();
}

void ShellRuntime::stop()
{
    if (!m_started && !m_initialized)
        return;
    m_started = false;
    m_gameMode->stop();
    m_shortcutClient->stop();
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
