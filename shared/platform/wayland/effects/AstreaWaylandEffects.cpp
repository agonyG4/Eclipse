#include "platform/wayland/effects/AstreaWaylandEffects.hpp"

#include "platform/wayland/effects/QtWaylandWindow.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQuickWindow>

#include <algorithm>
#include <cerrno>
#include <cstring>

#if ASTREA_HAVE_WAYLAND_EFFECTS
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include "ext-background-effect-v1-client-protocol.h"
#endif

AstreaWaylandEffects *AstreaWaylandEffects::instance()
{
    auto *application = QCoreApplication::instance();
    if (!application)
        return nullptr;
    if (auto *existing = application->findChild<AstreaWaylandEffects *>())
        return existing;
    return new AstreaWaylandEffects(application);
}

AstreaWaylandEffects::AstreaWaylandEffects(QObject *parent)
    : QObject(parent)
{
}

AstreaWaylandEffects::~AstreaWaylandEffects()
{
#if ASTREA_HAVE_WAYLAND_EFFECTS
    if (m_manager)
        ext_background_effect_manager_v1_destroy(m_manager);
#endif
}

bool AstreaWaylandEffects::initialize()
{
    if (m_initialized)
        return m_available;
    m_initialized = true;

#if !ASTREA_HAVE_WAYLAND_EFFECTS
    setError(QStringLiteral("Wayland background effects are unavailable in this build"));
    return false;
#else
    m_display = astreaQtWaylandDisplay();
    m_compositor = astreaQtWaylandCompositor();
    if (!m_display || !m_compositor) {
        setError(QStringLiteral("Qt did not expose its Wayland display and compositor"));
        return false;
    }

    struct Probe {
        ext_background_effect_manager_v1 *manager = nullptr;
        AstreaWaylandEffects *owner = nullptr;
    } probe;
    static const ext_background_effect_manager_v1_listener capabilitiesListener{
        &AstreaWaylandEffects::handleCapabilities,
    };
    const wl_registry_listener listener{
        [](void *data, wl_registry *registry, uint32_t name, const char *interface,
           uint32_t version) {
            auto *probe = static_cast<Probe *>(data);
            if (std::strcmp(interface, "ext_background_effect_manager_v1") == 0) {
                probe->manager = static_cast<ext_background_effect_manager_v1 *>(
                    wl_registry_bind(registry, name, &ext_background_effect_manager_v1_interface,
                                     std::min(version, 1u)));
                if (probe->manager)
                    ext_background_effect_manager_v1_add_listener(
                        probe->manager, &capabilitiesListener, probe->owner);
            }
        },
        [](void *, wl_registry *, uint32_t) {},
    };
    probe.owner = this;
    wl_registry *registry = wl_display_get_registry(m_display);
    if (!registry || wl_registry_add_listener(registry, &listener, &probe) < 0
        || wl_display_roundtrip(m_display) < 0) {
        if (registry)
            wl_registry_destroy(registry);
        setError(QStringLiteral("Wayland registry probe failed for background effects"));
        return false;
    }
    wl_registry_destroy(registry);
    m_manager = probe.manager;
    if (!m_manager) {
        setError(QStringLiteral("Compositor does not advertise ext_background_effect_v1"));
        return false;
    }

    m_capabilityState.setManagerBound(true);
    updateAvailability();
    if (wl_display_roundtrip(m_display) < 0) {
        setError(QStringLiteral("Wayland capability synchronization failed"));
        return false;
    }
    updateAvailability();
    if (!m_available)
        setError(QStringLiteral("Compositor does not advertise blur capability"));
    return true;
#endif
}

void AstreaWaylandEffects::handleCapabilities(void *data,
                                               ext_background_effect_manager_v1 *,
                                               uint32_t flags)
{
    auto *effects = static_cast<AstreaWaylandEffects *>(data);
    if (effects)
        effects->updateCapabilities(flags);
}

void AstreaWaylandEffects::updateCapabilities(const uint32_t flags)
{
    m_capabilityState.setCapabilities(flags);
    updateAvailability();
}

void AstreaWaylandEffects::updateAvailability()
{
    const bool available = m_capabilityState.available();
    if (m_available == available)
        return;
    m_available = available;
    if (m_available)
        m_error.clear();
    emit availableChanged();
}

bool AstreaWaylandEffects::createEffect(
    wl_surface *surface, ext_background_effect_surface_v1 **effect)
{
    if (effect)
        *effect = nullptr;
    if (!effect || !surface || !initialize() || !m_manager || !m_available)
        return false;

#if !ASTREA_HAVE_WAYLAND_EFFECTS
    Q_UNUSED(surface);
    Q_UNUSED(effect);
    return false;
#else
    *effect = ext_background_effect_manager_v1_get_background_effect(m_manager, surface);
    return *effect != nullptr;
#endif
}

bool AstreaWaylandEffects::setBlurRegion(
    QQuickWindow *window, ext_background_effect_surface_v1 *effect,
    const QVector<QRect> &rectangles)
{
    if (!window || !effect || !initialize() || !m_available)
        return false;

#if !ASTREA_HAVE_WAYLAND_EFFECTS
    Q_UNUSED(window);
    Q_UNUSED(effect);
    Q_UNUSED(rectangles);
    return false;
#else

    wl_region *region = nullptr;
    if (!rectangles.isEmpty()) {
        region = wl_compositor_create_region(m_compositor);
        if (!region)
            return false;
        for (const QRect &rectangle : rectangles) {
            if (rectangle.width() > 0 && rectangle.height() > 0)
                wl_region_add(region, rectangle.x(), rectangle.y(), rectangle.width(),
                             rectangle.height());
        }
    }
    ext_background_effect_surface_v1_set_blur_region(effect, region);
    if (region)
        wl_region_destroy(region);
    requestQtFrame(window);
    if (wl_display_flush(m_display) < 0 && errno != EAGAIN)
        return false;
    return true;
#endif
}

void AstreaWaylandEffects::destroyEffect(
    QQuickWindow *window, ext_background_effect_surface_v1 *effect, const bool requestFrame)
{
#if ASTREA_HAVE_WAYLAND_EFFECTS
    if (!m_display || !effect)
        return;
    ext_background_effect_surface_v1_destroy(effect);
    if (requestFrame)
        requestQtFrame(window);
    wl_display_flush(m_display);
#else
    Q_UNUSED(window);
    Q_UNUSED(effect);
    Q_UNUSED(requestFrame);
#endif
}

void AstreaWaylandEffects::requestQtFrame(QQuickWindow *window) const
{
    if (window)
        window->update();
}

void AstreaWaylandEffects::setError(const QString &error)
{
    m_error = error;
}
