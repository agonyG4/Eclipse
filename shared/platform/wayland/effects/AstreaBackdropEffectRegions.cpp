#include "platform/wayland/effects/AstreaBackdropEffectRegions.hpp"

#include "platform/wayland/effects/AstreaBackdropAnalyticCoverage.hpp"
#include "platform/wayland/effects/AstreaBackgroundEffectBinding.hpp"
#include "platform/wayland/effects/RoundedEffectRegion.hpp"
#include "platform/wayland/effects/AstreaWaylandEffects.hpp"

#include <QDebug>
#include <QQuickWindow>
#include <QPlatformSurfaceEvent>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <mutex>
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

bool finitePoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

bool nearlyEqual(const qreal left, const qreal right, const qreal tolerance)
{
    return qAbs(left - right) <= tolerance;
}

std::optional<AstreaAnalyticRoundedRect> mapRoundedRectExactly(
    QQuickItem *item, QQuickItem *surfaceItem, const QRectF &localRect, const qreal radius)
{
    if (!item || !surfaceItem || localRect.isEmpty() || !std::isfinite(radius) || radius < 0.0
        || radius > qMin(localRect.width(), localRect.height()) / 2.0)
        return std::nullopt;

    const QPointF p00 = item->mapToItem(surfaceItem, localRect.topLeft());
    const QPointF p10 = item->mapToItem(surfaceItem, localRect.topRight());
    const QPointF p01 = item->mapToItem(surfaceItem, localRect.bottomLeft());
    const QPointF p11 = item->mapToItem(surfaceItem, localRect.bottomRight());
    const QPointF pCenter = item->mapToItem(surfaceItem, localRect.center());
    if (!finitePoint(p00) || !finitePoint(p10) || !finitePoint(p01) || !finitePoint(p11)
        || !finitePoint(pCenter)) {
        return std::nullopt;
    }

    const QPointF xAxis = p10 - p00;
    const QPointF yAxis = p01 - p00;
    const qreal xLength = qAbs(xAxis.x());
    const qreal yLength = qAbs(yAxis.y());
    const qreal tolerance = 1e-6 * qMax<qreal>(1.0, qMax(xLength, yLength));
    if (xLength <= tolerance || yLength <= tolerance
        || qAbs(xAxis.y()) > tolerance || qAbs(yAxis.x()) > tolerance
        || !nearlyEqual(xLength / localRect.width(), yLength / localRect.height(), tolerance)
        || !nearlyEqual(p11.x(), p10.x() + yAxis.x(), tolerance)
        || !nearlyEqual(p11.y(), p10.y() + yAxis.y(), tolerance)
        || !nearlyEqual(pCenter.x(), (p00.x() + p11.x()) / 2.0, tolerance)
        || !nearlyEqual(pCenter.y(), (p00.y() + p11.y()) / 2.0, tolerance)) {
        return std::nullopt;
    }

    const qreal mappedLeft = qMin(p00.x(), p10.x());
    const qreal mappedTop = qMin(p00.y(), p01.y());
    const qreal scale = xLength / localRect.width();
    return AstreaAnalyticRoundedRect{
        QRectF(mappedLeft, mappedTop, xLength, yLength), radius * scale};
}

QRectF triangleBounds(const AstreaAnalyticTriangle &triangle)
{
    const qreal left = qMin(triangle.a.x(), qMin(triangle.b.x(), triangle.c.x()));
    const qreal top = qMin(triangle.a.y(), qMin(triangle.b.y(), triangle.c.y()));
    const qreal right = qMax(triangle.a.x(), qMax(triangle.b.x(), triangle.c.x()));
    const qreal bottom = qMax(triangle.a.y(), qMax(triangle.b.y(), triangle.c.y()));
    return QRectF(left, top, right - left, bottom - top);
}

bool validTriangle(const AstreaAnalyticTriangle &triangle)
{
    if (!finitePoint(triangle.a) || !finitePoint(triangle.b) || !finitePoint(triangle.c))
        return false;
    const qreal areaTwice = (triangle.b.x() - triangle.a.x())
            * (triangle.c.y() - triangle.a.y())
        - (triangle.b.y() - triangle.a.y()) * (triangle.c.x() - triangle.a.x());
    return std::isfinite(areaTwice) && qAbs(areaTwice) > 1e-8;
}

