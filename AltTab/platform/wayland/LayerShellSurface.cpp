#include "platform/wayland/LayerShellSurface.hpp"
#include <QQuickWindow>
#include <QScreen>
#include <QGuiApplication>

#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
#include <LayerShellQt/Window>
#endif

bool AltTabLayerShellSurface::configure(QQuickWindow *window, QString *errorOut) {
    if (!window) {
        if (errorOut) *errorOut = QStringLiteral("Null window");
        return false;
    }

#if defined(ASTREA_HAVE_LAYER_SHELL_QT) && ASTREA_HAVE_LAYER_SHELL_QT
    auto *layerWindow = LayerShellQt::Window::get(window);
    if (!layerWindow) {
        if (errorOut) *errorOut = QStringLiteral("Failed to create LayerShellQt::Window");
        return false;
    }

    layerWindow->setScope(QStringLiteral("astrea-alt-tab"));
    layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
    layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
    layerWindow->setExclusiveZone(-1);
    layerWindow->setAnchors(LayerShellQt::Window::Anchors(
        LayerShellQt::Window::AnchorTop |
        LayerShellQt::Window::AnchorBottom |
        LayerShellQt::Window::AnchorLeft |
        LayerShellQt::Window::AnchorRight));

    if (auto *screen = window->screen())
        window->setGeometry(screen->geometry());

    return true;
#else
    Q_UNUSED(window);
    if (errorOut)
        *errorOut = QStringLiteral("LayerShellQt not available, using normal window");
    return false;
#endif
}
