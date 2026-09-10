#include "core/DockSurfaceGeometry.hpp"
#include "core/DockMetrics.hpp"

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

QRect DockSurfaceGeometry::restingIconRectInGlobal(const QSize &outputSize,
                                                   const QSize &surfaceSize,
                                                   const QPoint &outputOrigin,
                                                   const QString &position,
                                                   int edgeMargin,
                                                   int chromeEdgeInset,
                                                   int iconSize,
                                                   int delegateWidth,
                                                   int delegateHeight,
                                                   int itemSpacing,
                                                   int panelPadding,
                                                   int index,
                                                   int count)
{
    const int boundedIcon = qMax(1, iconSize);
    const int boundedDelegateWidth = qMax(boundedIcon, delegateWidth);
    const int boundedDelegateHeight = qMax(boundedIcon, delegateHeight);
    const int boundedSpacing = qMax(0, itemSpacing);
    const int boundedPadding = qMax(0, panelPadding);
    const int boundedCount = qMax(1, count);
    const int boundedIndex = qBound(0, index, boundedCount - 1);
    const bool vertical = position == QStringLiteral("left") || position == QStringLiteral("right");
    const int restingCross = boundedIcon + 20;
    const int itemExtent = vertical ? boundedDelegateHeight : boundedDelegateWidth;
    const int restingPrimary = boundedCount * itemExtent
        + qMax(0, boundedCount - 1) * boundedSpacing + 2 * boundedPadding;
    const int chromeX = vertical
        ? (position == QStringLiteral("left") ? qMax(0, chromeEdgeInset)
                                               : surfaceSize.width() - restingCross - qMax(0, chromeEdgeInset))
        : (surfaceSize.width() - restingPrimary) / 2;
    const int chromeY = vertical
        ? (surfaceSize.height() - restingPrimary) / 2
        : surfaceSize.height() - restingCross - qMax(0, chromeEdgeInset);
    const int localX = vertical
        ? chromeX + (restingCross - boundedDelegateWidth) / 2
            + (boundedDelegateWidth - boundedIcon) / 2
        : chromeX + boundedPadding + boundedIndex * (boundedDelegateWidth + boundedSpacing)
            + (boundedDelegateWidth - boundedIcon) / 2;
    const int localY = vertical
        ? chromeY + boundedPadding + boundedIndex * (boundedDelegateHeight + boundedSpacing)
            + (boundedDelegateHeight - boundedIcon) / 2
        : chromeY + restingCross - boundedDelegateHeight - DockMetrics::chromeBottomMargin
            + (boundedDelegateHeight - boundedIcon) / 2;
    const QRect outputLocal = delegateRectInOutput(outputSize, surfaceSize, position, edgeMargin,
                                                   QRectF(localX, localY, boundedIcon, boundedIcon));
    return outputLocal.translated(outputOrigin);
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

QRect DockSurfaceGeometry::restingIconRectInGlobal(int outputWidth, int outputHeight,
                                                   int surfaceWidth, int surfaceHeight,
                                                   int outputOriginX, int outputOriginY,
                                                   const QString &position, int edgeMargin,
                                                   int chromeEdgeInset, int iconSize,
                                                   int delegateWidth, int delegateHeight,
                                                   int itemSpacing, int panelPadding, int index,
                                                   int count) const
{
    return restingIconRectInGlobal(QSize(outputWidth, outputHeight),
                                   QSize(surfaceWidth, surfaceHeight),
                                   QPoint(outputOriginX, outputOriginY), position, edgeMargin,
                                   chromeEdgeInset, iconSize, delegateWidth, delegateHeight,
                                   itemSpacing, panelPadding, index, count);
}
