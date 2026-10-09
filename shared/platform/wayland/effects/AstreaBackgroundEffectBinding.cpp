#include "platform/wayland/effects/AstreaBackgroundEffectBinding.hpp"

#include "platform/wayland/effects/AstreaBackdropEffectRegions.hpp"
#include "platform/wayland/effects/AstreaWaylandEffects.hpp"
#include "platform/wayland/effects/QtWaylandWindow.hpp"

#include <QDebug>
#include <QQuickWindow>

bool AstreaBackgroundEffectLifecycleState::surfaceCreated(const quintptr surfaceToken)
{
    if (surfaceToken == 0 || m_surfaceToken == surfaceToken)
        return false;
    m_surfaceToken = surfaceToken;
    m_effectBound = false;
    m_coverageBound = false;
    m_mode = AstreaBackgroundEffectMode::Disabled;
    m_publicRegion.clear();
    m_coverage.reset();
    ++m_generation;
    return true;
}

bool AstreaBackgroundEffectLifecycleState::surfaceAboutToBeDestroyed(
    const quintptr surfaceToken)
{
    if (surfaceToken == 0 || m_surfaceToken != surfaceToken)
        return false;
    m_surfaceToken = 0;
    m_effectBound = false;
    m_coverageBound = false;
    m_mode = AstreaBackgroundEffectMode::Disabled;
    m_publicRegion.clear();
    m_coverage.reset();
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

bool AstreaBackgroundEffectLifecycleState::bindCoverage()
{
    if (!hasSurface() || m_coverageBound)
        return false;
    m_coverageBound = true;
    ++m_totalCoverageBindings;
    return true;
}

bool AstreaBackgroundEffectLifecycleState::matches(
    const AstreaBackgroundEffectMode mode, const QVector<QRect> &publicRegion,
    const std::optional<AstreaAnalyticBackdropCoverage> &coverage) const
{
    return m_mode == mode && m_publicRegion == publicRegion && m_coverage == coverage;
}

bool AstreaBackgroundEffectLifecycleState::recordSync(
    const AstreaBackgroundEffectMode mode, const QVector<QRect> &publicRegion,
    const std::optional<AstreaAnalyticBackdropCoverage> &coverage, const bool requestFrame)
{
    if (!hasSurface() || matches(mode, publicRegion, coverage))
        return false;
    m_mode = mode;
    m_publicRegion = publicRegion;
    m_coverage = coverage;
    if (requestFrame)
        ++m_qtFrameRequests;
    return true;
}

void AstreaBackgroundEffectLifecycleState::destroyEffect()
{
    m_effectBound = false;
}

void AstreaBackgroundEffectLifecycleState::destroyCoverage()
{
    m_coverageBound = false;
}

AstreaBackgroundEffectBinding::AstreaBackgroundEffectBinding(QQuickWindow *window)
    : m_window(window)
{
}

AstreaBackgroundEffectBinding::~AstreaBackgroundEffectBinding()
{
    surfaceAboutToBeDestroyed();
}

bool AstreaBackgroundEffectBinding::sync(
    const QVector<QRect> &fallbackRegion,
    const std::optional<AstreaAnalyticBackdropCoverage> &analyticCoverage,
    const bool requested, const bool requestFrame)
{
    m_lastFallbackRegion = fallbackRegion;
    m_lastAnalyticCoverage = analyticCoverage;
    m_lastRequested = requested;

    auto *window = m_window.data();
    auto *effects = AstreaWaylandEffects::instance();
    const bool initialized = effects && effects->initialize();
    m_available = initialized && effects->available();

    auto *resolvedSurface = window ? astreaQtWaylandSurface(window) : nullptr;
    if (resolvedSurface != m_surface) {
        releaseSurfaceObjects();
        if (m_surface)
            m_lifecycle.surfaceAboutToBeDestroyed(reinterpret_cast<quintptr>(m_surface));
        m_surface = resolvedSurface;
        if (m_surface)
            m_lifecycle.surfaceCreated(reinterpret_cast<quintptr>(m_surface));
    }

    if (!window || !m_available || !requested || !window->isVisible()
        || window->width() <= 0 || window->height() <= 0 || !m_surface) {
        return disable(requestFrame);
    }

    bool newEffect = false;
    if (!m_effect) {
        if (!effects->createEffect(m_surface, &m_effect))
            return false;
        m_lifecycle.bindEffect();
        newEffect = true;
    }

    std::optional<AstreaAnalyticBackdropCoverage> selectedCoverage;
    AstreaBackgroundEffectMode selectedMode = AstreaBackgroundEffectMode::Fallback;
    QVector<QRect> protocolRegion = fallbackRegion;
    if (analyticCoverage && effects->analyticCoverageAvailable()) {
        if (!m_coverage) {
            if (effects->createCoverage(m_surface, &m_coverage))
                m_lifecycle.bindCoverage();
            else {
                static bool diagnosticLogged = false;
                if (!diagnosticLogged) {
                    qWarning() << "Typhon analytic background coverage proxy creation failed;"
                                   " using the public fallback";
                    diagnosticLogged = true;
                }
            }
        }
        if (m_coverage) {
            const auto workRegion = AstreaBackdropEffectRegions::analyticWorkRegion(
                *analyticCoverage, window->size());
            if (!workRegion.isEmpty()) {
                selectedCoverage = analyticCoverage;
                selectedMode = AstreaBackgroundEffectMode::Analytic;
                protocolRegion = workRegion;
            }
        }
    }

    if (protocolRegion.isEmpty())
        return disable(requestFrame);

    const bool publicRegionChanged = newEffect || m_lifecycle.publicRegion() != protocolRegion;
    const bool coverageChanged = selectedMode == AstreaBackgroundEffectMode::Analytic
        && (m_lifecycle.mode() != AstreaBackgroundEffectMode::Analytic
            || m_lifecycle.coverage() != selectedCoverage);
    const bool clearCoverage = selectedMode != AstreaBackgroundEffectMode::Analytic
        && m_coverage && m_lifecycle.mode() == AstreaBackgroundEffectMode::Analytic;
    const bool stateChanged = !m_lifecycle.matches(selectedMode, protocolRegion,
                                                   selectedCoverage);
    if (!stateChanged && !newEffect)
        return true;

    wl_region *region = publicRegionChanged ? effects->createRegion(protocolRegion) : nullptr;
    if (publicRegionChanged && !region) {
        if (selectedMode == AstreaBackgroundEffectMode::Analytic) {
            // No private request has been queued yet. Retry the portable region
            // before changing any surface state.
            const bool fallbackRegionChanged = newEffect
                || m_lifecycle.publicRegion() != fallbackRegion;
            if (fallbackRegion.isEmpty())
                return disable(requestFrame);
            wl_region *fallbackProxy = fallbackRegionChanged
                ? effects->createRegion(fallbackRegion) : nullptr;
            if (fallbackRegionChanged && !fallbackProxy)
                return false;
            const bool fallbackClear = m_coverage
                && m_lifecycle.mode() == AstreaBackgroundEffectMode::Analytic;
            effects->submitState(window, m_effect, m_coverage, fallbackProxy,
                                 fallbackRegionChanged, std::nullopt, fallbackClear,
                                 false, requestFrame);
            m_lifecycle.recordSync(AstreaBackgroundEffectMode::Fallback, fallbackRegion,
                                   std::nullopt, requestFrame);
            m_active = true;
            m_analyticActive = false;
            return true;
        }
        return false;
    }

    const bool removePublicEffect = false;
    effects->submitState(window, m_effect, m_coverage, region, publicRegionChanged,
                         coverageChanged ? selectedCoverage : std::nullopt, clearCoverage,
                         removePublicEffect, requestFrame);
    m_lifecycle.recordSync(selectedMode, protocolRegion, selectedCoverage, requestFrame);
    m_active = true;
    m_analyticActive = selectedMode == AstreaBackgroundEffectMode::Analytic;
    return true;
}

bool AstreaBackgroundEffectBinding::disable(const bool requestFrame)
{
    auto *effects = AstreaWaylandEffects::instance();
    const bool removePublicEffect = m_effect != nullptr;
    const bool clearCoverage = m_coverage
        && m_lifecycle.mode() == AstreaBackgroundEffectMode::Analytic;
    const QVector<QRect> emptyRegion;
    const bool stateChanged = !m_lifecycle.matches(AstreaBackgroundEffectMode::Disabled,
                                                  emptyRegion, std::nullopt);
    if (!removePublicEffect && !clearCoverage && !stateChanged) {
        m_active = false;
        m_analyticActive = false;
        return false;
    }

    if (effects)
        effects->submitState(m_window.data(), m_effect, m_coverage, nullptr, false,
                             std::nullopt, clearCoverage, removePublicEffect, requestFrame);
    if (removePublicEffect) {
        m_effect = nullptr;
        m_lifecycle.destroyEffect();
    }
    m_lifecycle.recordSync(AstreaBackgroundEffectMode::Disabled, emptyRegion,
                           std::nullopt, requestFrame);
    m_active = false;
    m_analyticActive = false;
    return true;
}

void AstreaBackgroundEffectBinding::surfaceCreated()
{
    if (m_lastRequested)
        sync(m_lastFallbackRegion, m_lastAnalyticCoverage, true, false);
}

void AstreaBackgroundEffectBinding::surfaceAboutToBeDestroyed()
{
    releaseSurfaceObjects();
    if (m_surface) {
        m_lifecycle.surfaceAboutToBeDestroyed(reinterpret_cast<quintptr>(m_surface));
        m_surface = nullptr;
    }
    m_active = false;
    m_analyticActive = false;
}

void AstreaBackgroundEffectBinding::releaseSurfaceObjects()
{
    if (m_effect || m_coverage) {
        if (auto *effects = AstreaWaylandEffects::instance())
            effects->destroySurfaceObjects(m_effect, m_coverage);
    }
    m_effect = nullptr;
    m_coverage = nullptr;
    m_lifecycle.destroyEffect();
    m_lifecycle.destroyCoverage();
    m_active = false;
    m_analyticActive = false;
}
