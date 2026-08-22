#include "platform/wayland/BarSurfaceBundle.hpp"

#include "core/BarClockService.hpp"
#include "core/BarController.hpp"
#include "core/BarLayoutMetrics.hpp"
#include "core/BarPopupController.hpp"
#include "core/BarSurfacePolicy.hpp"
#include "core/WorkspaceModel.hpp"
#include "platform/wayland/LayerShellHelper.hpp"
#include "system/audio/AudioService.hpp"
#include "system/network/NetworkService.hpp"
#include "system/bluetooth/BluetoothService.hpp"
#include "statusnotifier/StatusNotifierService.hpp"

#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QScreen>
#include <QUrl>
#include <QVariant>
#include <QVariantMap>

namespace {

QVariant objectVariant(QObject *object)
{
    return QVariant::fromValue(object);
}

} // namespace

BarSurfaceBundle::BarSurfaceBundle(QScreen *screen, QQmlApplicationEngine *engine,
                                   BarController *barController,
                                   BarClockService *clockService,
                                   WorkspaceModel *workspaceModel,
                                   Astrea::System::AudioService *audioService,
                                   Astrea::System::NetworkService *networkService,
                                   Astrea::System::BluetoothService *bluetoothService,
                                   QObject *parent,
                                   Astrea::StatusNotifier::StatusNotifierService *statusNotifier)
    : QObject(parent)
    , m_screen(screen)
    , m_engine(engine)
    , m_barController(barController)
    , m_clockService(clockService)
    , m_workspaceModel(workspaceModel)
    , m_audioService(audioService)
    , m_networkService(networkService)
    , m_bluetoothService(bluetoothService)
    , m_statusNotifier(statusNotifier)
    , m_popupController(new BarPopupController(this))
    , m_layoutMetrics(new BarLayoutMetrics(this))
{
    connect(m_popupController, &BarPopupController::changed,
            this, &BarSurfaceBundle::syncPopupMapping);
    connect(m_popupController, &BarPopupController::changed,
            this, &BarSurfaceBundle::popupStateChanged);
}

BarSurfaceBundle::~BarSurfaceBundle()
{
    destroySurfaces();
}

bool BarSurfaceBundle::initialize(QString *errorOut)
{
    if (m_layerConfigurationRequested)
        return true;
    if (!m_screen || !m_engine) {
        if (errorOut)
            *errorOut = QStringLiteral("Bar surface bundle requires a screen and QML engine");
        return false;
    }

    const QRect geometry = m_screen->geometry();
    const int outputWidth = qMax(1, geometry.width());
    const int outputHeight = qMax(1, geometry.height());
    m_reserveWindow = createSurface(
        QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Bar/qml/ReserveSurface.qml")),
        outputWidth, m_layoutMetrics->barHeight(), errorOut, true);
    m_launcherWindow = createSurface(
        QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Bar/qml/LauncherSurface.qml")),
        outputWidth, m_layoutMetrics->pillHeight(), errorOut, false);
    m_statusWindow = createSurface(
        QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Bar/qml/StatusSurface.qml")),
        outputWidth, m_layoutMetrics->pillHeight(), errorOut, false);
    m_tooltipWindow = createSurface(
        QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Bar/qml/TrayTooltipSurface.qml")),
        outputWidth, BarSurfacePolicy::kTrayTooltipHeight, errorOut, false);
    m_popupWindow = createSurface(
        QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/Bar/qml/PopupOverlaySurface.qml")),
        outputWidth, outputHeight, errorOut, true);

    if (!m_reserveWindow || !m_launcherWindow || !m_statusWindow || !m_tooltipWindow
        || !m_popupWindow) {
        destroySurfaces();
        return false;
    }

    connect(m_launcherWindow, &QWindow::widthChanged, this, [this] {
        if (m_statusWindow && m_launcherWindow)
            m_statusWindow->setProperty("launcherWidth", m_launcherWindow->width());
    });
    m_statusWindow->setProperty("trayTooltipSurface", objectVariant(m_tooltipWindow));
    connect(m_tooltipWindow, SIGNAL(tooltipVisibleChanged()), this,
            SLOT(syncTooltipMapping()));

    const QList<QPair<QQuickWindow *, BarSurfaceKind>> surfaces{
        {m_reserveWindow, BarSurfaceKind::Reserve},
        {m_launcherWindow, BarSurfaceKind::Launcher},
        {m_statusWindow, BarSurfaceKind::Status},
        {m_tooltipWindow, BarSurfaceKind::TrayTooltip},
        {m_popupWindow, BarSurfaceKind::PopupOverlay},
    };
    for (const auto &[window, kind] : surfaces) {
        if (!configureSurface(window, kind, errorOut)) {
            destroySurfaces();
            return false;
        }
    }
    m_layerConfigurationRequested = true;
    updateForScreen();
    return true;
}

