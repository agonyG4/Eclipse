#pragma once

#include "core/BarLayoutPolicy.hpp"
#include "platform/wayland/LayerShellHelper.hpp"

enum class BarSurfaceKind {
    Reserve,
    Launcher,
    Status,
    PopupOverlay,
    TrayTooltip,
};

class BarSurfacePolicy final {
public:
    static AstreaLayerShellConfig reserve();
    static AstreaLayerShellConfig launcher();
    static AstreaLayerShellConfig status();
    static AstreaLayerShellConfig popupOverlay();
    static AstreaLayerShellConfig trayTooltip();

    static int surfaceHeight(BarSurfaceKind kind);
};
