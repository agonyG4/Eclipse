#include "ContextMenuSurfaceBundle.hpp"

#include "core/ContextMenuController.hpp"
#include "core/ContextMenuSurfaceMapping.hpp"
#include "core/ContextMenuSurfacePolicy.hpp"
#include "platform/wayland/LayerShellHelper.hpp"
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

ContextMenuSurfaceBundle::ContextMenuSurfaceBundle(QScreen *screen,
                                                   QQmlApplicationEngine *engine,
                                                   Astrea::Shell::ContextMenuController *controller,
                                                   QObject *parent,
                                                   Astrea::StatusNotifier::StatusNotifierService *statusNotifier)
    : QObject(parent)
    , m_screen(screen)
    , m_engine(engine)
    , m_controller(controller)
    , m_statusNotifier(statusNotifier)
    , m_overlaySlot(QStringLiteral("contextMenuOverlay"))
{
    if (m_controller) {
        connect(m_controller, &Astrea::Shell::ContextMenuController::presentationChanged,
                this, &ContextMenuSurfaceBundle::syncOverlayMapping);
        connect(m_controller, &Astrea::Shell::ContextMenuController::lifecycleChanged,
                this, &ContextMenuSurfaceBundle::syncOverlayMapping);
    }
}

ContextMenuSurfaceBundle::~ContextMenuSurfaceBundle()
{
    destroySurfaces();
}

QString ContextMenuSurfaceBundle::outputKey() const
{
    return m_screen ? m_screen->name() : QString();
}

bool ContextMenuSurfaceBundle::initialize(QString *errorOut)
{
    if (m_layerConfigurationRequested)
        return true;
    if (!m_screen || !m_engine || !m_controller) {
        if (errorOut)
            *errorOut = QStringLiteral("Context menu surface requires screen, engine, and controller");
        return false;
    }

    const QRect geometry = m_screen->geometry();
    const int width = qMax(1, geometry.width());
    const int height = qMax(1, geometry.height());
    const QVariantMap commonProperties{
        {QStringLiteral("contextMenuController"), objectVariant(m_controller)},
        {QStringLiteral("outputKey"), outputKey()},
        {QStringLiteral("outputWidth"), width},
        {QStringLiteral("outputHeight"), height},
        {QStringLiteral("outputOriginX"), geometry.x()},
        {QStringLiteral("outputOriginY"), geometry.y()},
    };
    m_desktopInteractionWindow = createSurface(
        QUrl(QStringLiteral("qrc:/qt/qml/Astrea/Shell/ContextMenu/qml/DesktopInteractionSurface.qml")),
        commonProperties, errorOut);
    if (!m_desktopInteractionWindow) {
        destroySurfaces();
        return false;
    }
    if (!configureSurface(m_desktopInteractionWindow, false, errorOut)) {
        destroySurfaces();
        return false;
    }
    m_layerConfigurationRequested = true;
    updateForScreen();
    return true;
}

void ContextMenuSurfaceBundle::map()
{
    if (!m_layerConfigurationRequested || m_mapped)
        return;
    m_mapped = true;
    if (m_desktopInteractionWindow)
        m_desktopInteractionWindow->show();
    syncOverlayMapping();
}

void ContextMenuSurfaceBundle::updateForScreen()
{
    if (!m_screen)
        return;
    const QRect geometry = m_screen->geometry();
    const int width = qMax(1, geometry.width());
    const int height = qMax(1, geometry.height());
    for (QQuickWindow *window : {m_desktopInteractionWindow.data(), m_overlaySlot.window()}) {
        if (!window)
            continue;
        window->setProperty("outputWidth", width);
        window->setProperty("outputHeight", height);
        window->setProperty("outputOriginX", geometry.x());
        window->setProperty("outputOriginY", geometry.y());
        window->setScreen(m_screen.data());
        window->resize(width, height);
    }
}

void ContextMenuSurfaceBundle::destroySurfaces()
{
    m_mapped = false;
    m_overlaySlot.destroyNow();
    destroyWindow(m_desktopInteractionWindow);
    m_layerConfigurationRequested = false;
    m_screen.clear();
}

int ContextMenuSurfaceBundle::surfaceCount() const
{
    return static_cast<int>(!m_desktopInteractionWindow.isNull())
        + static_cast<int>(m_overlaySlot.created());
}

