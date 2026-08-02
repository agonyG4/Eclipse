#include "platform/wayland/LayerShellHelper.hpp"

#include <QQuickWindow>

#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
#include <LayerShellQt/Window>
#endif

namespace {

void setError(QString *errorOut, const QString &message)
{
    if (errorOut)
        *errorOut = message;
}

} // namespace

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
    if (config.screen)
        layerWindow->setScreen(config.screen);
    layerWindow->setActivateOnShow(false);
    return true;
#else
    Q_UNUSED(config);
    setError(errorOut, QStringLiteral("LayerShellQt not available, using normal window"));
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
