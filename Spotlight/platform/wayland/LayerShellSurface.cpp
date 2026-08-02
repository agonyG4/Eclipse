#include "platform/wayland/LayerShellSurface.hpp"
#include "platform/wayland/LayerShellHelper.hpp"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>

bool LayerShellSurface::configure(QQuickWindow *window, QString *errorOut) {
    AstreaLayerShellConfig config;
    config.scope = QStringLiteral("astrea-spotlight");
    config.layer = AstreaLayerShellConfig::Layer::Overlay;
    config.keyboardInteractivity = AstreaLayerShellConfig::KeyboardInteractivity::Exclusive;
    config.anchorTop = true;
    config.anchorBottom = true;
    config.anchorLeft = true;
    config.anchorRight = true;
    config.exclusiveZone = -1;
    const bool configured = AstreaLayerShellHelper::configure(window, config, errorOut);
    if (configured) {
        if (auto *screen = QGuiApplication::primaryScreen())
            window->setGeometry(screen->geometry());
        window->setProperty("_astrea_layer_shell", true);
    }
    return configured;
}
