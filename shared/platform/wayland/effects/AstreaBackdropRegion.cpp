#include "platform/wayland/effects/AstreaBackdropRegion.hpp"

#include <QQuickItem>

AstreaBackdropRegion::AstreaBackdropRegion(QObject *parent)
    : QObject(parent)
{
}

void AstreaBackdropRegion::setItem(QQuickItem *item)
{
    if (m_item == item)
        return;
    m_item = item;
    emit itemChanged();
}

void AstreaBackdropRegion::setEnabled(const bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
}

void AstreaBackdropRegion::setRadius(const qreal radius)
{
    const qreal bounded = qMax<qreal>(0.0, radius);
    if (qFuzzyCompare(m_radius, bounded))
        return;
    m_radius = bounded;
    emit radiusChanged();
}