void BarSurfaceBundle::map()
{
    if (!m_layerConfigurationRequested || m_mapped)
        return;
    if (m_barEnabled) {
        if (m_reserveWindow)
            m_reserveWindow->show();
        if (m_launcherWindow)
            m_launcherWindow->show();
        if (m_statusWindow)
            m_statusWindow->show();
    }
    m_mapped = true;
    syncPopupMapping();
    syncTooltipMapping();
}

void BarSurfaceBundle::setBarEnabled(bool enabled)
{
    if (m_barEnabled == enabled)
        return;
    m_barEnabled = enabled;
    if (!m_barEnabled) {
        if (m_popupController)
            m_popupController->clearForOutput();
        if (m_reserveWindow)
            m_reserveWindow->hide();
        if (m_launcherWindow)
            m_launcherWindow->hide();
        if (m_statusWindow)
            m_statusWindow->hide();
        if (m_popupWindow)
            m_popupWindow->hide();
        if (m_tooltipWindow)
            m_tooltipWindow->hide();
    } else if (m_mapped) {
        if (m_reserveWindow)
            m_reserveWindow->show();
        if (m_launcherWindow)
            m_launcherWindow->show();
        if (m_statusWindow)
            m_statusWindow->show();
        syncPopupMapping();
        syncTooltipMapping();
    }
}

void BarSurfaceBundle::destroySurfaces()
{
    if (m_popupController)
        m_popupController->clearForOutput();
    m_mapped = false;
    destroyWindow(m_popupWindow);
    destroyWindow(m_tooltipWindow);
    destroyWindow(m_statusWindow);
    destroyWindow(m_launcherWindow);
    destroyWindow(m_reserveWindow);
    m_layerConfigurationRequested = false;
    m_screen.clear();
}

void BarSurfaceBundle::updateForScreen()
{
    if (!m_screen)
        return;
    const QRect geometry = m_screen->geometry();
    const int width = qMax(1, geometry.width());
    const int height = qMax(1, geometry.height());
    const QList<QPair<QQuickWindow *, QSize>> sizes{
        {m_reserveWindow, QSize(width, m_layoutMetrics->barHeight())},
        {m_launcherWindow, QSize(width, m_layoutMetrics->pillHeight())},
        {m_statusWindow, QSize(width, m_layoutMetrics->pillHeight())},
        {m_popupWindow, QSize(width, height)},
        {m_tooltipWindow, QSize(width, BarSurfacePolicy::kTrayTooltipHeight)},
    };
    for (const auto &[window, size] : sizes) {
        if (!window)
            continue;
        window->setProperty("outputWidth", width);
        window->setProperty("outputHeight", height);
        if (window == m_statusWindow && m_launcherWindow)
            window->setProperty("launcherWidth", m_launcherWindow->width());
        window->setScreen(m_screen.data());
        if (window == m_popupWindow || window == m_tooltipWindow)
            window->resize(size);
    }
}

int BarSurfaceBundle::surfaceCount() const
{
    return static_cast<int>(!m_reserveWindow.isNull())
        + static_cast<int>(!m_launcherWindow.isNull())
        + static_cast<int>(!m_statusWindow.isNull())
        + static_cast<int>(!m_tooltipWindow.isNull())
        + static_cast<int>(!m_popupWindow.isNull());
}

