#pragma once

#include "platform/wayland/effects/AstreaAnalyticBackdropCoverage.hpp"

#include <QPointer>
#include <QRect>
#include <QVector>

#include <QtGlobal>

#include <optional>

class QQuickWindow;
struct wl_surface;
struct ext_background_effect_surface_v1;
struct astrea_background_effect_coverage_v1;

enum class AstreaBackgroundEffectMode : quint8 {
    Disabled,
    Fallback,
    Analytic,
};

class AstreaBackgroundEffectLifecycleState final {
public:
    bool surfaceCreated(quintptr surfaceToken);
    bool surfaceAboutToBeDestroyed(quintptr surfaceToken);
    bool bindEffect();
    bool bindCoverage();
    bool recordSync(AstreaBackgroundEffectMode mode, const QVector<QRect> &publicRegion,
                    const std::optional<AstreaAnalyticBackdropCoverage> &coverage,
                    bool requestFrame);
    bool matches(AstreaBackgroundEffectMode mode, const QVector<QRect> &publicRegion,
                 const std::optional<AstreaAnalyticBackdropCoverage> &coverage) const;
    void destroyEffect();
    void destroyCoverage();

    quint64 generation() const { return m_generation; }
    bool hasSurface() const { return m_surfaceToken != 0; }
    bool hasEffect() const { return m_effectBound; }
    bool hasCoverage() const { return m_coverageBound; }
    AstreaBackgroundEffectMode mode() const { return m_mode; }
    const QVector<QRect> &publicRegion() const { return m_publicRegion; }
    const std::optional<AstreaAnalyticBackdropCoverage> &coverage() const
    {
        return m_coverage;
    }
    quint64 totalEffectBindings() const { return m_totalEffectBindings; }
    quint64 totalCoverageBindings() const { return m_totalCoverageBindings; }
    quint64 qtFrameRequests() const { return m_qtFrameRequests; }

private:
    quintptr m_surfaceToken = 0;
    quint64 m_generation = 0;
    quint64 m_totalEffectBindings = 0;
    quint64 m_totalCoverageBindings = 0;
    quint64 m_qtFrameRequests = 0;
    QVector<QRect> m_publicRegion;
    std::optional<AstreaAnalyticBackdropCoverage> m_coverage;
    AstreaBackgroundEffectMode m_mode = AstreaBackgroundEffectMode::Disabled;
    bool m_effectBound = false;
    bool m_coverageBound = false;
};

// Owns the complete ext-background-effect binding for one Qt window/native
// wl_surface. Both the Settings child window and existing-window Shell
// regions use this lifecycle, so there is only one protocol object per
// native surface.
class AstreaBackgroundEffectBinding final {
public:
    explicit AstreaBackgroundEffectBinding(QQuickWindow *window);
    ~AstreaBackgroundEffectBinding();

    bool sync(const QVector<QRect> &fallbackRegion,
              const std::optional<AstreaAnalyticBackdropCoverage> &analyticCoverage,
              bool requested, bool requestFrame);
    void surfaceCreated();
    void surfaceAboutToBeDestroyed();

    bool available() const { return m_available; }
    bool active() const { return m_active; }
    bool analyticActive() const { return m_analyticActive; }
    quint64 surfaceGeneration() const { return m_lifecycle.generation(); }
    bool hasEffectProxy() const { return m_effect != nullptr; }
    bool hasCoverageProxy() const { return m_coverage != nullptr; }
    const QVector<QRect> &lastRegion() const { return m_lifecycle.publicRegion(); }
    const QVector<QRect> &lastProtocolRegion() const { return m_lifecycle.publicRegion(); }
    AstreaBackgroundEffectMode lastMode() const { return m_lifecycle.mode(); }

private:
    bool disable(bool requestFrame);
    void releaseSurfaceObjects();

    QPointer<QQuickWindow> m_window;
    wl_surface *m_surface = nullptr;
    ext_background_effect_surface_v1 *m_effect = nullptr;
    astrea_background_effect_coverage_v1 *m_coverage = nullptr;
    AstreaBackgroundEffectLifecycleState m_lifecycle;
    QVector<QRect> m_lastFallbackRegion;
    std::optional<AstreaAnalyticBackdropCoverage> m_lastAnalyticCoverage;
    bool m_lastRequested = false;
    bool m_available = false;
    bool m_active = false;
    bool m_analyticActive = false;
};
