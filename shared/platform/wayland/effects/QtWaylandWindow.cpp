#include "platform/wayland/effects/QtWaylandWindow.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QWindow>

#if ASTREA_HAVE_WAYLAND_EFFECTS
#include <QtGui/qguiapplication_platform.h>
#include <wayland-client-core.h>

namespace QNativeInterface::Private {

// This is the stable native-interface lookup contract used by Qt's Wayland
// plugin. Keeping the declaration local avoids taking a dependency on Qt's
// private headers while still asking Qt for the surface it owns.
struct QWaylandWindow {
    QT_DECLARE_NATIVE_INTERFACE(QWaylandWindow, 1, QWindow)
    virtual wl_surface *surface() const = 0;
};

} // namespace QNativeInterface::Private
#endif

namespace {

#if ASTREA_HAVE_WAYLAND_EFFECTS
const QNativeInterface::QWaylandApplication *qtWaylandApplication()
{
    auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    return application ? application->nativeInterface<QNativeInterface::QWaylandApplication>()
                       : nullptr;
}
#endif

} // namespace

wl_display *astreaQtWaylandDisplay()
{
#if ASTREA_HAVE_WAYLAND_EFFECTS
    if (const auto *application = qtWaylandApplication())
        return application->display();
#endif
    return nullptr;
}

wl_compositor *astreaQtWaylandCompositor()
{
#if ASTREA_HAVE_WAYLAND_EFFECTS
    if (const auto *application = qtWaylandApplication())
        return application->compositor();
#endif
    return nullptr;
}

wl_surface *astreaQtWaylandSurface(QWindow *window)
{
#if ASTREA_HAVE_WAYLAND_EFFECTS
    if (!window || !astreaQtWaylandDisplay())
        return nullptr;
    const auto *nativeWindow =
        window->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
    return nativeWindow ? nativeWindow->surface() : nullptr;
#else
    Q_UNUSED(window);
    return nullptr;
#endif
}

bool astreaQtWaylandActive()
{
    return astreaQtWaylandDisplay() != nullptr && astreaQtWaylandCompositor() != nullptr;
}
