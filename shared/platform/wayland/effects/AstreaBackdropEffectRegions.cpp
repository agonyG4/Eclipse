#include "platform/wayland/effects/AstreaBackdropEffectRegions.hpp"

#include "platform/wayland/effects/AstreaBackgroundEffectBinding.hpp"
#include "platform/wayland/effects/RoundedEffectRegion.hpp"
#include "platform/wayland/effects/AstreaWaylandEffects.hpp"

#include <QQuickWindow>
#include <QPlatformSurfaceEvent>
#include <QtMath>

#include <algorithm>
#include <utility>

namespace {

constexpr int maxClientRegionRectangles = 96;
constexpr int defaultCornerSegments = 16;

struct RegionGeometry {
    QRect bounds;
    qreal radius;
};

bool itemIsVisible(const QQuickItem *item)
{
    qreal opacity = 1.0;
    for (auto *current = item; current; current = current->parentItem()) {
        if (!current->isVisible())
            return false;
        opacity *= current->opacity();
    }
    return opacity > 0.001;
}

QRect integerBounds(const QRectF &bounds)
{
    const int left = qFloor(bounds.left());
    const int top = qFloor(bounds.top());
    const int right = qCeil(bounds.right());
    const int bottom = qCeil(bounds.bottom());
    return QRect(left, top, qMax(0, right - left), qMax(0, bottom - top));
}

} // namespace

AstreaBackdropEffectRegions::AstreaBackdropEffectRegions(QQuickItem *parent)
    : QQuickItem(parent)
    , m_binding(std::make_unique<AstreaBackgroundEffectBinding>(nullptr))
{
    setFlag(ItemHasContents, false);
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow *window) {
        m_binding = std::make_unique<AstreaBackgroundEffectBinding>(window);
        refreshConnections();
        scheduleSync();
    });
    connect(this, &QQuickItem::xChanged, this, &AstreaBackdropEffectRegions::scheduleSync);
    connect(this, &QQuickItem::yChanged, this, &AstreaBackdropEffectRegions::scheduleSync);
    connect(this, &QQuickItem::widthChanged, this, &AstreaBackdropEffectRegions::scheduleSync);
    connect(this, &QQuickItem::heightChanged, this, &AstreaBackdropEffectRegions::scheduleSync);
}

AstreaBackdropEffectRegions::~AstreaBackdropEffectRegions()
{
    if (m_observedWindow)
        m_observedWindow->removeEventFilter(this);
    const auto connections = std::exchange(m_connections, {});
    for (const auto &connection : connections)
        QObject::disconnect(connection);
}

void AstreaBackdropEffectRegions::setEnabled(const bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
    scheduleSync();
}

QQmlListProperty<AstreaBackdropRegion> AstreaBackdropEffectRegions::regions()
{
    return {this, nullptr, &AstreaBackdropEffectRegions::appendRegion,
            &AstreaBackdropEffectRegions::regionCount, &AstreaBackdropEffectRegions::regionAt,
            &AstreaBackdropEffectRegions::clearRegions};
}

void AstreaBackdropEffectRegions::appendRegion(QQmlListProperty<AstreaBackdropRegion> *property,
                                               AstreaBackdropRegion *region)
{
    if (!property || !region)
        return;
    auto *self = static_cast<AstreaBackdropEffectRegions *>(property->object);
    if (self->m_regions.contains(region))
        return;
    self->m_regions.append(region);
    self->connectRegion(region);
    self->scheduleSync();
}

qsizetype AstreaBackdropEffectRegions::regionCount(
    QQmlListProperty<AstreaBackdropRegion> *property)
{
    const auto *self = property ? static_cast<AstreaBackdropEffectRegions *>(property->object)
                                : nullptr;
    return self ? self->m_regions.size() : 0;
}

AstreaBackdropRegion *AstreaBackdropEffectRegions::regionAt(
    QQmlListProperty<AstreaBackdropRegion> *property, const qsizetype index)
{
    const auto *self = property ? static_cast<AstreaBackdropEffectRegions *>(property->object)
                                : nullptr;
    return self && index >= 0 && index < self->m_regions.size() ? self->m_regions.at(index)
                                                                : nullptr;
}

void AstreaBackdropEffectRegions::clearRegions(QQmlListProperty<AstreaBackdropRegion> *property)
{
    auto *self = property ? static_cast<AstreaBackdropEffectRegions *>(property->object) : nullptr;
    if (!self)
        return;
    self->m_regions.clear();
    self->refreshConnections();
    self->scheduleSync();
}

void AstreaBackdropEffectRegions::connectRegion(AstreaBackdropRegion *region)
{
    if (!region)
        return;
    m_connections.append(connect(region, &AstreaBackdropRegion::itemChanged, this,
                                 &AstreaBackdropEffectRegions::refreshConnections));
    m_connections.append(connect(region, &AstreaBackdropRegion::enabledChanged, this,
                                 &AstreaBackdropEffectRegions::scheduleSync));
    m_connections.append(connect(region, &AstreaBackdropRegion::radiusChanged, this,
                                 &AstreaBackdropEffectRegions::scheduleSync));
    refreshConnections();
}

