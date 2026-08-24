#pragma once

#include "shared/platform/wayland/LayerShellHelper.hpp"

class QScreen;

namespace Paper {

class WallpaperSurfacePolicy final
{
public:
    static AstreaLayerShellConfig background(QScreen *screen = nullptr);
};

} // namespace Paper
