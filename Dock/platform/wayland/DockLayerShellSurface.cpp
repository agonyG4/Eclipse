#include "platform/wayland/DockLayerShellSurface.hpp"

#include "platform/wayland/LayerShellHelper.hpp"

#include <QQuickWindow>

#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
#include <LayerShellQt/Window>
#endif

bool DockLayerShellSurface::configure(QQuickWindow *window, const DockConfig &config,
                                      int exclusiveZoneHeight, QScreen *screen,
                                      QString *errorOut)
{
    AstreaLayerShellConfig layerConfig;
    layerConfig.scope = QStringLiteral("astrea-dock");
    layerConfig.layer = AstreaLayerShellConfig::Layer::Top;
    layerConfig.keyboardInteractivity = AstreaLayerShellConfig::KeyboardInteractivity::None;
    layerConfig.anchorBottom = true;
    layerConfig.exclusiveZone = qMax(0, exclusiveZoneHeight);
    layerConfig.margins.setBottom(config.bottomMargin);
    layerConfig.screen = screen;
    return AstreaLayerShellHelper::configure(window, layerConfig, errorOut);
}

bool DockLayerShellSurface::updateExclusiveZone(QQuickWindow *window, int exclusiveZoneHeight,
                                                QString *errorOut)
{
    if (!window) {
        if (errorOut)
            *errorOut = QStringLiteral("Null window");
        return false;
    }
#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
    auto *layerWindow = LayerShellQt::Window::get(window);
    if (!layerWindow) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to create LayerShellQt::Window");
        return false;
    }
    layerWindow->setExclusiveZone(qMax(0, exclusiveZoneHeight));
    return true;
#else
    Q_UNUSED(exclusiveZoneHeight);
    if (errorOut)
        *errorOut = QStringLiteral(
            "LayerShellQt is disabled; Dock Layer Shell cannot update its exclusive zone");
    return false;
#endif
}

bool DockLayerShellSurface::setMapped(QQuickWindow *window, bool mapped, int exclusiveZoneHeight,
                                      QString *errorOut)
{
    if (!window) {
        if (errorOut)
            *errorOut = QStringLiteral("Null window");
        return false;
    }
#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
    auto *layerWindow = LayerShellQt::Window::get(window);
    if (!layerWindow) {
        if (errorOut)
            *errorOut = QStringLiteral("Failed to create LayerShellQt::Window");
        return false;
    }
    layerWindow->setExclusiveZone(exclusiveZoneForMapping(mapped, exclusiveZoneHeight));
    window->setVisible(mapped);
    return true;
#else
    Q_UNUSED(exclusiveZoneHeight);
    if (errorOut)
        *errorOut = QStringLiteral(
            "LayerShellQt is disabled; Dock Layer Shell cannot change its mapped state");
    return false;
#endif
}

bool DockLayerShellSurface::compiled()
{
    return AstreaLayerShellHelper::compiled();
}
