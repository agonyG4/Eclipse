#include "platform/wayland/DockInputRegionPolicy.hpp"

#include <QRect>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace {

std::optional<QRect> clippedIntegerRect(const QRectF &rect, const QSize &surfaceSize)
{
    if (surfaceSize.width() <= 0 || surfaceSize.height() <= 0
        || !std::isfinite(rect.x()) || !std::isfinite(rect.y())
        || !std::isfinite(rect.width()) || !std::isfinite(rect.height())) {
        return std::nullopt;
    }

    if (rect.width() == 0 || rect.height() == 0)
        return std::nullopt;

    // Use a wider intermediate for endpoint arithmetic. QML can provide
    // finite coordinates whose sum would overflow a qreal even though the
    // eventual rectangle is safely clipped to the window.
    const long double x1 = static_cast<long double>(rect.x());
    const long double rawX2 = x1 + static_cast<long double>(rect.width());
    const long double y1 = static_cast<long double>(rect.y());
    const long double rawY2 = y1 + static_cast<long double>(rect.height());
    if (std::isnan(rawX2) || std::isnan(rawY2))
        return std::nullopt;
    const auto saturatedEndpoint = [](long double value) {
        if (std::isfinite(value))
            return value;
        return value > 0 ? std::numeric_limits<long double>::max()
                         : std::numeric_limits<long double>::lowest();
    };
    const long double x2 = saturatedEndpoint(rawX2);
    const long double y2 = saturatedEndpoint(rawY2);

    const long double leftEdge = std::min(x1, x2);
    const long double rightEdge = std::max(x1, x2);
    const long double topEdge = std::min(y1, y2);
    const long double bottomEdge = std::max(y1, y2);
    const long double clippedLeft = std::max(0.0L, leftEdge);
    const long double clippedTop = std::max(0.0L, topEdge);
    const long double clippedRight = std::min(
        static_cast<long double>(surfaceSize.width()), rightEdge);
    const long double clippedBottom = std::min(
        static_cast<long double>(surfaceSize.height()), bottomEdge);
    if (clippedRight <= clippedLeft || clippedBottom <= clippedTop)
        return std::nullopt;

    const auto boundedFloor = [](long double value, int maximum) {
        return static_cast<int>(std::clamp(std::floor(value), 0.0L,
                                           static_cast<long double>(maximum)));
    };
    const auto boundedCeil = [](long double value, int minimum, int maximum) {
        return static_cast<int>(std::clamp(std::ceil(value),
                                           static_cast<long double>(minimum),
                                           static_cast<long double>(maximum)));
    };
    const int left = boundedFloor(clippedLeft, surfaceSize.width());
    const int top = boundedFloor(clippedTop, surfaceSize.height());
    const int rightExclusive = boundedCeil(clippedRight, left, surfaceSize.width());
    const int bottomExclusive = boundedCeil(clippedBottom, top, surfaceSize.height());
    if (rightExclusive <= left || bottomExclusive <= top)
        return std::nullopt;

    return QRect(left, top, rightExclusive - left, bottomExclusive - top);
}

} // namespace

QRegion DockInputRegionPolicy::regionFor(const QSize &surfaceSize,
                                         const QRectF &chromeRect,
                                         const QVector<QRectF> &interactionRects,
                                         int maximumInteractionRects)
{
    QRegion region;
    if (surfaceSize.width() <= 0 || surfaceSize.height() <= 0)
        return region;

    if (const auto chrome = clippedIntegerRect(chromeRect, surfaceSize))
        region += *chrome;

    const int available = interactionRects.size();
    const int limit = maximumInteractionRects < 0
        ? available
        : qBound(0, maximumInteractionRects, available);
    for (int i = 0; i < limit; ++i) {
        if (const auto interaction = clippedIntegerRect(interactionRects.at(i), surfaceSize))
            region += *interaction;
    }
    return region;
}
