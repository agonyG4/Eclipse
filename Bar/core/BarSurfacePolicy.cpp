#include "core/BarSurfacePolicy.hpp"

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
    config.exclusiveZone = kBarHeight;
    return config;
}

AstreaLayerShellConfig BarSurfacePolicy::launcher()
{
    auto config = baseConfig(QStringLiteral("astrea-bar-launcher"));
    config.anchorTop = true;
    config.anchorLeft = true;
    config.margins = QMargins(kLauncherLeftMargin, kPillTopMargin, 0, 0);
    return config;
}

AstreaLayerShellConfig BarSurfacePolicy::status()
{
    auto config = baseConfig(QStringLiteral("astrea-bar-status"));
    config.anchorTop = true;
    config.anchorRight = true;
    config.margins = QMargins(0, kPillTopMargin, kStatusRightMargin, 0);
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
    config.margins = QMargins(0, kTrayTooltipTopMargin, 0, 0);
    return config;
}

int BarSurfacePolicy::surfaceHeight(BarSurfaceKind kind)
{
    if (kind == BarSurfaceKind::Reserve)
        return kBarHeight;
    if (kind == BarSurfaceKind::TrayTooltip)
        return kTrayTooltipHeight;
    return kPillHeight;
}

int BarSurfacePolicy::statusWidth(int outputWidth, int launcherWidth, int pillWidth)
{
    const int available = outputWidth - kLauncherLeftMargin - launcherWidth
        - kMinimumGap - kStatusRightMargin;
    return qMin(qMax(0, available), qMax(0, pillWidth));
}

int BarSurfacePolicy::statusLeft(int outputWidth, int statusWidth)
{
    return qMax(0, outputWidth - qMax(0, statusWidth) - kStatusRightMargin);
}

int BarSurfacePolicy::popupX(int outputWidth, int cardWidth, int anchorX, int sidePadding)
{
    const int output = qMax(0, outputWidth);
    const int minimum = qMin(qMax(0, sidePadding), output);
    const int maximum = qMax(minimum, output - qMax(0, cardWidth) - minimum);
    return qBound(minimum, anchorX - cardWidth / 2, maximum);
}
