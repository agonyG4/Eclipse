#include "WallpaperSurfacePolicy.hpp"

namespace Paper {

AstreaLayerShellConfig WallpaperSurfacePolicy::background(QScreen *screen)
{
    AstreaLayerShellConfig config;
    config.scope = QStringLiteral("astrea-paper-wallpaper");
    config.layer = AstreaLayerShellConfig::Layer::Background;
    config.keyboardInteractivity = AstreaLayerShellConfig::KeyboardInteractivity::None;
    config.anchorTop = true;
    config.anchorBottom = true;
    config.anchorLeft = true;
    config.anchorRight = true;
    config.exclusiveZone = -1;
    config.margins = QMargins();
    config.screen = screen;
    return config;
}

} // namespace Paper
