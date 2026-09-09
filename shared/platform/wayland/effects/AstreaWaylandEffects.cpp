#include "platform/wayland/effects/AstreaWaylandEffects.hpp"

#include "platform/wayland/effects/QtWaylandWindow.hpp"

#include <QCoreApplication>
#include <QGuiApplication>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>

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
    for (auto *effect : std::as_const(m_surfaces))
        ext_background_effect_surface_v1_destroy(effect);
    m_surfaces.clear();
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
    m_available = m_capabilities & 1u;
    if (!m_available) {
        setError(QStringLiteral("Compositor does not advertise blur capability"));
        ext_background_effect_manager_v1_destroy(m_manager);
        m_manager = nullptr;
        return false;
    }
    emit availableChanged();
    return true;
#endif
}

void AstreaWaylandEffects::handleCapabilities(void *data,
                                               ext_background_effect_manager_v1 *,
                                               uint32_t flags)
{
    auto *effects = static_cast<AstreaWaylandEffects *>(data);
    if (effects)
        effects->m_capabilities = flags;
}

bool AstreaWaylandEffects::setBlurRegion(QWindow *window, const QVector<QRect> &rectangles)
{
    if (!initialize())
        return false;

#if !ASTREA_HAVE_WAYLAND_EFFECTS
    Q_UNUSED(window);
    Q_UNUSED(rectangles);
    return false;
#else
    wl_surface *surface = astreaQtWaylandSurface(window);
    if (!surface)
        return false;

    auto effect = m_surfaces.value(surface);
    if (!effect) {
        effect = ext_background_effect_manager_v1_get_background_effect(m_manager, surface);
        if (!effect)
            return false;
        m_surfaces.insert(surface, effect);
    }

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
    wl_surface_commit(surface);
    if (wl_display_flush(m_display) < 0 && errno != EAGAIN)
        return false;
    return true;
#endif
}

void AstreaWaylandEffects::clear(QWindow *window)
{
#if ASTREA_HAVE_WAYLAND_EFFECTS
    if (!m_display || !window)
        return;
    wl_surface *surface = astreaQtWaylandSurface(window);
    if (!surface)
        return;
    if (auto *effect = m_surfaces.take(surface)) {
        ext_background_effect_surface_v1_destroy(effect);
        wl_surface_commit(surface);
        wl_display_flush(m_display);
    }
#else
    Q_UNUSED(window);
#endif
}

void AstreaWaylandEffects::setError(const QString &error)
{
    m_error = error;
}
