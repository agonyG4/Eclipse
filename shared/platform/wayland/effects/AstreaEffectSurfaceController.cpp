#include "platform/wayland/effects/AstreaEffectSurfaceController.hpp"

#include "platform/wayland/effects/AstreaWaylandEffects.hpp"
#include "platform/wayland/effects/RoundedEffectRegion.hpp"

#include <QQuickWindow>

AstreaEffectSurfaceController::AstreaEffectSurfaceController(QQuickWindow *window)
    : m_window(window)
    , m_binding(std::make_unique<AstreaBackgroundEffectBinding>(window))
{
    auto *effects = AstreaWaylandEffects::instance();
    const bool initialized = effects && effects->initialize();
    m_available = initialized && effects->available();
}

AstreaEffectSurfaceController::~AstreaEffectSurfaceController()
{
    surfaceAboutToBeDestroyed();
}

bool AstreaEffectSurfaceController::sync()
{
    const auto *window = m_window;
    const auto rectangles = window
        ? AstreaRoundedEffectRegion::rectangles(window->size(), m_cornerRadius)
        : QVector<QRect>{};
    m_active = m_binding->sync(rectangles, m_enabled, true);
    m_available = m_binding->available();
    return m_active;
}

void AstreaEffectSurfaceController::surfaceCreated()
{
    m_binding->surfaceCreated();
    sync();
}

void AstreaEffectSurfaceController::surfaceAboutToBeDestroyed()
{
    m_binding->surfaceAboutToBeDestroyed();
    m_active = false;
}
