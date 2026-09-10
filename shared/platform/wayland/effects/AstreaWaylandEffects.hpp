#pragma once

#include <QObject>
#include <QRect>
#include <QString>
#include <QVector>

#include <cstdint>
#include <memory>

class QQuickWindow;
struct wl_compositor;
struct wl_display;
struct wl_surface;
struct ext_background_effect_manager_v1;
struct ext_background_effect_surface_v1;

class AstreaWaylandEffectsCapabilityState final {
public:
    static constexpr uint32_t BlurCapability = 1u;

    bool managerBound() const { return m_managerBound; }
    bool available() const { return m_managerBound && (m_capabilities & BlurCapability) != 0; }
    uint32_t capabilities() const { return m_capabilities; }

    bool setManagerBound(bool bound)
    {
        const bool wasAvailable = available();
        m_managerBound = bound;
        return wasAvailable != available();
    }

    bool setCapabilities(uint32_t capabilities)
    {
        const bool wasAvailable = available();
        m_capabilities = capabilities;
        return wasAvailable != available();
    }

private:
    uint32_t m_capabilities = 0;
    bool m_managerBound = false;
};

class AstreaWaylandEffects final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    static AstreaWaylandEffects *instance();
    explicit AstreaWaylandEffects(QObject *parent = nullptr);
    ~AstreaWaylandEffects() override;

    bool initialize();
    bool available() const { return m_available; }
    QString errorString() const { return m_error; }

    bool createEffect(wl_surface *surface, ext_background_effect_surface_v1 **effect);
    bool setBlurRegion(QQuickWindow *window, ext_background_effect_surface_v1 *effect,
                       const QVector<QRect> &rectangles, bool requestFrame = true);
    void destroyEffect(QQuickWindow *window, ext_background_effect_surface_v1 *effect,
                       bool requestFrame);

signals:
    void availableChanged();

private:
    static void handleCapabilities(void *data, ext_background_effect_manager_v1 *manager,
                                   uint32_t flags);
    void updateCapabilities(uint32_t flags);
    void updateAvailability();
    void setError(const QString &error);
    void requestQtFrame(QQuickWindow *window) const;

    wl_display *m_display = nullptr;
    wl_compositor *m_compositor = nullptr;
    ext_background_effect_manager_v1 *m_manager = nullptr;
    QString m_error;
    AstreaWaylandEffectsCapabilityState m_capabilityState;
    bool m_initialized = false;
    bool m_available = false;
};
