#include "platform/wayland/effects/AstreaBackgroundEffectBinding.hpp"

#include "platform/wayland/effects/AstreaWaylandEffects.hpp"
#include "platform/wayland/effects/QtWaylandWindow.hpp"

#include <QQuickWindow>

bool AstreaBackgroundEffectLifecycleState::surfaceCreated(const quintptr surfaceToken)
{
    if (surfaceToken == 0 || m_surfaceToken == surfaceToken)
        return false;
    m_surfaceToken = surfaceToken;
    m_effectBound = false;
    ++m_generation;
    return true;
}

bool AstreaBackgroundEffectLifecycleState::surfaceAboutToBeDestroyed(const quintptr surfaceToken)
{
    if (surfaceToken == 0 || m_surfaceToken != surfaceToken)
        return false;
    m_surfaceToken = 0;
    m_effectBound = false;
    m_syncedRegion.clear();
    return true;
}

bool AstreaBackgroundEffectLifecycleState::bindEffect()
{
    if (!hasSurface() || m_effectBound)
        return false;
    m_effectBound = true;
    ++m_totalEffectBindings;
    return true;
}

bool AstreaBackgroundEffectLifecycleState::recordRegionSync(const QVector<QRect> &region,
                                                             const bool requestFrame)
{
    if (!hasEffect() || m_syncedRegion == region)
        return false;
    m_syncedRegion = region;
    if (requestFrame)
        ++m_qtFrameRequests;
    return true;
}

bool AstreaBackgroundEffectLifecycleState::clearRegion(const bool requestFrame)
{
    if (m_syncedRegion.isEmpty())
        return false;
    m_syncedRegion.clear();
    if (requestFrame)
        ++m_qtFrameRequests;
    return true;
}

void AstreaBackgroundEffectLifecycleState::destroyEffect()
{
    m_effectBound = false;
    m_syncedRegion.clear();
}

AstreaBackgroundEffectBinding::AstreaBackgroundEffectBinding(QQuickWindow *window)
    : m_window(window)
{
}

AstreaBackgroundEffectBinding::~AstreaBackgroundEffectBinding()
{
    surfaceAboutToBeDestroyed();
}

bool AstreaBackgroundEffectBinding::sync(const QVector<QRect> &rectangles,
                                         const bool requested, const bool requestFrame)
{
    auto *window = m_window.data();
    auto *effects = AstreaWaylandEffects::instance();
    const bool initialized = effects && effects->initialize();
    m_available = initialized && effects->available();

    auto *resolvedSurface = window ? astreaQtWaylandSurface(window) : nullptr;
    if (resolvedSurface != m_surface) {
        releaseEffect(false);
        if (m_surface)
            m_lifecycle.surfaceAboutToBeDestroyed(reinterpret_cast<quintptr>(m_surface));
        m_surface = resolvedSurface;
        m_lastRegion.clear();
        if (m_surface)
            m_lifecycle.surfaceCreated(reinterpret_cast<quintptr>(m_surface));
    }

    if (!window || !m_available || !requested || rectangles.isEmpty() || !window->isVisible()
        || window->width() <= 0 || window->height() <= 0 || !m_surface) {
        releaseEffect(requestFrame);
        return false;
    }

    if (!m_effect && effects->createEffect(m_surface, &m_effect))
        m_lifecycle.bindEffect();
    if (!m_effect)
        return false;

    if (m_active && m_lastRegion == rectangles)
        return true;

    if (!effects->setBlurRegion(window, m_effect, rectangles, requestFrame)) {
        m_lifecycle.clearRegion(false);
        m_lastRegion.clear();
        m_active = false;
        return false;
    }
    m_lifecycle.recordRegionSync(rectangles, requestFrame);
    m_lastRegion = rectangles;
    m_active = true;
    return true;
}

void AstreaBackgroundEffectBinding::surfaceCreated()
{
    sync(m_lastRegion, !m_lastRegion.isEmpty(), false);
}

void AstreaBackgroundEffectBinding::surfaceAboutToBeDestroyed()
{
    releaseEffect(false);
    if (m_surface) {
        m_lifecycle.surfaceAboutToBeDestroyed(reinterpret_cast<quintptr>(m_surface));
        m_surface = nullptr;
    }
    m_lastRegion.clear();
    m_active = false;
}

void AstreaBackgroundEffectBinding::releaseEffect(const bool requestFrame)
{
    if (!m_effect) {
        m_active = false;
        return;
    }
    if (auto *effects = AstreaWaylandEffects::instance())
        effects->destroyEffect(m_window.data(), m_effect, requestFrame);
    m_lifecycle.clearRegion(requestFrame);
    m_effect = nullptr;
    m_lifecycle.destroyEffect();
    m_lastRegion.clear();
    m_active = false;
}