void AstreaBackdropEffectRegions::refreshConnections()
{
    const auto connections = std::exchange(m_connections, {});
    for (const auto &connection : connections)
        QObject::disconnect(connection);
    auto *window = this->window();
    if (m_observedWindow && m_observedWindow != window)
        m_observedWindow->removeEventFilter(this);
    m_observedWindow = window;
    if (window) {
        window->installEventFilter(this);
        m_connections.append(connect(window, &QWindow::visibleChanged, this,
                                     &AstreaBackdropEffectRegions::scheduleSync));
        m_connections.append(connect(window, &QWindow::widthChanged, this,
                                     &AstreaBackdropEffectRegions::scheduleSync));
        m_connections.append(connect(window, &QWindow::heightChanged, this,
                                     &AstreaBackdropEffectRegions::scheduleSync));
        m_connections.append(connect(window, &QQuickWindow::afterAnimating, this,
                                     &AstreaBackdropEffectRegions::syncForCurrentAnimationFrame,
                                     Qt::DirectConnection));
        if (auto *effects = AstreaWaylandEffects::instance())
            m_connections.append(connect(effects, &AstreaWaylandEffects::availableChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
    }
    const auto *regions = m_regions.constData();
    const auto regionCount = m_regions.size();
    for (qsizetype index = 0; index < regionCount; ++index) {
        auto *region = regions[index];
        if (!region)
            continue;
        m_connections.append(connect(region, &AstreaBackdropRegion::itemChanged, this,
                                     &AstreaBackdropEffectRegions::refreshConnections));
        m_connections.append(connect(region, &AstreaBackdropRegion::enabledChanged, this,
                                     &AstreaBackdropEffectRegions::scheduleSync));
        m_connections.append(connect(region, &AstreaBackdropRegion::radiusChanged, this,
                                     &AstreaBackdropEffectRegions::scheduleSync));
        if (auto *item = region->item()) {
            m_connections.append(connect(item, &QObject::destroyed, this, [this] {
                scheduleSync();
            }));
            m_connections.append(connect(item, &QQuickItem::xChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(item, &QQuickItem::yChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(item, &QQuickItem::widthChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(item, &QQuickItem::heightChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(item, &QQuickItem::visibleChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(item, &QQuickItem::opacityChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(item, &QQuickItem::scaleChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(item, &QQuickItem::windowChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
        }
    }
    scheduleSync();
}

QVector<QRect> AstreaBackdropEffectRegions::resolvedRegion() const
{
    auto *window = this->window();
    if (!window || !window->contentItem() || !m_enabled || !window->isVisible())
        return {};

    const QRect surfaceBounds(QPoint(0, 0), window->size());
    QVector<RegionGeometry> geometries;
    for (const auto *region : m_regions) {
        auto *item = region ? region->item() : nullptr;
        if (!region || !region->enabled() || !item || !itemIsVisible(item))
            continue;
        const QRect bounds = integerBounds(item->mapRectToItem(window->contentItem(),
                                                               item->boundingRect()));
        const QRect clipped = bounds.intersected(surfaceBounds);
        if (clipped.isEmpty())
            continue;
        qreal radius = region->radius();
        if (item->width() > 0.0)
            radius *= qMax<qreal>(0.0, bounds.width() / item->width());
        geometries.append({bounds, radius});
    }

    QVector<int> segmentLimits(geometries.size(), defaultCornerSegments);
    const auto buildRegion = [&] {
        QVector<QRect> result;
        for (qsizetype index = 0; index < geometries.size(); ++index) {
            const auto &geometry = geometries.at(index);
            const auto decomposition = AstreaRoundedEffectRegion::rectangles(
                geometry.bounds.size(), geometry.radius, segmentLimits.at(index));
            for (const auto &rectangle : decomposition) {
                const QRect translated = rectangle.translated(geometry.bounds.topLeft())
                                             .intersected(surfaceBounds);
                if (!translated.isEmpty())
                    result.append(translated);
            }
        }
        std::sort(result.begin(), result.end(), [](const QRect &left, const QRect &right) {
            if (left.y() != right.y())
                return left.y() < right.y();
            if (left.x() != right.x())
                return left.x() < right.x();
            if (left.width() != right.width())
                return left.width() < right.width();
            return left.height() < right.height();
        });
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    };

    auto result = buildRegion();
    while (result.size() > maxClientRegionRectangles) {
        qsizetype selected = -1;
        for (qsizetype index = 0; index < segmentLimits.size(); ++index) {
            if (segmentLimits.at(index) > 1
                && (selected < 0 || segmentLimits.at(index) > segmentLimits.at(selected)))
                selected = index;
        }
        if (selected < 0)
            break;
        --segmentLimits[selected];
        result = buildRegion();
    }
    return result;
}

void AstreaBackdropEffectRegions::componentComplete()
{
    QQuickItem::componentComplete();
    m_componentComplete = true;
    refreshConnections();
    scheduleSync();
}

void AstreaBackdropEffectRegions::itemChange(const ItemChange change,
                                             const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    if (change == ItemSceneChange)
        scheduleSync();
}

bool AstreaBackdropEffectRegions::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_observedWindow && event
        && event->type() == QEvent::PlatformSurface) {
        auto *surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);
        if (surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
            if (m_binding)
                m_binding->surfaceAboutToBeDestroyed();
        } else if (surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
            if (m_binding)
                m_binding->surfaceCreated();
            scheduleSync();
        }
    }
    return QQuickItem::eventFilter(watched, event);
}

void AstreaBackdropEffectRegions::scheduleSync()
{
    if (!m_componentComplete || m_syncScheduled)
        return;
    m_syncScheduled = true;
    QMetaObject::invokeMethod(this, [this] {
        m_syncScheduled = false;
        sync();
    }, Qt::QueuedConnection);
}

void AstreaBackdropEffectRegions::sync()
{
    sync(true);
}

void AstreaBackdropEffectRegions::syncForCurrentAnimationFrame()
{
    if (!m_componentComplete)
        return;
    sync(false);
}

void AstreaBackdropEffectRegions::sync(const bool requestFrame)
{
    const auto region = resolvedRegion();
    if (region != m_lastResolvedRegion) {
        m_lastResolvedRegion = region;
        emit resolvedRegionChanged();
    }
    if (m_binding)
        m_binding->sync(region, m_enabled && !region.isEmpty(), requestFrame);
}