bool BarSurfaceBundle::popupOpen() const
{
    return m_popupController && m_popupController->isOpen();
}

QQuickWindow *BarSurfaceBundle::createSurface(const QUrl &sourceUrl, int width, int height,
                                              QString *errorOut, bool sizeWindow)
{
    if (!m_engine)
        return nullptr;
    QQmlComponent component(m_engine, sourceUrl, this);
    if (component.status() != QQmlComponent::Ready) {
        if (errorOut)
            *errorOut = component.errors().isEmpty()
                ? QStringLiteral("Bar QML component is not ready")
                : component.errors().constFirst().toString();
        return nullptr;
    }

    QVariantMap properties{
        {QStringLiteral("barController"), objectVariant(m_barController)},
        {QStringLiteral("clockService"), objectVariant(m_clockService)},
        {QStringLiteral("workspaceModel"), objectVariant(m_workspaceModel)},
        {QStringLiteral("audioService"), objectVariant(m_audioService)},
        {QStringLiteral("networkService"), objectVariant(m_networkService)},
        {QStringLiteral("bluetoothService"), objectVariant(m_bluetoothService)},
        {QStringLiteral("statusNotifierService"), objectVariant(m_statusNotifier)},
        {QStringLiteral("popupController"), objectVariant(m_popupController)},
        {QStringLiteral("barGeometry"), objectVariant(m_layoutMetrics)},
        {QStringLiteral("outputWidth"), width},
        {QStringLiteral("outputHeight"), height},
    };
    QObject *object = component.createWithInitialProperties(properties, m_engine->rootContext());
    auto *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        if (object)
            object->deleteLater();
        if (errorOut)
            *errorOut = QStringLiteral("Bar QML component did not create a QQuickWindow");
        return nullptr;
    }
    QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);
    window->setScreen(m_screen.data());
    window->setVisible(false);
    if (sizeWindow)
        window->resize(width, height);
    return window;
}

bool BarSurfaceBundle::configureSurface(QQuickWindow *window, BarSurfaceKind kind,
                                        QString *errorOut)
{
    if (!window || !m_screen)
        return false;
    if (kind == BarSurfaceKind::Reserve) {
        window->setFlag(Qt::WindowTransparentForInput, true);
        window->setFlag(Qt::WindowDoesNotAcceptFocus, true);
    }
    if (kind == BarSurfaceKind::TrayTooltip) {
        window->setFlag(Qt::WindowTransparentForInput, true);
        window->setFlag(Qt::WindowDoesNotAcceptFocus, true);
    }
    AstreaLayerShellConfig config;
    switch (kind) {
    case BarSurfaceKind::Reserve: config = BarSurfacePolicy::reserve(); break;
    case BarSurfaceKind::Launcher: config = BarSurfacePolicy::launcher(); break;
    case BarSurfaceKind::Status: config = BarSurfacePolicy::status(); break;
    case BarSurfaceKind::PopupOverlay: config = BarSurfacePolicy::popupOverlay(); break;
    case BarSurfaceKind::TrayTooltip: config = BarSurfacePolicy::trayTooltip(); break;
    }
    config.screen = m_screen.data();
    if (!AstreaLayerShellHelper::configure(window, config, errorOut))
        return false;
    window->setProperty("_astrea_layer_shell", true);
    return true;
}

void BarSurfaceBundle::syncTooltipMapping()
{
    if (!m_tooltipWindow || !m_layerConfigurationRequested)
        return;
    m_tooltipWindow->setVisible(m_mapped && m_barEnabled
                                && m_tooltipWindow->property("tooltipVisible").toBool());
}

void BarSurfaceBundle::syncPopupMapping()
{
    if (!m_popupWindow || !m_layerConfigurationRequested)
        return;
    m_popupWindow->setVisible(m_mapped && m_popupController
                              && m_popupController->surfaceRequired());
}

void BarSurfaceBundle::destroyWindow(QPointer<QQuickWindow> &window)
{
    if (!window)
        return;
    window->setVisible(false);
    window->close();
    delete window.data();
    window.clear();
}
