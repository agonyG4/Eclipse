#pragma once

class QWindow;
struct wl_compositor;
struct wl_display;
struct wl_surface;

wl_display *astreaQtWaylandDisplay();
wl_compositor *astreaQtWaylandCompositor();
wl_surface *astreaQtWaylandSurface(QWindow *window);
bool astreaQtWaylandActive();
