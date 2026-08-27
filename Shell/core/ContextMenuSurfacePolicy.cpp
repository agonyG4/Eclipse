#include "ContextMenuSurfacePolicy.hpp"

namespace Astrea::Shell::ContextMenuSurfacePolicy {

AstreaLayerShellConfig desktopInteraction()
{
    AstreaLayerShellConfig config;
    config.scope = QStringLiteral("astrea-desktop-interaction");
    config.layer = AstreaLayerShellConfig::Layer::Bottom;
    config.keyboardInteractivity = AstreaLayerShellConfig::KeyboardInteractivity::None;
    config.anchorTop = true;
    config.anchorBottom = true;
    config.anchorLeft = true;
    config.anchorRight = true;
    config.exclusiveZone = -1;
    return config;
}

AstreaLayerShellConfig overlay()
{
    AstreaLayerShellConfig config;
    config.scope = QStringLiteral("astrea-context-menu");
    config.layer = AstreaLayerShellConfig::Layer::Overlay;
    config.keyboardInteractivity = AstreaLayerShellConfig::KeyboardInteractivity::Exclusive;
    config.anchorTop = true;
    config.anchorBottom = true;
    config.anchorLeft = true;
    config.anchorRight = true;
    config.exclusiveZone = -1;
    return config;
}

} // namespace Astrea::Shell::ContextMenuSurfacePolicy
