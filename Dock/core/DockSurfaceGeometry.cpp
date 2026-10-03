#include "core/DockSurfaceGeometry.hpp"
#include "core/DockMetrics.hpp"

#include <QtGlobal>

namespace {

qreal clampValue(qreal value, qreal minimum, qreal maximum)
{
    return qBound(minimum, value, qMax(minimum, maximum));
}

int clampedPointerCenter(qreal targetCenter, qreal cardOrigin, int edgeLength)
{
    const int pillCornerRadius = DockSurfaceGeometry::TooltipBodyHeight / 2;
    const int pointerHalfWidth = DockSurfaceGeometry::TooltipPointerWidth / 2;
    const int safeInset = qMin(edgeLength / 2, pillCornerRadius + pointerHalfWidth);
    return qRound(clampValue(targetCenter - cardOrigin, safeInset, edgeLength - safeInset));
}

} // namespace

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

DockTooltipPlacement DockSurfaceGeometry::calculateTooltipPlacement(
    const QSize &outputSize, const QRectF &anchorRect, const QSize &cardSize,
    const QString &position)
{
    constexpr qreal edgeInset = 8.0;
    constexpr qreal pointerDepth = DockSurfaceGeometry::TooltipPointerDepth;
    constexpr qreal targetGap = DockSurfaceGeometry::TooltipTargetGap;
    constexpr qreal spacing = targetGap + pointerDepth;

    const int outputWidth = qMax(1, outputSize.width());
    const int outputHeight = qMax(1, outputSize.height());
    const qreal insetX = qMin(edgeInset, qMax(0.0, (outputWidth - 1) / 2.0));
    const qreal insetY = qMin(edgeInset, qMax(0.0, (outputHeight - 1) / 2.0));
    const int width = qMin(qMax(1, cardSize.width()),
                           qMax(1, outputWidth - qRound(2 * insetX)));
    const int height = qMin(qMax(1, cardSize.height()),
                            qMax(1, outputHeight - qRound(2 * insetY)));
    const qreal minimumX = insetX;
    const qreal maximumX = outputWidth - insetX - width;
    const qreal minimumY = insetY;
    const qreal maximumY = outputHeight - insetY - height;
    const qreal targetCenterX = anchorRect.x() + anchorRect.width() / 2.0;
    const qreal targetCenterY = anchorRect.y() + anchorRect.height() / 2.0;
    const qreal anchorRight = anchorRect.x() + anchorRect.width();
    const qreal anchorBottom = anchorRect.y() + anchorRect.height();

    qreal x = minimumX;
    qreal y = minimumY;
    QString edge;

    if (position == QStringLiteral("left")) {
        const qreal inwardX = anchorRight + spacing;
        const qreal outwardX = anchorRect.x() - spacing - width;
        const bool fitsInward = inwardX + width <= outputWidth - insetX;
        const bool fitsOutward = outwardX >= minimumX;
        if (fitsInward || (!fitsOutward
                           && outputWidth - insetX - anchorRight
                               >= anchorRect.x() - insetX)) {
            x = clampValue(inwardX, minimumX, maximumX);
            edge = QStringLiteral("left");
        } else {
            x = clampValue(outwardX, minimumX, maximumX);
            edge = QStringLiteral("right");
        }
        y = clampValue(targetCenterY - height / 2.0, minimumY, maximumY);
    } else if (position == QStringLiteral("right")) {
        const qreal inwardX = anchorRect.x() - spacing - width;
        const qreal outwardX = anchorRight + spacing;
        const bool fitsInward = inwardX >= minimumX;
        const bool fitsOutward = outwardX + width <= outputWidth - insetX;
        if (fitsInward || (!fitsOutward
                           && anchorRect.x() - insetX
                               >= outputWidth - insetX - anchorRight)) {
            x = clampValue(inwardX, minimumX, maximumX);
            edge = QStringLiteral("right");
        } else {
            x = clampValue(outwardX, minimumX, maximumX);
            edge = QStringLiteral("left");
        }
        y = clampValue(targetCenterY - height / 2.0, minimumY, maximumY);
    } else {
        const qreal aboveY = anchorRect.y() - spacing - height;
        const qreal belowY = anchorBottom + spacing;
        const bool fitsAbove = aboveY >= minimumY;
        const bool fitsBelow = belowY <= maximumY;
        if (fitsAbove || (!fitsBelow
                          && anchorRect.y() - minimumY
                              >= outputHeight - insetY - anchorBottom)) {
            y = clampValue(aboveY, minimumY, maximumY);
            edge = QStringLiteral("bottom");
        } else {
            y = clampValue(belowY, minimumY, maximumY);
            edge = QStringLiteral("top");
        }
        x = clampValue(targetCenterX - width / 2.0, minimumX, maximumX);
    }

    DockTooltipPlacement result;
    result.cardRect = QRect(qRound(x), qRound(y), width, height);
    result.pointerEdge = edge;
    result.pointerCenter = (edge == QStringLiteral("left")
                            || edge == QStringLiteral("right"))
        ? clampedPointerCenter(targetCenterY, y, height)
        : clampedPointerCenter(targetCenterX, x, width);
    return result;
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

QVariantMap DockSurfaceGeometry::tooltipPlacementInOutput(
    int outputWidth, int outputHeight, const QRectF &anchorRect, int cardWidth,
    int cardHeight, const QString &position) const
{
    const DockTooltipPlacement placement = calculateTooltipPlacement(
        QSize(outputWidth, outputHeight), anchorRect, QSize(cardWidth, cardHeight), position);
    return {{QStringLiteral("cardRect"), placement.cardRect},
            {QStringLiteral("pointerEdge"), placement.pointerEdge},
            {QStringLiteral("pointerCenter"), placement.pointerCenter}};
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