bool ContextMenuSurfaceBundle::overlayMapped() const
{
    return m_overlaySlot.window() && m_overlaySlot.window()->isVisible();
}

QQuickWindow *ContextMenuSurfaceBundle::createOverlaySurface(QString *errorOut)
{
    if (!m_engine || !m_screen || !m_controller)
        return nullptr;
    const QRect geometry = m_screen->geometry();
    const int width = qMax(1, geometry.width());
    const int height = qMax(1, geometry.height());
    TransientQmlWindowSlot::CreateRequest request;
    request.sourceUrl = QUrl(QStringLiteral(
        "qrc:/qt/qml/Astrea/Shell/ContextMenu/qml/ContextMenuOverlaySurface.qml"));
    request.screen = m_screen.data();
    request.size = QSize(width, height);
    request.initialProperties = {
        {QStringLiteral("contextMenuController"), objectVariant(m_controller)},
        {QStringLiteral("outputKey"), outputKey()},
        {QStringLiteral("outputWidth"), width},
        {QStringLiteral("outputHeight"), height},
        {QStringLiteral("outputOriginX"), geometry.x()},
        {QStringLiteral("outputOriginY"), geometry.y()},
        {QStringLiteral("trayService"), objectVariant(m_statusNotifier)},
    };
    request.configure = [this](QQuickWindow *window, QString *error) {
        return configureSurface(window, true, error);
    };
    return m_overlaySlot.ensureCreated(m_engine, request, errorOut);
}

QQuickWindow *ContextMenuSurfaceBundle::createSurface(const QUrl &sourceUrl,
                                                       const QVariantMap &properties,
                                                       QString *errorOut)
{
    QQmlComponent component(m_engine, sourceUrl, this);
    if (component.status() != QQmlComponent::Ready) {
        if (errorOut)
            *errorOut = component.errors().isEmpty()
                ? QStringLiteral("Context menu QML component is not ready")
                : component.errors().constFirst().toString();
        return nullptr;
    }
    QObject *object = component.createWithInitialProperties(properties, m_engine->rootContext());
    auto *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        if (object)
            object->deleteLater();
        if (errorOut)
            *errorOut = QStringLiteral("Context menu QML component did not create a QQuickWindow");
        return nullptr;
    }
    QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);
    window->setScreen(m_screen.data());
    window->setVisible(false);
    window->resize(properties.value(QStringLiteral("outputWidth")).toInt(),
                   properties.value(QStringLiteral("outputHeight")).toInt());
    return window;
}

bool ContextMenuSurfaceBundle::configureSurface(QQuickWindow *window, bool overlay,
                                                QString *errorOut)
{
    if (!window || !m_screen)
        return false;
    if (!overlay)
        window->setFlag(Qt::WindowDoesNotAcceptFocus, true);
    const auto policy = overlay ? Astrea::Shell::ContextMenuSurfacePolicy::overlay()
                                : Astrea::Shell::ContextMenuSurfacePolicy::desktopInteraction();
    auto config = policy;
    config.screen = m_screen.data();
    if (!AstreaLayerShellHelper::configure(window, config, errorOut))
        return false;
    window->setProperty("_astrea_layer_shell", true);
    return true;
}

void ContextMenuSurfaceBundle::syncOverlayMapping()
{
    if (!m_layerConfigurationRequested)
        return;
    const bool wasMapped = overlayMapped();
    const bool shouldMap = Astrea::Shell::ContextMenuSurfaceMapping::overlayShouldMap(
        outputKey(), m_mapped, m_controller && m_controller->hasActivePresentation(),
        m_controller ? m_controller->outputKey() : QString());
    if (shouldMap) {
        QString error;
        QQuickWindow *window = m_overlaySlot.window();
        if (window) {
            m_overlaySlot.cancelPendingDestruction();
        } else {
            window = createOverlaySurface(&error);
        }
        if (!window) {
            emit surfaceError(error.isEmpty()
                ? QStringLiteral("Context menu overlay surface creation failed") : error);
            return;
        }
        window->setVisible(true);
    } else {
        m_overlaySlot.destroyWhenSafe();
    }
    if (wasMapped != shouldMap)
        emit mappingChanged();
}

void ContextMenuSurfaceBundle::destroyWindow(QPointer<QQuickWindow> &window)
{
    if (!window)
        return;
    window->setVisible(false);
    window->close();
    delete window.data();
    window.clear();
}
