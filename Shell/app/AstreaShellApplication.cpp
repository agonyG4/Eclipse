#include "app/AstreaShellApplication.hpp"
#include "app/ScreenshotShortcutStatus.hpp"

#include "AltTab/core/AltTabController.hpp"
#include "AltTab/platform/wayland/LayerShellSurface.hpp"
#include "Dock/core/DockController.hpp"
#include "dock/DockConfig.hpp"
#include "Dock/core/DockSurfaceGeometry.hpp"
#include "Dock/platform/wayland/DockInputRegionBridge.hpp"
#include "Dock/platform/wayland/DockLayerShellSurface.hpp"
#include "Spotlight/core/SpotlightController.hpp"
#include "Spotlight/platform/runtime/SpotlightRuntimePaths.hpp"
#include "Spotlight/platform/wayland/LayerShellSurface.hpp"
#include "Spotlight/services/AstreaI18n.hpp"
#include "Bar/platform/wayland/BarSurfaceManager.hpp"
#include "Bar/core/BarController.hpp"
#include "Bar/services/BarClockService.hpp"
#include "Bar/core/WorkspaceModel.hpp"
#include "core/ContextMenuController.hpp"
#include "platform/wayland/ContextMenuSurfaceManager.hpp"
#include "Paper/platform/wayland/WallpaperSurfaceManager.hpp"
#include "platform/ipc/ShellIpcServer.hpp"
#include "runtime/ShellRuntime.hpp"
#include "platform/wayland/LayerShellHelper.hpp"
#include "platform/wayland/TransientQmlWindowSlot.hpp"
#include "icons/AstreaIconProvider.hpp"
#include "icons/AstreaIconTheme.hpp"
#include "statusnotifier/StatusNotifierIconProvider.hpp"
#include "statusnotifier/StatusNotifierService.hpp"
#include "screenshot/ScreenshotController.hpp"
#include "screenshot/ScreenshotInputRegionBridge.hpp"
#include "screenshot/ScreenshotImageProvider.hpp"
#include "theme/ThemeController.hpp"
#include "system/audio/AudioService.hpp"
#include "system/audio/spatial/SpatialAudioEngine.hpp"
#include "system/network/NetworkService.hpp"
#include "system/bluetooth/BluetoothService.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QProcessEnvironment>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QWindow>

namespace {

constexpr QLatin1StringView kIpcName("astrea-shell-v1");

bool isWaylandPlatform(const QGuiApplication &application)
{
    return application.platformName().startsWith(QStringLiteral("wayland"), Qt::CaseInsensitive);
}

QVariantMap emptyTransientSurfaceSnapshot()
{
    return {
        {QStringLiteral("class"), QStringLiteral("transient")},
        {QStringLiteral("created"), false},
        {QStringLiteral("visible"), false},
        {QStringLiteral("destroyPending"), false},
        {QStringLiteral("width"), 0},
        {QStringLiteral("height"), 0},
        {QStringLiteral("liveInstances"), 0},
        {QStringLiteral("visibleInstances"), 0},
        {QStringLiteral("createCount"), 0},
        {QStringLiteral("destroyCount"), 0},
    };
}

QJsonObject transientSurfaceJson(const QVariantMap &snapshot)
{
    return QJsonObject::fromVariantMap(snapshot);
}

} // namespace

AstreaShellApplication::AstreaShellApplication(QGuiApplication &application)
    : QObject(&application), m_application(application)
{
}

AstreaShellApplication::~AstreaShellApplication() = default;

int AstreaShellApplication::run()
{
    m_application.setApplicationName(QStringLiteral("Astrea Shell"));
    m_application.setOrganizationName(QStringLiteral("AstreaOS"));
    m_application.setQuitOnLastWindowClosed(false);
    AstreaIconTheme::apply();

    if (!AstreaLayerShellHelper::compiled()) {
        qCritical("Astrea shell requires a production LayerShellQt build; configure with "
                  "-DASTREA_ENABLE_LAYER_SHELL=ON");
        return 1;
    }
    if (!isWaylandPlatform(m_application)) {
        qCritical("Astrea shell requires the Qt Wayland platform; current platform is %s",
                  qPrintable(m_application.platformName()));
        return 1;
    }
    m_layerShellWaylandBackend = true;

    QString capabilityError;
    m_layerShellProtocolAdvertised =
        AstreaLayerShellHelper::protocolAdvertised(&capabilityError);
    if (!AstreaLayerShellHelper::validateRuntime(m_layerShellWaylandBackend,
                                                 m_layerShellProtocolAdvertised,
                                                 &capabilityError)) {
        qCritical("Astrea shell Layer Shell runtime capability check failed: %s",
                  qPrintable(capabilityError));
        return 1;
    }

    if (!initializeRuntime() || !listenForCommands() || !initializeQml())
        return 1;

    m_runtime->start();
    return m_application.exec();
}

