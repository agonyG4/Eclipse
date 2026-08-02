#include "platform/wayland/DockLayerShellSurface.hpp"

#include "platform/wayland/LayerShellHelper.hpp"

#include <QQuickWindow>

#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
#include <LayerShellQt/Window>
#endif

bool DockLayerShellSurface::configure(QQuickWindow *window, const DockConfig &config,
                                      int surfaceHeight, QScreen *screen, QString *errorOut)
{
    AstreaLayerShellConfig layerConfig;
    layerConfig.scope = QStringLiteral("astrea-dock");
    layerConfig.layer = AstreaLayerShellConfig::Layer::Top;
    layerConfig.keyboardInteractivity = AstreaLayerShellConfig::KeyboardInteractivity::None;
    layerConfig.anchorBottom = true;
    layerConfig.exclusiveZone = qMax(0, surfaceHeight);
    layerConfig.margins.setBottom(config.bottomMargin);
    layerConfig.screen = screen;
    return AstreaLayerShellHelper::configure(window, layerConfig, errorOut);
}

bool DockLayerShellSurface::updateExclusiveZone(QQuickWindow *window, int surfaceHeight,
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
    layerWindow->setExclusiveZone(qMax(0, surfaceHeight));
    return true;
#else
    Q_UNUSED(surfaceHeight);
    if (errorOut)
        *errorOut = QStringLiteral("LayerShellQt not available, using normal window");
    return false;
#endif
}

bool DockLayerShellSurface::setMapped(QQuickWindow *window, bool mapped, QString *errorOut)
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
    if (!mapped)
        layerWindow->setExclusiveZone(0);
    window->setVisible(mapped);
    return true;
#else
    if (errorOut)
        *errorOut = QStringLiteral("LayerShellQt not available, using normal window");
    window->setVisible(mapped);
    return false;
#endif
}

bool DockLayerShellSurface::compiled()
{
    return AstreaLayerShellHelper::compiled();
}
