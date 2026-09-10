#include "platform/wayland/effects/RoundedEffectRegion.hpp"

#include <QtMath>

QVector<QRect> AstreaRoundedEffectRegion::rectangles(const QSize &size, qreal radius)
{
    return rectangles(size, radius, 16);
}

QVector<QRect> AstreaRoundedEffectRegion::rectangles(const QSize &size,
                                                    qreal radius,
                                                    const int maxSegmentsPerCorner)
{
    if (size.width() <= 0 || size.height() <= 0)
        return {};

    const int maxRadius = qMin(size.width(), size.height()) / 2;
    const int clippedRadius = qBound(0, qRound(radius), maxRadius);
    if (clippedRadius == 0)
        return {QRect(QPoint(0, 0), size)};

    const int segments = qBound(1, clippedRadius, qMax(1, maxSegmentsPerCorner));
    QVector<QRect> result;
    result.reserve(2 * segments + 1);

    const int maxInset = qMax(0, (size.width() - 1) / 2);
    const auto appendBand = [&](const int band, const bool mirror) {
        const int top = (clippedRadius * band) / segments;
        const int bottom = (clippedRadius * (band + 1)) / segments;
        const int height = bottom - top;
        if (height <= 0)
            return;

        const qreal distanceFromCenter = qreal(clippedRadius - top);
        const qreal remaining = qMax(
            0.0,
            qreal(clippedRadius * clippedRadius) - distanceFromCenter * distanceFromCenter);
        const int inset = qBound(0, clippedRadius - qFloor(qSqrt(remaining)), maxInset);
        const int width = size.width() - 2 * inset;
        if (width <= 0)
            return;

        const int y = mirror ? size.height() - bottom : top;
        result.append(QRect(inset, y, width, height));
    };

    for (int band = 0; band < segments; ++band)
        appendBand(band, false);

    const int middleTop = clippedRadius;
    const int middleHeight = size.height() - 2 * middleTop;
    if (middleHeight > 0)
        result.append(QRect(0, middleTop, size.width(), middleHeight));

    for (int band = segments - 1; band >= 0; --band)
        appendBand(band, true);
    return result;
}