bool AstreaShellApplication::initializeRuntime()
{
    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QString backend = environment.value(QStringLiteral("ASTREA_COMPOSITOR_BACKEND"),
                                               QStringLiteral("auto"));

    m_runtime = std::make_unique<ShellRuntime>();
    QString error;
    if (!m_runtime->initialize(backend, &error)) {
        qCritical("Astrea shell runtime initialization failed: %s", qPrintable(error));
        return false;
    }

    const SpotlightRuntimePaths paths = SpotlightRuntimePaths::fromEnvironment();
    m_i18n = std::make_unique<AstreaI18n>(paths.i18nDir());
    m_runtime->barClock()->setLocale(QLocale(m_i18n->language()));
    if (m_i18n->language() != QStringLiteral("en_US")) {
        if (!m_runtime->spotlightController()->setLocale(m_i18n->language(), &error)) {
            qCritical("Astrea shell locale initialization failed: %s", qPrintable(error));
            return false;
        }
    }
    return true;
}

bool AstreaShellApplication::listenForCommands()
{
    QString error;
    if (!m_runtime->ipcServer()->listen(QString::fromLatin1(kIpcName), &error)) {
        qCritical("Astrea shell IPC listen failed: %s", qPrintable(error));
        return false;
    }
    m_runtime->ipcServer()->setReplyCallback(
        [this](const ShellIpcServer::Command &) { return statusJson(); });
    connect(m_runtime->ipcServer(), &ShellIpcServer::commandReceived,
            this, &AstreaShellApplication::handleCommand);
    return true;
}

bool AstreaShellApplication::loadSurface(const QUrl &url, QQuickWindow **windowOut)
{
    const int rootCount = m_engine->rootObjects().size();
    m_engine->load(url);
    if (m_engine->rootObjects().size() <= rootCount) {
        qWarning("Astrea shell QML surface failed to load: %s", qPrintable(url.toString()));
        return false;
    }

    auto *window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().constLast());
    if (!window) {
        qWarning("Astrea shell QML surface is not a window: %s", qPrintable(url.toString()));
        return false;
    }
    if (windowOut)
        *windowOut = window;
    return true;
}

