#include "platform/wayland/effects/AstreaWaylandEffects.hpp"

#include "platform/wayland/effects/QtWaylandWindow.hpp"

#if ASTREA_HAVE_ANALYTIC_COVERAGE
#include "platform/typhon/TyphonShellAuthenticator.hpp"
#endif

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQuickWindow>

#include <algorithm>
#include <cerrno>
#include <cstring>

#if ASTREA_HAVE_WAYLAND_EFFECTS
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include "ext-background-effect-v1-client-protocol.h"
#endif

#if ASTREA_HAVE_ANALYTIC_COVERAGE
#include "astrea-background-effect-coverage-v1-client-protocol.h"
#endif

Q_LOGGING_CATEGORY(lcAstreaWaylandEffects, "astrea.wayland.effects")

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
#if ASTREA_HAVE_ANALYTIC_COVERAGE
    if (m_coverageManager)
        astrea_background_effect_coverage_manager_v1_destroy(m_coverageManager);
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
    ++m_displayGeneration;

    struct Probe {
        ext_background_effect_manager_v1 *manager = nullptr;
        astrea_background_effect_coverage_manager_v1 *coverageManager = nullptr;
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
#if ASTREA_HAVE_ANALYTIC_COVERAGE
            if (std::strcmp(interface,
                            "astrea_background_effect_coverage_manager_v1") == 0) {
                probe->coverageManager = static_cast<
                    astrea_background_effect_coverage_manager_v1 *>(wl_registry_bind(
                    registry, name,
                    &astrea_background_effect_coverage_manager_v1_interface,
                    std::min(version, 1u)));
            }
#endif
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
    m_coverageManager = probe.coverageManager;

    m_capabilityState.setManagerBound(m_manager != nullptr);
    updateAvailability();
    if (m_manager && wl_display_roundtrip(m_display) < 0) {
        setError(QStringLiteral("Wayland capability synchronization failed"));
        return false;
    }
    updateAvailability();
    if (!m_manager)
        setError(QStringLiteral("Compositor does not advertise ext_background_effect_v1"));
    else if (!m_available)
        setError(QStringLiteral("Compositor does not advertise blur capability"));

#if ASTREA_HAVE_ANALYTIC_COVERAGE
    if (m_coverageManager) {
        QString diagnostic;
        m_authenticationAttempted = true;
        m_authenticated = Astrea::Typhon::TyphonShellAuthenticator::authenticate(
            m_display, &diagnostic);
        if (m_authenticated)
            m_authenticatedDisplayGeneration = m_displayGeneration;
        else {
            static bool diagnosticLogged = false;
            if (!diagnosticLogged) {
                qWarning().noquote() << "Typhon analytic background coverage disabled:"
                                     << diagnostic;
                diagnosticLogged = true;
            }
        }
    } else {
        qCDebug(lcAstreaWaylandEffects)
            << "Typhon analytic coverage global is absent; retaining public fallback";
    }
#endif
    updateAnalyticCoverageAvailability();
    return m_available;
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

void AstreaWaylandEffects::updateAnalyticCoverageAvailability()
{
    const bool available = AstreaWaylandEffectsCapabilityState::analyticCoverageAvailable(
        m_coverageManager != nullptr, m_authenticationAttempted, m_authenticated,
        m_displayGeneration, m_authenticatedDisplayGeneration);
    if (m_analyticCoverageAvailable == available)
        return;
    m_analyticCoverageAvailable = available;
    emit analyticCoverageAvailableChanged();
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

bool AstreaWaylandEffects::createCoverage(
    wl_surface *surface, astrea_background_effect_coverage_v1 **coverage)
{
    if (coverage)
        *coverage = nullptr;
    if (!coverage || !surface || !initialize() || !analyticCoverageAvailable()
        || !m_coverageManager
        || m_authenticatedDisplayGeneration != m_displayGeneration) {
        return false;
    }

#if !ASTREA_HAVE_ANALYTIC_COVERAGE
    Q_UNUSED(surface);
    return false;
#else
    *coverage = astrea_background_effect_coverage_manager_v1_get_coverage(
        m_coverageManager, surface);
    return *coverage != nullptr;
#endif
}

wl_region *AstreaWaylandEffects::createRegion(const QVector<QRect> &rectangles)
{
#if !ASTREA_HAVE_WAYLAND_EFFECTS
    Q_UNUSED(rectangles);
    return nullptr;
#else
    if (!m_compositor || rectangles.isEmpty())
        return nullptr;
    wl_region *region = wl_compositor_create_region(m_compositor);
    if (!region)
        return nullptr;
    for (const QRect &rectangle : rectangles) {
        if (rectangle.width() > 0 && rectangle.height() > 0)
            wl_region_add(region, rectangle.x(), rectangle.y(), rectangle.width(),
                          rectangle.height());
    }
    return region;
#endif
}

void AstreaWaylandEffects::submitState(
    QQuickWindow *window, ext_background_effect_surface_v1 *effect,
    astrea_background_effect_coverage_v1 *coverage, wl_region *region,
    const bool updatePublicRegion,
    const std::optional<AstreaAnalyticBackdropCoverage> &analyticCoverage,
    const bool clearCoverage, const bool removePublicEffect, const bool requestFrame)
{
#if ASTREA_HAVE_ANALYTIC_COVERAGE
    if (coverage && analyticCoverage) {
        const auto &shape = *analyticCoverage;
        if (shape.roundedRect) {
            const auto &roundedRect = *shape.roundedRect;
            astrea_background_effect_coverage_v1_set_rounded_rect(
                coverage, wl_fixed_from_double(roundedRect.rect.x()),
                wl_fixed_from_double(roundedRect.rect.y()),
                wl_fixed_from_double(roundedRect.rect.width()),
                wl_fixed_from_double(roundedRect.rect.height()),
                wl_fixed_from_double(roundedRect.radius));
        } else {
            astrea_background_effect_coverage_v1_unset_rounded_rect(coverage);
        }
        if (shape.triangle) {
            const auto &triangle = *shape.triangle;
            astrea_background_effect_coverage_v1_set_triangle(
                coverage, wl_fixed_from_double(triangle.a.x()),
                wl_fixed_from_double(triangle.a.y()), wl_fixed_from_double(triangle.b.x()),
                wl_fixed_from_double(triangle.b.y()), wl_fixed_from_double(triangle.c.x()),
                wl_fixed_from_double(triangle.c.y()));
        } else {
            astrea_background_effect_coverage_v1_unset_triangle(coverage);
        }
    } else if (coverage && clearCoverage) {
        astrea_background_effect_coverage_v1_clear(coverage);
    }
#else
    Q_UNUSED(coverage);
    Q_UNUSED(analyticCoverage);
    Q_UNUSED(clearCoverage);
#endif

#if ASTREA_HAVE_WAYLAND_EFFECTS
    if (updatePublicRegion && effect)
        ext_background_effect_surface_v1_set_blur_region(effect, region);
    if (region)
        wl_region_destroy(region);
    if (removePublicEffect && effect)
        ext_background_effect_surface_v1_destroy(effect);
    if (requestFrame)
        requestQtFrame(window);
    if (m_display && wl_display_flush(m_display) < 0 && errno != EAGAIN) {
        static bool diagnosticLogged = false;
        if (!diagnosticLogged) {
            qWarning() << "Wayland background-effect state flush failed" << errno;
            diagnosticLogged = true;
        }
    }
#else
    Q_UNUSED(window);
    Q_UNUSED(effect);
    Q_UNUSED(region);
    Q_UNUSED(updatePublicRegion);
    Q_UNUSED(removePublicEffect);
    Q_UNUSED(requestFrame);
#endif
}

void AstreaWaylandEffects::destroySurfaceObjects(
    ext_background_effect_surface_v1 *effect,
    astrea_background_effect_coverage_v1 *coverage)
{
#if ASTREA_HAVE_ANALYTIC_COVERAGE
    if (coverage)
        astrea_background_effect_coverage_v1_destroy(coverage);
#else
    Q_UNUSED(coverage);
#endif
#if ASTREA_HAVE_WAYLAND_EFFECTS
    if (effect)
        ext_background_effect_surface_v1_destroy(effect);
    if (m_display && wl_display_flush(m_display) < 0 && errno != EAGAIN) {
        static bool diagnosticLogged = false;
        if (!diagnosticLogged) {
            qWarning() << "Wayland background-effect proxy release failed" << errno;
            diagnosticLogged = true;
        }
    }
#else
    Q_UNUSED(effect);
#endif
}

bool AstreaWaylandEffects::setBlurRegion(
    QQuickWindow *window, ext_background_effect_surface_v1 *effect,
    const QVector<QRect> &rectangles, const bool requestFrame)
{
    if (!window || !effect || !initialize() || !m_available)
        return false;

#if !ASTREA_HAVE_WAYLAND_EFFECTS
    Q_UNUSED(window);
    Q_UNUSED(effect);
    Q_UNUSED(rectangles);
    return false;
#else

    wl_region *region = rectangles.isEmpty() ? nullptr : createRegion(rectangles);
    if (!rectangles.isEmpty() && !region)
        return false;
    ext_background_effect_surface_v1_set_blur_region(effect, region);
    if (region)
        wl_region_destroy(region);
    if (requestFrame)
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
