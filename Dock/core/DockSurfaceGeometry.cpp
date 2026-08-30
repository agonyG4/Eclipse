#include "core/DockSurfaceGeometry.hpp"

#include <QtGlobal>

DockSurfacePlacement DockSurfaceGeometry::placementFor(const DockConfig &config,
                                                       bool autoHideActive)
{
    const QString position = config.position == QStringLiteral("left")
        ? QStringLiteral("left")
        : config.position == QStringLiteral("right") ? QStringLiteral("right")
                                                       : QStringLiteral("bottom");
    const int configuredMargin = config.effectiveEdgeMargin();
    if (autoHideActive)
        return {position, 0, configuredMargin, true};
    return {position, configuredMargin, 0, false};
}

QRect DockSurfaceGeometry::delegateRectInOutput(const QSize &outputSize,
                                                const QSize &surfaceSize,
                                                int bottomMargin,
                                                const QRectF &delegateRect)
{
    return delegateRectInOutput(outputSize, surfaceSize, QStringLiteral("bottom"),
                                bottomMargin, delegateRect);
}

QRect DockSurfaceGeometry::delegateRectInOutput(const QSize &outputSize,
                                                const QSize &surfaceSize,
                                                const QString &position,
                                                int edgeMargin,
                                                const QRectF &delegateRect)
{
    const qreal outputWidth = qMax(1, outputSize.width());
    const qreal outputHeight = qMax(1, outputSize.height());
    const qreal surfaceWidth = qMax(1, surfaceSize.width());
    const qreal surfaceHeight = qMax(1, surfaceSize.height());
    qreal surfaceOriginX = (outputWidth - surfaceWidth) / 2.0;
    qreal surfaceOriginY = (outputHeight - surfaceHeight) / 2.0;
    const qreal margin = qMax(0, edgeMargin);
    if (position == QStringLiteral("left"))
        surfaceOriginX = margin;
    else if (position == QStringLiteral("right"))
        surfaceOriginX = outputWidth - margin - surfaceWidth;
    else
        surfaceOriginY = outputHeight - margin - surfaceHeight;
    const QRectF outputRect = delegateRect.translated(surfaceOriginX, surfaceOriginY);
    return QRect(qRound(outputRect.x()), qRound(outputRect.y()),
                 qRound(outputRect.width()), qRound(outputRect.height()));
}

QRect DockSurfaceGeometry::outputLocalDelegateRect(int outputWidth, int outputHeight,
                                                   int surfaceWidth, int surfaceHeight,
                                                   const QString &position, int edgeMargin,
                                                   const QRectF &delegateRect) const
{
    return delegateRectInOutput(QSize(outputWidth, outputHeight),
                                QSize(surfaceWidth, surfaceHeight), position, edgeMargin,
                                delegateRect);
}
