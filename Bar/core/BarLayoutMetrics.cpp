#include "core/BarLayoutMetrics.hpp"

#include "core/BarSurfacePolicy.hpp"

BarLayoutMetrics::BarLayoutMetrics(QObject *parent)
    : QObject(parent)
{
}

int BarLayoutMetrics::barHeight() const
{
    return BarSurfacePolicy::kBarHeight;
}

int BarLayoutMetrics::pillHeight() const
{
    return BarSurfacePolicy::kPillHeight;
}

int BarLayoutMetrics::topMargin() const
{
    return BarSurfacePolicy::kPillTopMargin;
}

int BarLayoutMetrics::launcherLeftMargin() const
{
    return BarSurfacePolicy::kLauncherLeftMargin;
}

int BarLayoutMetrics::statusRightMargin() const
{
    return BarSurfacePolicy::kStatusRightMargin;
}

int BarLayoutMetrics::sidePadding() const
{
    return BarSurfacePolicy::kSidePadding;
}

int BarLayoutMetrics::minimumGap() const
{
    return BarSurfacePolicy::kMinimumGap;
}

int BarLayoutMetrics::popupSidePadding() const
{
    return BarSurfacePolicy::kPopupSidePadding;
}

int BarLayoutMetrics::popupTop() const
{
    return BarSurfacePolicy::kPopupTopOffset;
}

int BarLayoutMetrics::statusWidth(int outputWidth, int launcherWidth, int pillWidth) const
{
    return BarSurfacePolicy::statusWidth(outputWidth, launcherWidth, pillWidth);
}

int BarLayoutMetrics::statusLeft(int outputWidth, int statusWidth) const
{
    return BarSurfacePolicy::statusLeft(outputWidth, statusWidth);
}

int BarLayoutMetrics::statusAnchorX(int outputWidth, int statusWidth,
                                    int indicatorLocalX) const
{
    return statusLeft(outputWidth, statusWidth) + qMax(0, indicatorLocalX);
}

int BarLayoutMetrics::launcherAnchorX(int launcherWidth) const
{
    return launcherLeftMargin() + qMax(0, launcherWidth) / 2;
}

int BarLayoutMetrics::popupWidth(int outputWidth, int cardWidth, int sidePadding) const
{
    const int padding = sidePadding < 0 ? popupSidePadding() : qMax(0, sidePadding);
    const int available = qMax(0, outputWidth - 2 * padding);
    return qMin(qMax(0, cardWidth), available);
}

int BarLayoutMetrics::popupX(int outputWidth, int cardWidth, int anchorX, int sidePadding) const
{
    const int padding = sidePadding < 0 ? popupSidePadding() : qMax(0, sidePadding);
    return BarSurfacePolicy::popupX(outputWidth, popupWidth(outputWidth, cardWidth, padding),
                                    anchorX, padding);
}

QVariantMap BarLayoutMetrics::trayAnchor(int outputOriginX, int outputOriginY,
                                         int statusLeftValue, int statusTopValue,
                                         int indicatorLocalX, int indicatorLocalY) const
{
    const int localX = qMax(0, statusLeftValue) + qMax(0, indicatorLocalX);
    const int localY = qMax(0, statusTopValue) + qMax(0, indicatorLocalY);
    return {{QStringLiteral("localX"), localX},
            {QStringLiteral("localY"), localY},
            {QStringLiteral("globalX"), outputOriginX + localX},
            {QStringLiteral("globalY"), outputOriginY + localY}};
}
