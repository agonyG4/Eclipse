#include "platform/wayland/BarSurfacePolicy.hpp"

#include <QtGlobal>

namespace {

AstreaLayerShellConfig baseConfig(const QString &scope)
{
    AstreaLayerShellConfig config;
    config.scope = scope;
    config.keyboardInteractivity = AstreaLayerShellConfig::KeyboardInteractivity::None;
    config.exclusiveZone = -1;
    return config;
}

} // namespace

AstreaLayerShellConfig BarSurfacePolicy::reserve()
{
    auto config = baseConfig(QStringLiteral("astrea-bar-reserve"));
    config.anchorTop = true;
    config.anchorLeft = true;
    config.anchorRight = true;
    config.exclusiveZone = BarLayoutPolicy::kBarHeight;
    return config;
}

AstreaLayerShellConfig BarSurfacePolicy::launcher()
{
    auto config = baseConfig(QStringLiteral("astrea-bar-launcher"));
    config.anchorTop = true;
    config.anchorLeft = true;
    config.margins = QMargins(BarLayoutPolicy::kLauncherLeftMargin,
                              BarLayoutPolicy::kPillTopMargin, 0, 0);
    return config;
}

AstreaLayerShellConfig BarSurfacePolicy::status()
{
    auto config = baseConfig(QStringLiteral("astrea-bar-status"));
    config.anchorTop = true;
    config.anchorRight = true;
    config.margins = QMargins(0, BarLayoutPolicy::kPillTopMargin,
                              BarLayoutPolicy::kStatusRightMargin, 0);
    return config;
}

AstreaLayerShellConfig BarSurfacePolicy::popupOverlay()
{
    auto config = baseConfig(QStringLiteral("astrea-bar-popup"));
    config.layer = AstreaLayerShellConfig::Layer::Overlay;
    config.anchorTop = true;
    config.anchorBottom = true;
    config.anchorLeft = true;
    config.anchorRight = true;
    return config;
}

AstreaLayerShellConfig BarSurfacePolicy::trayTooltip()
{
    auto config = baseConfig(QStringLiteral("astrea-bar-tray-tooltip"));
    config.anchorTop = true;
    config.anchorLeft = true;
    config.anchorRight = true;
    config.margins = QMargins(0, BarLayoutPolicy::kTrayTooltipTopMargin, 0, 0);
    return config;
}

int BarSurfacePolicy::surfaceHeight(BarSurfaceKind kind)
{
    if (kind == BarSurfaceKind::Reserve)
        return BarLayoutPolicy::kBarHeight;
    if (kind == BarSurfaceKind::TrayTooltip)
        return BarLayoutPolicy::kTrayTooltipHeight;
    return BarLayoutPolicy::kPillHeight;
}
