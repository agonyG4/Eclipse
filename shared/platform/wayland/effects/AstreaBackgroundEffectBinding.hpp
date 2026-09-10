#pragma once

#include <QPointer>
#include <QRect>
#include <QVector>

#include <QtGlobal>

class QQuickWindow;
struct wl_surface;
struct ext_background_effect_surface_v1;

class AstreaBackgroundEffectLifecycleState final {
public:
    bool surfaceCreated(quintptr surfaceToken);
    bool surfaceAboutToBeDestroyed(quintptr surfaceToken);
    bool bindEffect();
    bool recordRegionSync(const QVector<QRect> &region, bool requestFrame);
    bool clearRegion(bool requestFrame);
    void destroyEffect();

    quint64 generation() const { return m_generation; }
    bool hasSurface() const { return m_surfaceToken != 0; }
    bool hasEffect() const { return m_effectBound; }
    quint64 totalEffectBindings() const { return m_totalEffectBindings; }
    quint64 qtFrameRequests() const { return m_qtFrameRequests; }

private:
    quintptr m_surfaceToken = 0;
    quint64 m_generation = 0;
    quint64 m_totalEffectBindings = 0;
    quint64 m_qtFrameRequests = 0;
    QVector<QRect> m_syncedRegion;
    bool m_effectBound = false;
};

// Owns the complete ext-background-effect binding for one Qt window/native
// wl_surface. Both the Settings child window and existing-window Shell
// regions use this lifecycle, so there is only one protocol object per
// native surface.
class AstreaBackgroundEffectBinding final {
public:
    explicit AstreaBackgroundEffectBinding(QQuickWindow *window);
    ~AstreaBackgroundEffectBinding();

    bool sync(const QVector<QRect> &rectangles, bool requested, bool requestFrame);
    void surfaceCreated();
    void surfaceAboutToBeDestroyed();

    bool available() const { return m_available; }
    bool active() const { return m_active; }
    quint64 surfaceGeneration() const { return m_lifecycle.generation(); }
    bool hasEffectProxy() const { return m_effect != nullptr; }
    const QVector<QRect> &lastRegion() const { return m_lastRegion; }

private:
    void releaseEffect(bool requestFrame);

    QPointer<QQuickWindow> m_window;
    wl_surface *m_surface = nullptr;
    ext_background_effect_surface_v1 *m_effect = nullptr;
    AstreaBackgroundEffectLifecycleState m_lifecycle;
    QVector<QRect> m_lastRegion;
    bool m_available = false;
    bool m_active = false;
};
