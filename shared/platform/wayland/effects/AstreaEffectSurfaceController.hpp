#pragma once

#include "platform/wayland/effects/AstreaBackgroundEffectBinding.hpp"

#include <QtGlobal>

#include <memory>

class QQuickWindow;
struct wl_surface;
struct ext_background_effect_surface_v1;

using AstreaEffectSurfaceLifecycleState = AstreaBackgroundEffectLifecycleState;

class AstreaEffectSurfaceController final {
public:
    explicit AstreaEffectSurfaceController(QQuickWindow *window);
    ~AstreaEffectSurfaceController();

    bool sync();
    void surfaceCreated();
    void surfaceAboutToBeDestroyed();
    void setEnabled(bool enabled) { m_enabled = enabled; }
    void setCornerRadius(qreal radius) { m_cornerRadius = radius; }

    bool available() const { return m_available; }
    bool active() const { return m_active; }
    bool enabled() const { return m_enabled; }
    qreal cornerRadius() const { return m_cornerRadius; }
    quint64 surfaceGeneration() const { return m_binding->surfaceGeneration(); }
    bool hasEffectProxy() const { return m_binding->hasEffectProxy(); }

private:
    QQuickWindow *m_window = nullptr;
    std::unique_ptr<AstreaBackgroundEffectBinding> m_binding;
    qreal m_cornerRadius = 18.0;
    bool m_enabled = true;
    bool m_available = false;
    bool m_active = false;
};
