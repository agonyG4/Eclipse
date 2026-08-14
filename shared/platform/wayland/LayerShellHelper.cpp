#include "platform/wayland/LayerShellHelper.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQuickWindow>

#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
#include <LayerShellQt/Window>
#include <QtGui/qguiapplication_platform.h>
#include <wayland-client.h>

#include <cstring>
#endif

namespace {

void setError(QString *errorOut, const QString &message)
{
    if (errorOut)
        *errorOut = message;
}

} // namespace

bool AstreaLayerShellHelper::protocolAdvertised(QString *errorOut)
{
#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
    auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    if (!application) {
        setError(errorOut, QStringLiteral(
                            "Layer Shell protocol probe requires QGuiApplication"));
        return false;
    }

    // Use Qt's already-open display. A second wl_display connection would have
    // a separate registry and could produce a false capability result.
    const auto *waylandApplication =
        application->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!waylandApplication || !waylandApplication->display()) {
        setError(errorOut, QStringLiteral(
                            "Qt did not expose its Wayland display for the Layer Shell probe"));
        return false;
    }

    bool advertised = false;
    const wl_registry_listener listener{
        [](void *data, wl_registry *, uint32_t, const char *interface, uint32_t) {
            if (std::strcmp(interface, "zwlr_layer_shell_v1") == 0)
                *static_cast<bool *>(data) = true;
        },
        [](void *, wl_registry *, uint32_t) {},
    };
    wl_registry *registry = wl_display_get_registry(waylandApplication->display());
    if (!registry) {
        setError(errorOut, QStringLiteral("Qt could not create a Wayland registry for the Layer Shell probe"));
        return false;
    }
    if (wl_registry_add_listener(registry, &listener, &advertised) < 0) {
        wl_registry_destroy(registry);
        setError(errorOut, QStringLiteral("Qt could not listen to the Wayland registry for the Layer Shell probe"));
        return false;
    }
    if (wl_display_roundtrip(waylandApplication->display()) < 0) {
        wl_registry_destroy(registry);
        setError(errorOut, QStringLiteral("The Wayland registry roundtrip failed during the Layer Shell probe"));
        return false;
    }
    wl_registry_destroy(registry);

    if (!advertised) {
        setError(errorOut, QStringLiteral(
                            "Compositor does not advertise zwlr_layer_shell_v1"));
        return false;
    }
    return true;
#else
    setError(errorOut, QStringLiteral(
                        "LayerShellQt is disabled; astrea-shell requires Layer Shell surfaces"));
    return false;
#endif
}

bool AstreaLayerShellHelper::validateRuntime(bool waylandBackend, bool protocolAvailable,
                                             QString *errorOut)
{
    if (!compiled()) {
        setError(errorOut, QStringLiteral(
                            "LayerShellQt is disabled; astrea-shell requires Layer Shell surfaces"));
        return false;
    }
    if (!waylandBackend) {
        setError(errorOut, QStringLiteral(
                            "Astrea shell requires the Qt Wayland platform"));
        return false;
    }
    if (!protocolAvailable) {
        setError(errorOut, QStringLiteral(
                            "Compositor does not advertise zwlr_layer_shell_v1"));
        return false;
    }
    return true;
}

bool AstreaLayerShellHelper::configure(QQuickWindow *window,
                                        const AstreaLayerShellConfig &config,
                                        QString *errorOut)
{
    if (!window) {
        setError(errorOut, QStringLiteral("Null window"));
        return false;
    }
    if (config.scope.trimmed().isEmpty()) {
        setError(errorOut, QStringLiteral("Layer Shell scope is empty"));
        return false;
    }
    if (config.exclusiveZone < -1) {
        setError(errorOut, QStringLiteral("Layer Shell exclusive zone is invalid"));
        return false;
    }
    if (!config.anchorTop && !config.anchorBottom && !config.anchorLeft && !config.anchorRight) {
        setError(errorOut, QStringLiteral("Layer Shell requires at least one anchor"));
        return false;
    }

#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
    // LayerShellQt 6.4.5 derives the output from the QWindow when its wrapper
    // is created; newer Window::setScreen APIs are not part of our minimum.
    if (config.screen)
        window->setScreen(config.screen);

    auto *layerWindow = LayerShellQt::Window::get(window);
    if (!layerWindow) {
        setError(errorOut, QStringLiteral("Failed to create LayerShellQt::Window"));
        return false;
    }

    LayerShellQt::Window::Anchors anchors;
    if (config.anchorTop) anchors |= LayerShellQt::Window::AnchorTop;
    if (config.anchorBottom) anchors |= LayerShellQt::Window::AnchorBottom;
    if (config.anchorLeft) anchors |= LayerShellQt::Window::AnchorLeft;
    if (config.anchorRight) anchors |= LayerShellQt::Window::AnchorRight;

    const auto layer = [config] {
        switch (config.layer) {
        case AstreaLayerShellConfig::Layer::Background: return LayerShellQt::Window::LayerBackground;
        case AstreaLayerShellConfig::Layer::Bottom: return LayerShellQt::Window::LayerBottom;
        case AstreaLayerShellConfig::Layer::Top: return LayerShellQt::Window::LayerTop;
        case AstreaLayerShellConfig::Layer::Overlay: return LayerShellQt::Window::LayerOverlay;
        }
        return LayerShellQt::Window::LayerTop;
    }();
    const auto keyboard = [config] {
        switch (config.keyboardInteractivity) {
        case AstreaLayerShellConfig::KeyboardInteractivity::None:
            return LayerShellQt::Window::KeyboardInteractivityNone;
        case AstreaLayerShellConfig::KeyboardInteractivity::Exclusive:
            return LayerShellQt::Window::KeyboardInteractivityExclusive;
        case AstreaLayerShellConfig::KeyboardInteractivity::OnDemand:
            return LayerShellQt::Window::KeyboardInteractivityOnDemand;
        }
        return LayerShellQt::Window::KeyboardInteractivityNone;
    }();

    layerWindow->setScope(config.scope);
    layerWindow->setLayer(layer);
    layerWindow->setKeyboardInteractivity(keyboard);
    layerWindow->setAnchors(anchors);
    layerWindow->setMargins(config.margins);
    layerWindow->setExclusiveZone(config.exclusiveZone);
    return true;
#else
    Q_UNUSED(config);
    setError(errorOut, QStringLiteral(
                        "LayerShellQt is disabled; Layer Shell surface cannot be configured"));
    return false;
#endif
}

bool AstreaLayerShellHelper::compiled()
{
#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
    return true;
#else
    return false;
#endif
}
