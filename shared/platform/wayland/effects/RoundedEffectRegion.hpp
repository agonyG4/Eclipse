#pragma once

#include <QRect>
#include <QSize>
#include <QVector>

class AstreaRoundedEffectRegion final {
public:
    static QVector<QRect> rectangles(const QSize &size, qreal radius);
};
