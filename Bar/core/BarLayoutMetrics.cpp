#include "core/BarLayoutMetrics.hpp"

#include "core/BarLayoutPolicy.hpp"

BarLayoutMetrics::BarLayoutMetrics(QObject *parent)
    : QObject(parent)
{
}

int BarLayoutMetrics::barHeight() const
{
    return BarLayoutPolicy::kBarHeight;
}

int BarLayoutMetrics::pillHeight() const
{
    return BarLayoutPolicy::kPillHeight;
}

int BarLayoutMetrics::topMargin() const
{
    return BarLayoutPolicy::kPillTopMargin;
}

int BarLayoutMetrics::launcherLeftMargin() const
{
    return BarLayoutPolicy::kLauncherLeftMargin;
}

int BarLayoutMetrics::statusRightMargin() const
{
    return BarLayoutPolicy::kStatusRightMargin;
}

int BarLayoutMetrics::sidePadding() const
{
    return BarLayoutPolicy::kSidePadding;
}

int BarLayoutMetrics::minimumGap() const
{
    return BarLayoutPolicy::kMinimumGap;
}

int BarLayoutMetrics::popupSidePadding() const
{
    return BarLayoutPolicy::kPopupSidePadding;
}

int BarLayoutMetrics::popupTop() const
{
    return BarLayoutPolicy::kPopupTopOffset;
}

int BarLayoutMetrics::statusWidth(int outputWidth, int launcherWidth, int pillWidth) const
{
    return BarLayoutPolicy::statusWidth(outputWidth, launcherWidth, pillWidth);
}

int BarLayoutMetrics::statusLeft(int outputWidth, int statusWidth) const
{
    return BarLayoutPolicy::statusLeft(outputWidth, statusWidth);
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
    return BarLayoutPolicy::popupX(outputWidth, popupWidth(outputWidth, cardWidth, padding),
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
