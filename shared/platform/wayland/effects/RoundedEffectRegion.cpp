#include "platform/wayland/effects/RoundedEffectRegion.hpp"

#include <QtMath>

QVector<QRect> AstreaRoundedEffectRegion::rectangles(const QSize &size, qreal radius)
{
    if (size.width() <= 0 || size.height() <= 0)
        return {};

    const int maxRadius = qMin(size.width(), size.height()) / 2;
    const int clippedRadius = qBound(0, qRound(radius), maxRadius);
    if (clippedRadius == 0)
        return {QRect(QPoint(0, 0), size)};

    constexpr int maxSegmentsPerCorner = 16;
    const int segments = qBound(1, clippedRadius, maxSegmentsPerCorner);
    const int bandHeight = qMax(1, qCeil(qreal(clippedRadius) / segments));
    QVector<QRect> result;
    result.reserve(2 * segments + 1);

    for (int band = 0; band < segments; ++band) {
        const int y = band * bandHeight;
        const int height = qMin(bandHeight, clippedRadius - y);
        const qreal distanceFromCorner = qreal(y + height);
        const qreal remaining = qMax(0.0,
                                     qreal(clippedRadius * clippedRadius)
                                         - qreal(clippedRadius) * qreal(clippedRadius)
                                               - distanceFromCorner * distanceFromCorner
                                               + 2.0 * qreal(clippedRadius) * distanceFromCorner);
        const int inset = qBound(0, clippedRadius - qFloor(qSqrt(remaining)), size.width() / 2);
        const int width = size.width() - 2 * inset;
        if (width > 0 && height > 0)
            result.append(QRect(inset, y, width, height));
    }

    const int middleTop = segments * bandHeight;
    const int middleHeight = size.height() - 2 * middleTop;
    if (middleHeight > 0)
        result.append(QRect(0, middleTop, size.width(), middleHeight));

    for (int band = segments - 1; band >= 0; --band) {
        const int y = band * bandHeight;
        const int height = qMin(bandHeight, clippedRadius - y);
        const int mirroredY = size.height() - y - height;
        const qreal distanceFromCorner = qreal(y + height);
        const qreal remaining = qMax(0.0,
                                     qreal(clippedRadius * clippedRadius)
                                         - qreal(clippedRadius) * qreal(clippedRadius)
                                               - distanceFromCorner * distanceFromCorner
                                               + 2.0 * qreal(clippedRadius) * distanceFromCorner);
        const int inset = qBound(0, clippedRadius - qFloor(qSqrt(remaining)), size.width() / 2);
        const int width = size.width() - 2 * inset;
        if (width > 0 && height > 0)
            result.append(QRect(inset, mirroredY, width, height));
    }
    return result;
}
