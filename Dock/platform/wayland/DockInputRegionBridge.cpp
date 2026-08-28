#include "platform/wayland/DockInputRegionBridge.hpp"

#include "platform/wayland/DockInputRegionPolicy.hpp"

#include <QRect>
#include <QQuickWindow>
#include <QVariantMap>

#include <cmath>
#include <optional>

namespace {

std::optional<QRectF> rectangleFromVariant(const QVariant &value)
{
    if (value.canConvert<QRectF>()) {
        const QRectF rect = value.toRectF();
        if (std::isfinite(rect.x()) && std::isfinite(rect.y())
            && std::isfinite(rect.width()) && std::isfinite(rect.height())) {
            return rect;
        }
        return std::nullopt;
    }

    if (value.canConvert<QRect>())
        return value.toRect();

    if (value.canConvert<QVariantMap>()) {
        const QVariantMap map = value.toMap();
        const QRectF rect(map.value(QStringLiteral("x")).toDouble(),
                          map.value(QStringLiteral("y")).toDouble(),
                          map.value(QStringLiteral("width")).toDouble(),
                          map.value(QStringLiteral("height")).toDouble());
        if (std::isfinite(rect.x()) && std::isfinite(rect.y())
            && std::isfinite(rect.width()) && std::isfinite(rect.height())) {
            return rect;
        }
    }
    return std::nullopt;
}

} // namespace

DockInputRegionBridge::DockInputRegionBridge(QObject *parent)
    : QObject(parent)
{
}

void DockInputRegionBridge::setWindow(QQuickWindow *window)
{
    if (m_window == window)
        return;
    m_window = window;
    m_lastRegion = QRegion();
    m_hasAppliedRegion = false;
    if (m_window)
        emit windowReady();
}

void DockInputRegionBridge::update(const QRectF &chromeRect,
                                   const QVariantList &interactionRects,
                                   int maximumInteractionRects)
{
    if (!m_window)
        return;

    const int available = interactionRects.size();
    const int limit = maximumInteractionRects < 0
        ? available
        : qBound(0, maximumInteractionRects, available);
    m_interactionRects.clear();
    m_interactionRects.reserve(limit);
    for (int i = 0; i < limit; ++i) {
        if (const auto rect = rectangleFromVariant(interactionRects.at(i)))
            m_interactionRects.append(*rect);
    }

    QRegion next = DockInputRegionPolicy::regionFor(m_window->size(), chromeRect,
                                                     m_interactionRects,
                                                     m_interactionRects.size());
    // An empty QRegion resets a QWindow mask to the entire surface. Keep a
    // malformed or transient geometry update from making the full headroom
    // interactive. A normal mapped Dock always contributes its chrome.
    if (next.isEmpty() && !m_window->size().isEmpty())
        next = QRegion(QRect(0, 0, 1, 1));

    if (m_hasAppliedRegion && next == m_lastRegion)
        return;

    m_window->setMask(next);
    m_lastRegion = next;
    m_hasAppliedRegion = true;
    emit regionApplied();
}
