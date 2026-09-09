#pragma once

#include <QPointer>
#include <QtGlobal>

class QQuickWindow;
struct wl_surface;
struct ext_background_effect_surface_v1;

class AstreaEffectSurfaceLifecycleState final {
public:
    bool surfaceCreated(quintptr surfaceToken);
    bool surfaceAboutToBeDestroyed(quintptr surfaceToken);
    bool bindEffect();
    void destroyEffect();

    quint64 generation() const { return m_generation; }
    bool hasSurface() const { return m_surfaceToken != 0; }
    bool hasEffect() const { return m_effectBound; }
    quint64 totalEffectBindings() const { return m_totalEffectBindings; }

private:
    quintptr m_surfaceToken = 0;
    quint64 m_generation = 0;
    quint64 m_totalEffectBindings = 0;
    bool m_effectBound = false;
};

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
    quint64 surfaceGeneration() const { return m_lifecycle.generation(); }
    bool hasEffectProxy() const { return m_effect != nullptr; }

private:
    void releaseEffect(bool requestFrame);

    QPointer<QQuickWindow> m_window;
    wl_surface *m_surface = nullptr;
    ext_background_effect_surface_v1 *m_effect = nullptr;
    AstreaEffectSurfaceLifecycleState m_lifecycle;
    qreal m_cornerRadius = 18.0;
    bool m_enabled = true;
    bool m_available = false;
    bool m_active = false;
};