void warnAnalyticGeometryFallbackOnce()
{
    static std::once_flag warningOnce;
    std::call_once(warningOnce, [] {
        qWarning() << "Analytic backdrop geometry is not representable by v1;"
                      " using the public backdrop region";
    });
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

void AstreaBackdropEffectRegions::setAnalyticCoverage(
    AstreaBackdropAnalyticCoverage *coverage)
{
    if (m_analyticCoverage == coverage)
        return;
    m_analyticCoverage = coverage;
    emit analyticCoverageChanged();
    refreshConnections();
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
    m_connections.append(connect(region, &AstreaBackdropRegion::localRectanglesChanged, this,
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
        if (auto *effects = AstreaWaylandEffects::instance())
            m_connections.append(connect(
                effects, &AstreaWaylandEffects::analyticCoverageAvailableChanged, this,
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
        m_connections.append(connect(region, &AstreaBackdropRegion::localRectanglesChanged, this,
                                     &AstreaBackdropEffectRegions::scheduleSync));
    }

    const auto observeItem = [this](QQuickItem *item) {
        if (!item)
            return;
        m_connections.append(connect(item, &QObject::destroyed, this, [this] {
            scheduleSync();
        }));
        for (auto *transformedItem = item; transformedItem;
             transformedItem = transformedItem->parentItem()) {
            m_connections.append(connect(transformedItem, &QQuickItem::xChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(transformedItem, &QQuickItem::yChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(transformedItem, &QQuickItem::widthChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(transformedItem, &QQuickItem::heightChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(transformedItem, &QQuickItem::visibleChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(transformedItem, &QQuickItem::opacityChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(transformedItem, &QQuickItem::scaleChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(transformedItem, &QQuickItem::rotationChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(transformedItem, &QQuickItem::transformOriginChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
            m_connections.append(connect(transformedItem, &QQuickItem::parentChanged, this,
                                         &AstreaBackdropEffectRegions::refreshConnections));
            m_connections.append(connect(transformedItem, &QQuickItem::windowChanged, this,
                                         &AstreaBackdropEffectRegions::scheduleSync));
        }
    };
    for (const auto *region : m_regions) {
        if (region)
            observeItem(region->item());
    }
    if (m_analyticCoverage) {
        m_connections.append(connect(m_analyticCoverage, &QObject::destroyed, this, [this] {
            scheduleSync();
        }));
        m_connections.append(connect(m_analyticCoverage, &AstreaBackdropAnalyticCoverage::itemChanged,
                                     this, &AstreaBackdropEffectRegions::refreshConnections));
        m_connections.append(connect(m_analyticCoverage,
                                     &AstreaBackdropAnalyticCoverage::roundedRectEnabledChanged,
                                     this, &AstreaBackdropEffectRegions::scheduleSync));
        m_connections.append(connect(m_analyticCoverage,
                                     &AstreaBackdropAnalyticCoverage::roundedRectChanged,
                                     this, &AstreaBackdropEffectRegions::scheduleSync));
        m_connections.append(connect(m_analyticCoverage, &AstreaBackdropAnalyticCoverage::radiusChanged,
                                     this, &AstreaBackdropEffectRegions::scheduleSync));
        m_connections.append(connect(m_analyticCoverage,
                                     &AstreaBackdropAnalyticCoverage::triangleEnabledChanged,
                                     this, &AstreaBackdropEffectRegions::scheduleSync));
        m_connections.append(connect(m_analyticCoverage, &AstreaBackdropAnalyticCoverage::triangleAChanged,
                                     this, &AstreaBackdropEffectRegions::scheduleSync));
        m_connections.append(connect(m_analyticCoverage, &AstreaBackdropAnalyticCoverage::triangleBChanged,
                                     this, &AstreaBackdropEffectRegions::scheduleSync));
        m_connections.append(connect(m_analyticCoverage, &AstreaBackdropAnalyticCoverage::triangleCChanged,
                                     this, &AstreaBackdropEffectRegions::scheduleSync));
        observeItem(m_analyticCoverage->item());
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
    qsizetype explicitRectangleCount = 0;
    for (const auto *region : m_regions) {
        auto *item = region ? region->item() : nullptr;
        if (!region || !region->enabled() || !item || !itemIsVisible(item))
            continue;
        const QRect bounds = integerBounds(item->mapRectToItem(window->contentItem(),
                                                               item->boundingRect()));
        qreal radius = region->radius();
        if (item->width() > 0.0)
            radius *= qMax<qreal>(0.0, bounds.width() / item->width());
        const QVariantList localRectangles = region->localRectangles();
        if (localRectangles.isEmpty()) {
            if (bounds.intersected(surfaceBounds).isEmpty())
                continue;
            geometries.append({bounds, radius});
            continue;
        }

        for (const auto &value : localRectangles) {
            if (!value.canConvert<QRectF>())
                continue;
            const QRectF localRectangle = value.toRectF();
            if (localRectangle.isEmpty())
                continue;
            const QRect localBounds = integerBounds(item->mapRectToItem(
                window->contentItem(), localRectangle));
            if (localBounds.intersected(surfaceBounds).isEmpty())
                continue;
            ++explicitRectangleCount;
            if (explicitRectangleCount > maxClientRegionRectangles) {
                static std::once_flag warningOnce;
                std::call_once(warningOnce, [] {
                    qWarning() << "Backdrop effect explicit rectangles exceed the client region budget;"
                                  " disabling this update";
                });
                return {};
            }
            geometries.append({localBounds, radius});
        }
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
        if (selected < 0) {
            static std::once_flag warningOnce;
            std::call_once(warningOnce, [&] {
                qWarning() << "Backdrop effect region budget cannot represent" << result.size()
                           << "rectangles; disabling this update";
            });
            return {};
        }
        --segmentLimits[selected];
        result = buildRegion();
    }
    return result;
}

std::optional<AstreaAnalyticBackdropCoverage>
AstreaBackdropEffectRegions::resolvedAnalyticCoverage() const
{
    auto *window = this->window();
    auto *item = m_analyticCoverage ? m_analyticCoverage->item() : nullptr;
    if (!window || !window->contentItem() || !window->isVisible() || !m_enabled
        || !m_analyticCoverage || !item || !itemIsVisible(item)) {
        return std::nullopt;
    }
    if (item->window() != window)
        return std::nullopt;

    AstreaAnalyticBackdropCoverage result;
    if (m_analyticCoverage->roundedRectEnabled()) {
        const auto mapped = mapRoundedRectExactly(item, window->contentItem(),
                                                   m_analyticCoverage->roundedRect(),
                                                   m_analyticCoverage->radius());
        if (!mapped) {
            warnAnalyticGeometryFallbackOnce();
            return std::nullopt;
        }
        result.roundedRect = *mapped;
    }
    if (m_analyticCoverage->triangleEnabled()) {
        const auto mapPoint = [item, content = window->contentItem()](const QPointF &point) {
            return item->mapToItem(content, point);
        };
        AstreaAnalyticTriangle triangle{mapPoint(m_analyticCoverage->triangleA()),
                                        mapPoint(m_analyticCoverage->triangleB()),
                                        mapPoint(m_analyticCoverage->triangleC())};
        if (!validTriangle(triangle)) {
            warnAnalyticGeometryFallbackOnce();
            return std::nullopt;
        }
        result.triangle = triangle;
    }
    if (result.isEmpty())
        return std::nullopt;
    return result;
}

QVector<QRect> AstreaBackdropEffectRegions::analyticWorkRegion(
    const AstreaAnalyticBackdropCoverage &coverage, const QSize surfaceSize)
{
    if (coverage.isEmpty() || surfaceSize.width() <= 0 || surfaceSize.height() <= 0)
        return {};

    QRectF bounds;
    if (coverage.roundedRect)
        bounds = coverage.roundedRect->rect;
    if (coverage.triangle) {
        const auto triangle = triangleBounds(*coverage.triangle);
        bounds = bounds.isNull() ? triangle : bounds.united(triangle);
    }
    if (bounds.isEmpty() || !std::isfinite(bounds.left()) || !std::isfinite(bounds.top())
        || !std::isfinite(bounds.right()) || !std::isfinite(bounds.bottom())) {
        return {};
    }

    const qreal left = qMax<qreal>(0.0, std::floor(bounds.left()) - analyticCoverageGuard);
    const qreal top = qMax<qreal>(0.0, std::floor(bounds.top()) - analyticCoverageGuard);
    const qreal right = qMin<qreal>(surfaceSize.width(),
                                    std::ceil(bounds.right()) + analyticCoverageGuard);
    const qreal bottom = qMin<qreal>(surfaceSize.height(),
                                     std::ceil(bounds.bottom()) + analyticCoverageGuard);
    if (right <= left || bottom <= top)
        return {};
    return {QRect(QPoint(static_cast<int>(left), static_cast<int>(top)),
                  QSize(static_cast<int>(right - left), static_cast<int>(bottom - top)))};
}

QVector<QRect> AstreaBackdropEffectRegions::resolvedAnalyticWorkRegion() const
{
    const auto coverage = resolvedAnalyticCoverage();
    const auto *window = this->window();
    return coverage && window
        ? analyticWorkRegion(*coverage, window->size()) : QVector<QRect>{};
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
    const auto analyticCoverage = resolvedAnalyticCoverage();
    if (region != m_lastResolvedRegion) {
        m_lastResolvedRegion = region;
        emit resolvedRegionChanged();
    }
    const bool requested = m_enabled
        && (!region.isEmpty() || !resolvedAnalyticWorkRegion().isEmpty());
    if (m_binding)
        m_binding->sync(region, analyticCoverage, requested, requestFrame);
}
