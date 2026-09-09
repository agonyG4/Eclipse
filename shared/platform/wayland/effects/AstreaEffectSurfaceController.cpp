#include "platform/wayland/effects/AstreaEffectSurfaceController.hpp"

#include "platform/wayland/effects/AstreaWaylandEffects.hpp"
#include "platform/wayland/effects/QtWaylandWindow.hpp"
#include "platform/wayland/effects/RoundedEffectRegion.hpp"

#include <QQuickWindow>

bool AstreaEffectSurfaceLifecycleState::surfaceCreated(const quintptr surfaceToken)
{
    if (surfaceToken == 0 || m_surfaceToken == surfaceToken)
        return false;
    m_surfaceToken = surfaceToken;
    m_effectBound = false;
    ++m_generation;
    return true;
}

bool AstreaEffectSurfaceLifecycleState::surfaceAboutToBeDestroyed(const quintptr surfaceToken)
{
    if (surfaceToken == 0 || m_surfaceToken != surfaceToken)
        return false;
    m_surfaceToken = 0;
    m_effectBound = false;
    return true;
}

bool AstreaEffectSurfaceLifecycleState::bindEffect()
{
    if (!hasSurface() || m_effectBound)
        return false;
    m_effectBound = true;
    ++m_totalEffectBindings;
    return true;
}

void AstreaEffectSurfaceLifecycleState::destroyEffect()
{
    m_effectBound = false;
}

AstreaEffectSurfaceController::AstreaEffectSurfaceController(QQuickWindow *window)
    : m_window(window)
{
}

AstreaEffectSurfaceController::~AstreaEffectSurfaceController()
{
    surfaceAboutToBeDestroyed();
}

bool AstreaEffectSurfaceController::sync()
{
    auto *window = m_window.data();
    auto *effects = AstreaWaylandEffects::instance();
    m_available = effects && effects->initialize();

    auto *resolvedSurface = window ? astreaQtWaylandSurface(window) : nullptr;
    if (resolvedSurface != m_surface) {
        releaseEffect(false);
        if (m_surface)
            m_lifecycle.surfaceAboutToBeDestroyed(reinterpret_cast<quintptr>(m_surface));
        m_surface = resolvedSurface;
        if (m_surface)
            m_lifecycle.surfaceCreated(reinterpret_cast<quintptr>(m_surface));
    }

    if (!window || !m_available || !m_enabled || !window->isVisible() || window->width() <= 0
        || window->height() <= 0 || !m_surface) {
        releaseEffect(true);
        return false;
    }

    if (!m_effect && effects->createEffect(m_surface, &m_effect))
        m_lifecycle.bindEffect();
    if (!m_effect)
        return false;

    m_active = effects->setBlurRegion(
        window, m_effect, AstreaRoundedEffectRegion::rectangles(window->size(), m_cornerRadius));
    return m_active;
}

void AstreaEffectSurfaceController::surfaceCreated()
{
    sync();
}

void AstreaEffectSurfaceController::surfaceAboutToBeDestroyed()
{
    releaseEffect(false);
    if (m_surface) {
        m_lifecycle.surfaceAboutToBeDestroyed(reinterpret_cast<quintptr>(m_surface));
        m_surface = nullptr;
    }
    m_active = false;
}

void AstreaEffectSurfaceController::releaseEffect(const bool requestFrame)
{
    if (!m_effect) {
        m_active = false;
        return;
    }
    if (auto *effects = AstreaWaylandEffects::instance())
        effects->destroyEffect(m_window.data(), m_effect, requestFrame);
    m_effect = nullptr;
    m_lifecycle.destroyEffect();
    m_active = false;
}
