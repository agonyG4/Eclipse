#include "platform/wayland/DockLayerShellSurface.hpp"

#include "platform/wayland/LayerShellHelper.hpp"

#include <QRect>
#include <QQuickWindow>
#include <QScreen>
#include <QVariant>

#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
#include <LayerShellQt/Window>
#endif

bool DockLayerShellSurface::configure(QQuickWindow *window, const DockConfig &config,
                                      int exclusiveZoneHeight, QScreen *screen,
                                      QString *errorOut)
{
    return configure(window, DockSurfaceGeometry::placementFor(config, false),
                     exclusiveZoneHeight, screen, errorOut);
}

bool DockLayerShellSurface::configure(QQuickWindow *window,
                                      const DockSurfacePlacement &placement,
                                      int exclusiveZoneHeight, QScreen *screen,
                                      QString *errorOut)
{
    if (!setOutputGeometry(window, screen, errorOut))
        return false;

    const AstreaLayerShellConfig layerConfig =
        configurationFor(placement, exclusiveZoneHeight, screen);
    return AstreaLayerShellHelper::configure(window, layerConfig, errorOut);
}

AstreaLayerShellConfig DockLayerShellSurface::configurationFor(const DockConfig &config,
                                                               int exclusiveZoneHeight,
                                                               QScreen *screen)
{
    return configurationFor(DockSurfaceGeometry::placementFor(config, false),
                            exclusiveZoneHeight, screen);
}

AstreaLayerShellConfig DockLayerShellSurface::configurationFor(
    const DockSurfacePlacement &placement, int exclusiveZoneHeight, QScreen *screen)
{
    AstreaLayerShellConfig layerConfig;
    layerConfig.scope = QStringLiteral("astrea-dock");
    layerConfig.layer = AstreaLayerShellConfig::Layer::Top;
    layerConfig.keyboardInteractivity = AstreaLayerShellConfig::KeyboardInteractivity::None;
    layerConfig.exclusiveZone = qMax(0, exclusiveZoneHeight);
    const int edgeMargin = placement.layerShellEdgeMargin;
    if (placement.position == QStringLiteral("left")) {
        layerConfig.anchorLeft = true;
        layerConfig.margins.setLeft(edgeMargin);
    } else if (placement.position == QStringLiteral("right")) {
        layerConfig.anchorRight = true;
        layerConfig.margins.setRight(edgeMargin);
    } else {
        layerConfig.anchorBottom = true;
        layerConfig.margins.setBottom(edgeMargin);
    }
    layerConfig.screen = screen;
    return layerConfig;
}

bool DockLayerShellSurface::setOutputGeometry(QQuickWindow *window, QScreen *screen,
                                              QString *errorOut)
{
    if (!window) {
        if (errorOut)
            *errorOut = QStringLiteral("Null window");
        return false;
    }
    if (!screen) {
        if (errorOut)
            *errorOut = QStringLiteral("Dock output screen is unavailable");
        return false;
    }

    const QRect geometry = screen->geometry();
    const int width = qMax(1, geometry.width());
    const int height = qMax(1, geometry.height());
    const auto setProperty = [window, errorOut](const char *name, const QVariant &value) {
        if (window->metaObject()->indexOfProperty(name) < 0) {
            if (errorOut)
                *errorOut = QStringLiteral("Dock surface is missing property '%1'")
                    .arg(QString::fromLatin1(name));
            return false;
        }
        if (!window->setProperty(name, value)) {
            if (errorOut)
                *errorOut = QStringLiteral("Dock surface property '%1' could not be set")
                    .arg(QString::fromLatin1(name));
            return false;
        }
        return true;
    };

    window->setScreen(screen);
    return setProperty("outputKey", screen->name())
        && setProperty("outputWidth", width)
        && setProperty("outputHeight", height)
        && setProperty("outputOriginX", geometry.x())
        && setProperty("outputOriginY", geometry.y());
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
