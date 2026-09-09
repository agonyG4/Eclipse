#include "platform/wayland/effects/QtWaylandWindow.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>

#if ASTREA_HAVE_WAYLAND_EFFECTS
#include <wayland-client-core.h>
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
    auto *platformInterface = QGuiApplication::platformNativeInterface();
    if (!platformInterface)
        return nullptr;
    return static_cast<wl_surface *>(
        platformInterface->nativeResourceForWindow(QByteArrayLiteral("surface"), window));
#else
    Q_UNUSED(window);
    return nullptr;
#endif
}

bool astreaQtWaylandActive()
{
    return astreaQtWaylandDisplay() != nullptr && astreaQtWaylandCompositor() != nullptr;
}
