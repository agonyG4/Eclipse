#include "platform/wayland/LayerShellSurface.hpp"
#include "platform/wayland/LayerShellHelper.hpp"

#include <QQuickWindow>
#include <QScreen>

bool AltTabLayerShellSurface::configure(QQuickWindow *window, QString *errorOut) {
    AstreaLayerShellConfig config;
    config.scope = QStringLiteral("astrea-alt-tab");
    config.layer = AstreaLayerShellConfig::Layer::Overlay;
    config.keyboardInteractivity = AstreaLayerShellConfig::KeyboardInteractivity::Exclusive;
    config.anchorTop = true;
    config.anchorBottom = true;
    config.anchorLeft = true;
    config.anchorRight = true;
    config.exclusiveZone = -1;
    const bool configured = AstreaLayerShellHelper::configure(window, config, errorOut);
    if (configured && window->screen())
        window->setGeometry(window->screen()->geometry());
    return configured;
}