bool AstreaShellApplication::initializeQml()
{
    m_engine = std::make_unique<QQmlApplicationEngine>();
    connect(m_engine.get(), &QQmlApplicationEngine::warnings, this,
            [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            qWarning("Astrea shell QML: %s", qPrintable(warning.toString()));
    });

    auto *context = m_engine->rootContext();
    auto *iconProvider = new AstreaIconProvider;
    context->setContextProperty(QStringLiteral("AstreaIconProvider"), iconProvider);
    m_engine->addImageProvider(QStringLiteral("astrea-icon"), iconProvider);
    auto *trayIconProvider = new Astrea::StatusNotifier::StatusNotifierIconProvider(
        m_runtime->statusNotifier()->iconStore());
    m_engine->addImageProvider(QStringLiteral("astrea-tray"), trayIconProvider);
    auto *screenshotProvider = new ScreenshotImageProvider(m_runtime->screenshotController());
    m_engine->addImageProvider(QStringLiteral("astrea-screenshot"), screenshotProvider);
    context->setContextProperty(QStringLiteral("DockController"), m_runtime->dockController());
    context->setContextProperty(QStringLiteral("DockSurfaceGeometry"),
                                static_cast<QObject *>(m_runtime->dockSurfaceGeometry()));
    m_dockInputRegion = std::make_unique<DockInputRegionBridge>();
    context->setContextProperty(QStringLiteral("DockInputRegion"),
                                static_cast<QObject *>(m_dockInputRegion.get()));
    context->setContextProperty(QStringLiteral("AltTabController"),
                                m_runtime->altTabController());
    context->setContextProperty(QStringLiteral("AltTabWindowModel"),
                                m_runtime->altTabController()->windowModel());
    context->setContextProperty(QStringLiteral("SpotlightController"),
                                m_runtime->spotlightController());
    context->setContextProperty(QStringLiteral("ScreenshotController"),
                                static_cast<QObject *>(m_runtime->screenshotController()));
    m_screenshotInputRegion = std::make_unique<ScreenshotInputRegionBridge>();
    context->setContextProperty(QStringLiteral("ScreenshotInputRegion"),
                                static_cast<QObject *>(m_screenshotInputRegion.get()));
    m_screenshotUiBarrier = std::make_unique<ScreenshotWaylandUiBarrier>();
    m_runtime->screenshotController()->setUiBarrier(m_screenshotUiBarrier.get());
    m_screenshotUiBarrier->setTrackedWindows({nullptr, nullptr, nullptr});
    m_altTabSlot = std::make_unique<TransientQmlWindowSlot>(QStringLiteral("altTab"));
    m_spotlightSlot = std::make_unique<TransientQmlWindowSlot>(QStringLiteral("spotlight"));
    m_screenshotOverlaySlot = std::make_unique<TransientQmlWindowSlot>(
        QStringLiteral("screenshotOverlay"));
    m_screenshotThumbnailSlot = std::make_unique<TransientQmlWindowSlot>(
        QStringLiteral("screenshotThumbnail"));
    m_screenshotPreviewSlot = std::make_unique<TransientQmlWindowSlot>(
        QStringLiteral("screenshotPreview"));

    connect(m_runtime->altTabController(), &AltTabController::surfaceVisibleChanged,
            this, &AstreaShellApplication::syncAltTabSurface, Qt::DirectConnection);
    connect(m_runtime->spotlightController(), &SpotlightController::surfaceVisibleChanged,
            this, &AstreaShellApplication::syncSpotlightSurface, Qt::DirectConnection);
    auto *screenshotController = m_runtime->screenshotController();
    connect(screenshotController, &ScreenshotController::visibleChanged,
            this, &AstreaShellApplication::syncScreenshotOverlaySurface, Qt::DirectConnection);
    connect(screenshotController, &ScreenshotController::phaseChanged,
            this, &AstreaShellApplication::syncScreenshotOverlaySurface);
    connect(screenshotController, &ScreenshotController::thumbnailVisibleChanged,
            this, &AstreaShellApplication::syncScreenshotThumbnailSurface, Qt::DirectConnection);
    connect(screenshotController, &ScreenshotController::thumbnailDragActiveChanged,
            this, &AstreaShellApplication::syncScreenshotThumbnailSurface);
    connect(screenshotController, &ScreenshotController::previewVisibleChanged,
            this, &AstreaShellApplication::syncScreenshotPreviewSurface, Qt::DirectConnection);
    connect(m_screenshotOverlaySlot.get(), &TransientQmlWindowSlot::windowDestroyed,
            this, [this] { refreshScreenshotBarrierWindows(); });
    connect(m_screenshotThumbnailSlot.get(), &TransientQmlWindowSlot::windowDestroyed,
            this, [this] {
        if (m_screenshotInputRegion)
            m_screenshotInputRegion->setWindow(nullptr);
        refreshScreenshotBarrierWindows();
    });
    connect(m_screenshotPreviewSlot.get(), &TransientQmlWindowSlot::windowDestroyed,
            this, [this] { refreshScreenshotBarrierWindows(); });
    context->setContextProperty(QStringLiteral("AstreaI18n"), m_i18n.get());
    context->setContextProperty(QStringLiteral("BarController"),
                                static_cast<QObject *>(m_runtime->barController()));
    context->setContextProperty(QStringLiteral("BarClockService"),
                                static_cast<QObject *>(m_runtime->barClock()));
    context->setContextProperty(QStringLiteral("ThemeController"),
                                static_cast<QObject *>(m_runtime->themeController()));
    context->setContextProperty(QStringLiteral("WorkspaceModel"),
                                static_cast<QObject *>(m_runtime->workspaceModel()));
    context->setContextProperty(QStringLiteral("AudioService"),
                                static_cast<QObject *>(m_runtime->audioService()));
    context->setContextProperty(QStringLiteral("NetworkService"),
                                static_cast<QObject *>(m_runtime->networkService()));
    context->setContextProperty(QStringLiteral("BluetoothService"),
                                static_cast<QObject *>(m_runtime->bluetoothService()));
    context->setContextProperty(QStringLiteral("StatusNotifierService"),
                                static_cast<QObject *>(m_runtime->statusNotifier()));
    context->setContextProperty(QStringLiteral("ContextMenuController"),
                                static_cast<QObject *>(m_runtime->contextMenuController()));

    QQuickWindow *window = nullptr;
    if (!loadSurface(QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Dock/qml/Main.qml")),
                     &window))
        return false;
    m_dockWindow = window;
    m_dockTooltipWindow = m_dockWindow->findChild<QQuickWindow *>(
        QStringLiteral("dockTooltipSurface"));
    if (!m_dockTooltipWindow) {
        qCritical("Astrea shell Dock tooltip surface failed to load");
        return false;
    }
    m_dockInputRegion->setWindow(m_dockWindow);

    if (!configureSurfaces())
        return false;

    QString barError;
    m_barSurfaceManager = std::make_unique<BarSurfaceManager>(
        m_application, *m_engine, m_runtime->barController(), m_runtime->barClock(),
        m_runtime->workspaceModel(), m_runtime->audioService(), m_runtime->networkService(),
        m_runtime->bluetoothService(), this, BarSurfaceManager::BundleFactory{},
        m_runtime->statusNotifier(), m_runtime->contextMenuController());
    connect(m_barSurfaceManager.get(), &BarSurfaceManager::popupStateChanged,
            this, [this] {
        if (!m_barSurfaceManager || !m_barSurfaceManager->popupOpen() || !m_runtime)
            return;
        auto *contextMenu = m_runtime->contextMenuController();
        if (!contextMenu || !contextMenu->hasActivePresentation())
            return;
        contextMenu->close();
        contextMenu->completeClose();
    }, Qt::DirectConnection);
    if (!m_barSurfaceManager->initialize(&barError)) {
        qCritical("Astrea shell Bar surface initialization failed: %s", qPrintable(barError));
        return false;
    }
    m_runtime->contextMenuController()->setBarPopupCloser([this] {
        if (m_barSurfaceManager)
            m_barSurfaceManager->closePopups();
    });

    QString contextMenuError;
    m_contextMenuSurfaceManager = std::make_unique<ContextMenuSurfaceManager>(
        m_application, *m_engine, m_runtime->contextMenuController(), this,
        ContextMenuSurfaceManager::BundleFactory{}, m_runtime->statusNotifier());
    if (!m_contextMenuSurfaceManager->initialize(&contextMenuError)) {
        qCritical("Astrea shell Context Menu surface initialization failed: %s",
                  qPrintable(contextMenuError));
        return false;
    }

    QString wallpaperError;
    m_wallpaperSurfaceManager = std::make_unique<Paper::WallpaperSurfaceManager>(
        m_application, *m_engine, m_runtime->wallpaperService(), this);
    if (!m_wallpaperSurfaceManager->initialize(&wallpaperError)) {
        qCritical("Astrea shell Paper wallpaper surface initialization failed: %s",
                  qPrintable(wallpaperError));
        return false;
    }

    syncAltTabSurface();
    syncSpotlightSurface();
    syncScreenshotOverlaySurface();
    syncScreenshotThumbnailSurface();
    syncScreenshotPreviewSurface();

    connect(m_runtime->dockController(), &DockController::visibleChanged,
            this, &AstreaShellApplication::syncDockVisibility);
    connect(m_runtime->dockController(), &DockController::surfacePlacementChanged,
            this, &AstreaShellApplication::configureDockSurface);
    connect(m_runtime->dockController(), &DockController::reservationChanged,
            this, &AstreaShellApplication::updateDockExclusiveZone);
    syncDockVisibility();
    return true;
}

