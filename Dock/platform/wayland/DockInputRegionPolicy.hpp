#pragma once

#include <QRegion>
#include <QRectF>
#include <QSize>
#include <QVector>

class DockInputRegionPolicy final {
public:
    static QRegion regionFor(const QSize &surfaceSize,
                             const QRectF &chromeRect,
                             const QVector<QRectF> &interactionRects,
                             int maximumInteractionRects = -1);
};
