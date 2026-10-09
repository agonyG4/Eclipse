#include "platform/wayland/effects/AstreaBackdropAnalyticCoverage.hpp"

#include <QQuickItem>

AstreaBackdropAnalyticCoverage::AstreaBackdropAnalyticCoverage(QObject *parent)
    : QObject(parent)
{
}

void AstreaBackdropAnalyticCoverage::setItem(QQuickItem *item)
{
    if (m_item == item)
        return;
    m_item = item;
    emit itemChanged();
}

void AstreaBackdropAnalyticCoverage::setRoundedRectEnabled(const bool enabled)
{
    if (m_roundedRectEnabled == enabled)
        return;
    m_roundedRectEnabled = enabled;
    emit roundedRectEnabledChanged();
}

void AstreaBackdropAnalyticCoverage::setRoundedRect(const QRectF &rect)
{
    if (m_roundedRect == rect)
        return;
    m_roundedRect = rect;
    emit roundedRectChanged();
}

void AstreaBackdropAnalyticCoverage::setRadius(const qreal radius)
{
    if (m_radius == radius)
        return;
    m_radius = radius;
    emit radiusChanged();
}

void AstreaBackdropAnalyticCoverage::setTriangleEnabled(const bool enabled)
{
    if (m_triangleEnabled == enabled)
        return;
    m_triangleEnabled = enabled;
    emit triangleEnabledChanged();
}

void AstreaBackdropAnalyticCoverage::setTriangleA(const QPointF &point)
{
    if (m_triangleA == point)
        return;
    m_triangleA = point;
    emit triangleAChanged();
}

void AstreaBackdropAnalyticCoverage::setTriangleB(const QPointF &point)
{
    if (m_triangleB == point)
        return;
    m_triangleB = point;
    emit triangleBChanged();
}

void AstreaBackdropAnalyticCoverage::setTriangleC(const QPointF &point)
{
    if (m_triangleC == point)
        return;
    m_triangleC = point;
    emit triangleCChanged();
}