bool AstreaShellApplication::configureSurfaces()
{
    if (!m_dockWindow) {
        qCritical("Astrea shell Dock surface is unavailable");
        return false;
    }
    if (!DockLayerShellSurface::compiled()) {
        qCritical("Astrea shell Dock requires the compiled LayerShellQt integration");
        return false;
    }
    if (!configureDockSurface()) {
        qCritical("Astrea shell Dock Layer Shell setup failed");
        return false;
    }
    return true;
}

void AstreaShellApplication::syncAltTabSurface()
{
    if (!m_runtime || !m_engine || !m_altTabSlot)
        return;
    if (!m_runtime->altTabController()->surfaceVisible()) {
        m_altTabSlot->hide();
        m_altTabSlot->destroyWhenSafe();
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect geometry = screen->geometry();
    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/AltTab/qml/Main.qml"));
    request.screen = screen;
    request.size = geometry.size();
    request.requireSurfaceReadyGate = true;
    request.configure = [](QQuickWindow *window, QString *error) {
        return AltTabLayerShellSurface::configure(window, error);
    };
    QString error;
    if (!m_altTabSlot->ensureCreated(m_engine.get(), request, &error)) {
        qCritical("Astrea shell Alt+Tab surface creation failed: %s", qPrintable(error));
        m_application.exit(1);
    }
}

void AstreaShellApplication::syncSpotlightSurface()
{
    if (!m_runtime || !m_engine || !m_spotlightSlot)
        return;
    if (!m_runtime->spotlightController()->surfaceVisible()) {
        m_spotlightSlot->hide();
        m_spotlightSlot->destroyWhenSafe();
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Spotlight/qml/Main.qml"));
    request.screen = screen;
    request.size = screen->geometry().size();
    request.requireSurfaceReadyGate = true;
    request.configure = [](QQuickWindow *window, QString *error) {
        return LayerShellSurface::configure(window, error);
    };
    QString error;
    if (!m_spotlightSlot->ensureCreated(m_engine.get(), request, &error)) {
        qCritical("Astrea shell Spotlight surface creation failed: %s", qPrintable(error));
        m_application.exit(1);
    }
}

void AstreaShellApplication::syncScreenshotOverlaySurface()
{
    if (!m_runtime || !m_engine || !m_screenshotOverlaySlot)
        return;
    auto *controller = m_runtime->screenshotController();
    if (!controller->isVisible()) {
        m_screenshotOverlaySlot->hide();
        QTimer::singleShot(0, this, [this] {
            if (!m_runtime || !m_screenshotOverlaySlot)
                return;
            auto *current = m_runtime->screenshotController();
            if (current->isVisible()
                || current->phase() == ScreenshotPhase::WaitingLiveCapture)
                return;
            m_screenshotOverlaySlot->destroyWhenSafe();
        });
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    AstreaLayerShellConfig config;
    config.scope = QStringLiteral("astrea-screenshot");
    config.layer = AstreaLayerShellConfig::Layer::Overlay;
    config.keyboardInteractivity = AstreaLayerShellConfig::KeyboardInteractivity::Exclusive;
    config.anchorTop = true;
    config.anchorBottom = true;
    config.anchorLeft = true;
    config.anchorRight = true;
    config.exclusiveZone = -1;
    config.screen = screen;

    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = QUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Shell/Screenshot/ScreenshotOverlay.qml"));
    request.screen = screen;
    request.size = screen->geometry().size();
    request.requireSurfaceReadyGate = true;
    request.configure = [config](QQuickWindow *window, QString *error) {
        return AstreaLayerShellHelper::configure(window, config, error);
    };
    request.install = [this](QQuickWindow *, QString *) {
        refreshScreenshotBarrierWindows();
        return true;
    };
    request.beforeDestroy = [this](QQuickWindow *window) {
        refreshScreenshotBarrierWindows(window);
    };
    QString error;
    if (!m_screenshotOverlaySlot->ensureCreated(m_engine.get(), request, &error)) {
        qCritical("Astrea shell screenshot surface creation failed: %s", qPrintable(error));
        m_application.exit(1);
    }
}

void AstreaShellApplication::syncScreenshotThumbnailSurface()
{
    if (!m_runtime || !m_engine || !m_screenshotThumbnailSlot)
        return;
    auto *controller = m_runtime->screenshotController();
    if (!controller->thumbnailVisible()) {
        if (controller->thumbnailDragActive())
            return;
        m_screenshotThumbnailSlot->hide();
        QTimer::singleShot(0, this, [this] {
            if (!m_runtime || !m_screenshotThumbnailSlot)
                return;
            auto *current = m_runtime->screenshotController();
            if (current->thumbnailVisible() || current->thumbnailDragActive())
                return;
            m_screenshotThumbnailSlot->destroyWhenSafe();
        });
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    AstreaLayerShellConfig config;
    config.scope = QStringLiteral("astrea-screenshot-thumbnail");
    config.layer = AstreaLayerShellConfig::Layer::Overlay;
    config.keyboardInteractivity = AstreaLayerShellConfig::KeyboardInteractivity::None;
    config.anchorTop = true;
    config.anchorBottom = true;
    config.anchorLeft = true;
    config.anchorRight = true;
    config.exclusiveZone = -1;
    config.screen = screen;

    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = QUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Shell/Screenshot/ScreenshotThumbnail.qml"));
    request.screen = screen;
    request.size = screen->geometry().size();
    request.requireSurfaceReadyGate = true;
    request.configure = [config](QQuickWindow *window, QString *error) {
        return AstreaLayerShellHelper::configure(window, config, error);
    };
    request.install = [this](QQuickWindow *window, QString *) {
        m_screenshotInputRegion->setWindow(window);
        refreshScreenshotBarrierWindows();
        return true;
    };
    request.beforeDestroy = [this](QQuickWindow *window) {
        m_screenshotInputRegion->setWindow(nullptr);
        refreshScreenshotBarrierWindows(window);
    };
    QString error;
    if (!m_screenshotThumbnailSlot->ensureCreated(m_engine.get(), request, &error)) {
        qCritical("Astrea shell screenshot thumbnail creation failed: %s", qPrintable(error));
        m_application.exit(1);
    }
}

void AstreaShellApplication::syncScreenshotPreviewSurface()
{
    if (!m_runtime || !m_engine || !m_screenshotPreviewSlot)
        return;
    if (!m_runtime->screenshotController()->previewVisible()) {
        m_screenshotPreviewSlot->hide();
        QTimer::singleShot(0, this, [this] {
            if (!m_runtime || !m_screenshotPreviewSlot
                || m_runtime->screenshotController()->previewVisible())
                return;
            m_screenshotPreviewSlot->destroyWhenSafe();
        });
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    AstreaLayerShellConfig config;
    config.scope = QStringLiteral("astrea-screenshot-preview");
    config.layer = AstreaLayerShellConfig::Layer::Overlay;
    config.keyboardInteractivity = AstreaLayerShellConfig::KeyboardInteractivity::Exclusive;
    config.anchorTop = true;
    config.anchorBottom = true;
    config.anchorLeft = true;
    config.anchorRight = true;
    config.exclusiveZone = -1;
    config.screen = screen;

    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = QUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Shell/Screenshot/ScreenshotPreview.qml"));
    request.screen = screen;
    request.size = screen->geometry().size();
    request.requireSurfaceReadyGate = true;
    request.configure = [config](QQuickWindow *window, QString *error) {
        return AstreaLayerShellHelper::configure(window, config, error);
    };
    request.install = [this](QQuickWindow *, QString *) {
        refreshScreenshotBarrierWindows();
        return true;
    };
    request.beforeDestroy = [this](QQuickWindow *window) {
        refreshScreenshotBarrierWindows(window);
    };
    QString error;
    if (!m_screenshotPreviewSlot->ensureCreated(m_engine.get(), request, &error)) {
        qCritical("Astrea shell screenshot preview creation failed: %s", qPrintable(error));
        m_application.exit(1);
    }
}

void AstreaShellApplication::refreshScreenshotBarrierWindows(QQuickWindow *excluded)
{
    if (!m_screenshotUiBarrier)
        return;
    auto currentWindow = [excluded](const std::unique_ptr<TransientQmlWindowSlot> &slot) {
        QQuickWindow *window = slot ? slot->window() : nullptr;
        return window == excluded ? nullptr : window;
    };
    m_screenshotUiBarrier->setTrackedWindows({
        currentWindow(m_screenshotOverlaySlot),
        currentWindow(m_screenshotThumbnailSlot),
        currentWindow(m_screenshotPreviewSlot),
    });
}

QJsonObject AstreaShellApplication::surfaceResidencyJson() const
{
    int liveWindowCount = 0;
    int visibleWindowCount = 0;
    qint64 livePixelArea = 0;
    qint64 hiddenPixelArea = 0;
    for (QWindow *nativeWindow : QGuiApplication::allWindows()) {
        auto *window = qobject_cast<QQuickWindow *>(nativeWindow);
        if (!window)
            continue;
        ++liveWindowCount;
        const qint64 area = qint64(qMax(0, window->width())) * qMax(0, window->height());
        livePixelArea += area;
        if (window->isVisible())
            ++visibleWindowCount;
        else
            hiddenPixelArea += area;
    }

    const QVariantMap barPopup = m_barSurfaceManager
        ? m_barSurfaceManager->popupResidency() : emptyTransientSurfaceSnapshot();
    const QVariantMap contextOverlay = m_contextMenuSurfaceManager
        ? m_contextMenuSurfaceManager->overlayResidency() : emptyTransientSurfaceSnapshot();
    const QVariantMap altTab = m_altTabSlot
        ? m_altTabSlot->residencySnapshot() : emptyTransientSurfaceSnapshot();
    const QVariantMap spotlight = m_spotlightSlot
        ? m_spotlightSlot->residencySnapshot() : emptyTransientSurfaceSnapshot();
    const QVariantMap screenshotOverlay = m_screenshotOverlaySlot
        ? m_screenshotOverlaySlot->residencySnapshot() : emptyTransientSurfaceSnapshot();
    const QVariantMap screenshotThumbnail = m_screenshotThumbnailSlot
        ? m_screenshotThumbnailSlot->residencySnapshot() : emptyTransientSurfaceSnapshot();
    const QVariantMap screenshotPreview = m_screenshotPreviewSlot
        ? m_screenshotPreviewSlot->residencySnapshot() : emptyTransientSurfaceSnapshot();
    const QList<QVariantMap> transientSnapshots{
        barPopup, contextOverlay, altTab, spotlight,
        screenshotOverlay, screenshotThumbnail, screenshotPreview,
    };
    int transientCreatedCount = 0;
    int transientVisibleCount = 0;
    for (const QVariantMap &snapshot : transientSnapshots) {
        transientCreatedCount += snapshot.value(QStringLiteral("liveInstances"),
            snapshot.value(QStringLiteral("created")).toBool() ? 1 : 0).toInt();
        transientVisibleCount += snapshot.value(QStringLiteral("visibleInstances"),
            snapshot.value(QStringLiteral("visible")).toBool() ? 1 : 0).toInt();
    }

    return {
        {QStringLiteral("liveWindowCount"), liveWindowCount},
        {QStringLiteral("visibleWindowCount"), visibleWindowCount},
        {QStringLiteral("hiddenWindowCount"), liveWindowCount - visibleWindowCount},
        {QStringLiteral("livePixelArea"), livePixelArea},
        {QStringLiteral("hiddenPixelArea"), hiddenPixelArea},
        {QStringLiteral("transientCreatedCount"), transientCreatedCount},
        {QStringLiteral("transientVisibleCount"), transientVisibleCount},
        {QStringLiteral("surfaces"), QJsonObject{
            {QStringLiteral("barPopup"), transientSurfaceJson(barPopup)},
            {QStringLiteral("contextMenuOverlay"), transientSurfaceJson(contextOverlay)},
            {QStringLiteral("altTab"), transientSurfaceJson(altTab)},
            {QStringLiteral("spotlight"), transientSurfaceJson(spotlight)},
            {QStringLiteral("screenshotOverlay"), transientSurfaceJson(screenshotOverlay)},
            {QStringLiteral("screenshotThumbnail"), transientSurfaceJson(screenshotThumbnail)},
            {QStringLiteral("screenshotPreview"), transientSurfaceJson(screenshotPreview)},
        }},
    };
}

bool AstreaShellApplication::configureDockSurface()
{
    if (!m_dockWindow || !m_runtime || !m_runtime->dockConfig())
        return false;
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        qCritical("Astrea shell Dock has no output screen");
        return false;
    }
    QString error;
    const bool configured = DockLayerShellSurface::configure(
        m_dockWindow, m_runtime->dockController()->surfacePlacement(),
        m_runtime->dockController()->exclusiveZone(),
        screen, &error);
    if (!configured) {
        qCritical("Astrea shell Dock Layer Shell setup failed: %s", qPrintable(error));
        m_application.exit(1);
        return false;
    }
    if (!DockLayerShellSurface::configureTooltip(m_dockTooltipWindow, screen, &error)) {
        qCritical("Astrea shell Dock tooltip Layer Shell setup failed: %s", qPrintable(error));
        m_application.exit(1);
        return false;
    }
    if (m_dockScreen != screen) {
        QObject::disconnect(m_dockGeometryConnection);
        m_dockScreen = screen;
        m_dockGeometryConnection = connect(
            screen, &QScreen::geometryChanged, this,
            [this](const QRect &) { updateDockOutputGeometry(); });
    }
    m_dockLayerConfigurationRequested = true;
    return true;
}

void AstreaShellApplication::updateDockExclusiveZone()
{
    if (!m_dockWindow || !m_dockLayerConfigurationRequested)
        return;
    QString error;
    if (!DockLayerShellSurface::updateExclusiveZone(
            m_dockWindow, m_runtime->dockController()->exclusiveZone(), &error)) {
        qCritical("Astrea shell Dock exclusive zone update failed: %s", qPrintable(error));
        m_application.exit(1);
    }
}

void AstreaShellApplication::updateDockOutputGeometry()
{
    if (!m_dockWindow || !m_dockTooltipWindow || !m_dockScreen)
        return;
    QString error;
    if (!DockLayerShellSurface::setOutputGeometry(m_dockWindow, m_dockScreen, &error)) {
        qCritical("Astrea shell Dock output geometry update failed: %s", qPrintable(error));
        m_application.exit(1);
        return;
    }
    if (!DockLayerShellSurface::setOutputGeometry(m_dockTooltipWindow, m_dockScreen, &error)) {
        qCritical("Astrea shell Dock tooltip output geometry update failed: %s",
                  qPrintable(error));
        m_application.exit(1);
    }
}

void AstreaShellApplication::syncDockVisibility()
{
    if (!m_dockWindow)
        return;
    const bool visible = m_runtime->dockController()->visible();
    if (!m_dockLayerConfigurationRequested) {
        qCritical("Astrea shell Dock cannot change visibility before Layer Shell setup");
        m_application.exit(1);
        return;
    }
    QString error;
    if (!DockLayerShellSurface::setMapped(m_dockWindow, visible,
                                          m_runtime->dockController()->exclusiveZone(), &error)) {
        qCritical("Astrea shell Dock Layer Shell mapping failed: %s", qPrintable(error));
        m_application.exit(1);
    }
}

void AstreaShellApplication::handleCommand(const ShellIpcServer::Command &command)
{
    if (!command.valid)
        return;

    if (command.feature == QStringLiteral("shell")) {
        if (command.action == QStringLiteral("quit"))
            m_application.quit();
        else if (command.action == QStringLiteral("reload")) {
            m_runtime->reloadCatalog();
            m_runtime->reloadDockConfig();
            m_runtime->reloadAltTabConfig();
            m_runtime->reloadSpotlightConfig();
        }
        return;
    }

    if (command.feature == QStringLiteral("dock")) {
        if (command.action == QStringLiteral("live")) {
            if (command.argument.size() > 3000)
                return;
            const QByteArray json = QByteArray::fromBase64(command.argument.toLatin1());
            if (json.size() > 2048)
                return;
            const QJsonDocument document = QJsonDocument::fromJson(json);
            if (!document.isObject())
                return;
            QStringList warnings;
            DockConfig next = DockConfigCodec::parse(document.object(), &warnings);
            if (!warnings.isEmpty()
                || DockConfigCodec::patchKnownFields({}, next, false) != document.object())
                return;
            next.pins = m_runtime->dockController()->configSnapshot().pins;
            m_runtime->dockController()->applyConfig(next);
        } else if (command.action == QStringLiteral("sync")) {
            m_runtime->reloadDockConfig();
        } else if (command.action == QStringLiteral("reload")) {
            m_runtime->reloadDockConfig();
            m_runtime->reloadCatalog();
        } else if (command.action == QStringLiteral("show")) {
            m_runtime->dockController()->show();
        } else if (command.action == QStringLiteral("hide")) {
            m_runtime->dockController()->hide();
        } else if (command.action == QStringLiteral("quit")) {
            m_application.quit();
        }
        return;
    }

    if (command.feature == QStringLiteral("alttab")) {
        if (command.action == QStringLiteral("next"))
            m_runtime->altTabController()->step(1);
        else if (command.action == QStringLiteral("previous"))
            m_runtime->altTabController()->step(-1);
        else if (command.action == QStringLiteral("commit"))
            m_runtime->altTabController()->commit();
        else if (command.action == QStringLiteral("cancel")
                 || command.action == QStringLiteral("hide"))
            m_runtime->altTabController()->cancel();
        else if (command.action == QStringLiteral("show"))
            m_runtime->altTabController()->show();
        else if (command.action == QStringLiteral("reload"))
            m_runtime->reloadAltTabConfig();
        return;
    }

    if (command.feature == QStringLiteral("screenshot-config")
        && command.action == QStringLiteral("live")) {
        auto *screenshot = m_runtime->screenshotController();
        if (screenshot->demonstrationActive()) {
            const QByteArray payload = QByteArray::fromBase64(command.argument.toLatin1());
            screenshot->configuration()->applyTransientPayload(payload);
        }
        return;
    }

    if (command.feature == QStringLiteral("screenshot-demo")) {
        auto *screenshot = m_runtime->screenshotController();
        if (command.action == QStringLiteral("start"))
            screenshot->startDemonstration();
        else if (command.action == QStringLiteral("stop"))
            screenshot->requestDemonstrationStop();
        else if (command.action == QStringLiteral("replay"))
            screenshot->replayDemonstration();
        else if (command.action == QStringLiteral("renew"))
            screenshot->renewDemonstration();
        return;
    }

    if (command.feature == QStringLiteral("spotlight")) {
        if (command.action == QStringLiteral("show"))
            m_runtime->spotlightController()->show();
        else if (command.action == QStringLiteral("hide"))
            m_runtime->spotlightController()->close();
        else if (command.action == QStringLiteral("toggle"))
            m_runtime->spotlightController()->toggle();
        else if (command.action == QStringLiteral("query")) {
            m_runtime->spotlightController()->show();
            m_runtime->spotlightController()->setQuery(command.argument);
        } else if (command.action == QStringLiteral("activate")) {
            m_runtime->spotlightController()->activateCurrent();
        } else if (command.action == QStringLiteral("reload")) {
            m_runtime->reloadSpotlightConfig();
            m_runtime->spotlightController()->reloadIndex();
            m_runtime->reloadCatalog();
        }
    }
}

QString AstreaShellApplication::statusJson() const
{
    const auto *runtime = m_runtime.get();
    const auto *dock = runtime->dockController();
    const auto *altTab = runtime->altTabController();
    const auto *spotlight = runtime->spotlightController();
    const auto *bar = runtime->barController();
    const auto *screenshot = runtime->screenshotController();
    const auto *captureClient = runtime->screenCaptureClient();
    const auto *shortcutClient = runtime->shortcutClient();
    const QJsonObject object{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("running"), true},
        {QStringLiteral("catalogRevision"), runtime->catalog()->revision()},
        {QStringLiteral("layerShell"), QJsonObject{
            {QStringLiteral("compiled"), AstreaLayerShellHelper::compiled()},
            {QStringLiteral("required"), true},
            {QStringLiteral("platform"), m_application.platformName()},
            {QStringLiteral("waylandBackend"), m_layerShellWaylandBackend},
            {QStringLiteral("protocolAdvertised"), m_layerShellProtocolAdvertised},
            {QStringLiteral("dockConfigurationRequested"), m_dockLayerConfigurationRequested},
            {QStringLiteral("altTabConfigurationRequested"),
             m_altTabSlot && m_altTabSlot->created()},
            {QStringLiteral("spotlightConfigurationRequested"),
             m_spotlightSlot && m_spotlightSlot->created()},
            {QStringLiteral("screenshotConfigurationRequested"),
             m_screenshotOverlaySlot && m_screenshotOverlaySlot->created()},
            {QStringLiteral("screenshotThumbnailConfigurationRequested"),
             m_screenshotThumbnailSlot && m_screenshotThumbnailSlot->created()},
            {QStringLiteral("screenshotPreviewConfigurationRequested"),
             m_screenshotPreviewSlot && m_screenshotPreviewSlot->created()},
            {QStringLiteral("barConfigurationRequested"),
             m_barSurfaceManager && m_barSurfaceManager->layerConfigurationRequested()}}},
        {QStringLiteral("surfaceResidency"), surfaceResidencyJson()},
        {QStringLiteral("dock"), QJsonObject{
            {QStringLiteral("visible"), dock->visible()},
            {QStringLiteral("enabled"), dock->enabled()},
            {QStringLiteral("pins"), dock->pinCount()}}},
        {QStringLiteral("alttab"), QJsonObject{
            {QStringLiteral("open"), altTab->isOpen()},
            {QStringLiteral("windows"), altTab->windowCount()}}},
        {QStringLiteral("spotlight"), QJsonObject{
            {QStringLiteral("open"), spotlight->isOpen()},
            {QStringLiteral("results"), spotlight->resultCount()}}},
        {QStringLiteral("screenshot"), ShellRuntimeStatus::screenshot(*screenshot,
                                                                         *captureClient)},
        {QStringLiteral("shortcuts"), ShellRuntimeStatus::shortcuts(*shortcutClient)},
        {QStringLiteral("bar"), QJsonObject{
            {QStringLiteral("enabled"), bar && bar->enabled()},
            {QStringLiteral("outputBundles"),
             m_barSurfaceManager ? m_barSurfaceManager->bundleCount() : 0},
            {QStringLiteral("popupOpen"),
             m_barSurfaceManager && m_barSurfaceManager->popupOpen()},
            {QStringLiteral("layerConfigurationRequested"),
             m_barSurfaceManager && m_barSurfaceManager->layerConfigurationRequested()}}},
        {QStringLiteral("system"), QJsonObject{
            {QStringLiteral("audio"), runtime->audioService()->healthJson()},
            {QStringLiteral("spatialAudio"), runtime->spatialAudioEngine()->healthJson()},
            {QStringLiteral("network"), runtime->networkService()->healthJson()},
            {QStringLiteral("bluetooth"), runtime->bluetoothService()->healthJson()}}},
        {QStringLiteral("statusNotifier"), runtime->statusNotifier()->healthJson()}
    };
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}
