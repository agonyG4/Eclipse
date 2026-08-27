#pragma once

namespace DockMetrics {

constexpr int chromeBottomMargin = 3;

// The resting height is the only height reserved from the exclusive zone. Visual
// magnification may grow above this baseline without changing window clearance.
constexpr int restingHeight(int iconSize)
{
    return iconSize + 20;
}

constexpr int delegateWidth(int iconSize)
{
    return iconSize + 8;
}

constexpr int delegateHeight(int iconSize)
{
    return iconSize + 14;
}

constexpr int iconRestingTop(int iconSize)
{
    return restingHeight(iconSize) - chromeBottomMargin - delegateHeight(iconSize)
        + (delegateHeight(iconSize) - iconSize) / 2;
}

} // namespace DockMetrics
