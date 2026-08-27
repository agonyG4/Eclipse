#include "core/DockSurfaceGeometry.hpp"

#include <QtGlobal>

QRect DockSurfaceGeometry::delegateRectInOutput(const QSize &outputSize,
                                                const QSize &surfaceSize,
                                                int bottomMargin,
                                                const QRectF &delegateRect)
{
    const qreal surfaceOriginX =
        (qMax(1, outputSize.width()) - qMax(1, surfaceSize.width())) / 2.0;
    const qreal surfaceOriginY = qMax(1, outputSize.height())
        - qMax(0, bottomMargin) - qMax(1, surfaceSize.height());
    const QRectF outputRect = delegateRect.translated(surfaceOriginX, surfaceOriginY);
    return QRect(qRound(outputRect.x()), qRound(outputRect.y()),
                 qRound(outputRect.width()), qRound(outputRect.height()));
}

QRect DockSurfaceGeometry::outputLocalDelegateRect(int outputWidth, int outputHeight,
                                                   int surfaceWidth, int surfaceHeight,
                                                   int bottomMargin,
                                                   const QRectF &delegateRect) const
{
    return delegateRectInOutput(QSize(outputWidth, outputHeight),
                                QSize(surfaceWidth, surfaceHeight), bottomMargin, delegateRect);
}
