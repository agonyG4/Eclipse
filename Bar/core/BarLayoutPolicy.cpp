#include "core/BarLayoutPolicy.hpp"

#include <QtGlobal>

int BarLayoutPolicy::statusWidth(int outputWidth, int launcherWidth, int pillWidth)
{
    const int available = outputWidth - kLauncherLeftMargin - launcherWidth
        - kMinimumGap - kStatusRightMargin;
    return qMin(qMax(0, available), qMax(0, pillWidth));
}

int BarLayoutPolicy::statusLeft(int outputWidth, int statusWidth)
{
    return qMax(0, outputWidth - qMax(0, statusWidth) - kStatusRightMargin);
}

int BarLayoutPolicy::popupX(int outputWidth, int cardWidth, int anchorX, int sidePadding)
{
    const int output = qMax(0, outputWidth);
    const int minimum = qMin(qMax(0, sidePadding), output);
    const int maximum = qMax(minimum, output - qMax(0, cardWidth) - minimum);
    return qBound(minimum, anchorX - cardWidth / 2, maximum);
}
