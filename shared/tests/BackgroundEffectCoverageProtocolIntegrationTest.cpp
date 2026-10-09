#include "platform/typhon/TyphonShellAuthenticator.hpp"
#include "platform/wayland/effects/AstreaAnalyticBackdropCoverage.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QRect>
#include <QSocketNotifier>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include "astrea-background-effect-coverage-v1-client-protocol.h"
#include "astrea-background-effect-coverage-v1-server-protocol.h"
#include "astrea-shell-auth-v1-server-protocol.h"
#include "ext-background-effect-v1-client-protocol.h"
#include "ext-background-effect-v1-server-protocol.h"

class FakeBackgroundEffectCompositor final : public QObject {
    Q_OBJECT

public:
    FakeBackgroundEffectCompositor(const bool advertiseCoverage,
                                   const bool acceptAuthentication,
                                   QObject *parent = nullptr)
        : QObject(parent)
        , m_advertiseCoverage(advertiseCoverage)
        , m_acceptAuthentication(acceptAuthentication)
    {
        QVERIFY(m_runtime.isValid());
        m_previousRuntime = qgetenv("XDG_RUNTIME_DIR");
        m_previousDisplay = qgetenv("WAYLAND_DISPLAY");
        m_previousCapability = qgetenv("ASTREA_SHELL_CAPABILITY_FILE");
        qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
        m_socketName = QStringLiteral("wayland-frosted-%1")
                           .arg(QCoreApplication::applicationPid());
        qputenv("WAYLAND_DISPLAY", m_socketName.toUtf8());
        m_capabilityPath = m_runtime.filePath(QStringLiteral("capability"));
        qputenv("ASTREA_SHELL_CAPABILITY_FILE", m_capabilityPath.toUtf8());
        QFile capability(m_capabilityPath);
        QVERIFY(capability.open(QIODevice::WriteOnly));
        QCOMPARE(capability.write(QByteArray(64, 'a') + '\n'), qint64(65));

        m_display = wl_display_create();
        QVERIFY(m_display);
        m_loop = wl_display_get_event_loop(m_display);
        QVERIFY(m_loop);
        QCOMPARE(wl_display_add_socket(m_display, m_socketName.toUtf8().constData()), 0);
        m_serverNotifier = new QSocketNotifier(wl_event_loop_get_fd(m_loop),
                                                QSocketNotifier::Read, this);
        connect(m_serverNotifier, &QSocketNotifier::activated, this,
                [this](QSocketDescriptor, QSocketNotifier::Type) { dispatchServerEvents(); });

        m_compositorGlobal = wl_global_create(m_display, &wl_compositor_interface, 1,
                                              this, &bindCompositor);
        m_authGlobal = wl_global_create(m_display,
                                        &astrea_shell_auth_manager_v1_interface, 1,
                                        this, &bindAuthManager);
        m_effectGlobal = wl_global_create(m_display,
                                          &ext_background_effect_manager_v1_interface, 1,
                                          this, &bindEffectManager);
        QVERIFY(m_compositorGlobal);
        QVERIFY(m_authGlobal);
        QVERIFY(m_effectGlobal);
        if (m_advertiseCoverage) {
            m_coverageGlobal = wl_global_create(
                m_display, &astrea_background_effect_coverage_manager_v1_interface,
                1, this, &bindCoverageManager);
            QVERIFY(m_coverageGlobal);
        }
    }

    ~FakeBackgroundEffectCompositor() override
    {
        if (m_clientNotifier)
            m_clientNotifier->setEnabled(false);
        delete m_clientNotifier;
        m_clientNotifier = nullptr;
        if (m_client) {
            wl_display_disconnect(m_client);
            m_client = nullptr;
        }
        if (m_serverNotifier)
            m_serverNotifier->setEnabled(false);
        delete m_serverNotifier;
        m_serverNotifier = nullptr;
        if (m_display) {
            wl_display_destroy_clients(m_display);
            wl_display_destroy(m_display);
            m_display = nullptr;
        }
        restoreEnvironment("XDG_RUNTIME_DIR", m_previousRuntime);
        restoreEnvironment("WAYLAND_DISPLAY", m_previousDisplay);
        restoreEnvironment("ASTREA_SHELL_CAPABILITY_FILE", m_previousCapability);
    }

    bool connectClient()
    {
        m_client = wl_display_connect(nullptr);
        if (!m_client)
            return false;
        m_clientNotifier = new QSocketNotifier(wl_display_get_fd(m_client),
                                                QSocketNotifier::Read, this);
        connect(m_clientNotifier, &QSocketNotifier::activated, this,
                [this](QSocketDescriptor, QSocketNotifier::Type) {
                    if (wl_display_dispatch(m_client) < 0)
                        m_clientDispatchFailed = true;
                });

        m_registry = wl_display_get_registry(m_client);
        static const wl_registry_listener listener{
            &registryGlobal,
            &registryGlobalRemove,
        };
        if (!m_registry || wl_registry_add_listener(m_registry, &listener, this) < 0
            || !roundtrip()) {
            return false;
        }
        return m_compositor && m_effectManager
            && (!m_advertiseCoverage || m_coverageManager);
    }

    bool authenticate()
    {
        return Astrea::Typhon::TyphonShellAuthenticator::authenticate(m_client);
    }

    bool roundtrip()
    {
        if (!m_client)
            return false;
        struct SyncState {
            bool done = false;
        } state;
        static const wl_callback_listener listener{
            [](void *data, wl_callback *callback, uint32_t) {
                static_cast<SyncState *>(data)->done = true;
                wl_callback_destroy(callback);
            },
        };
        wl_callback *callback = wl_display_sync(m_client);
        if (!callback || wl_callback_add_listener(callback, &listener, &state) != 0
            || wl_display_flush(m_client) < 0) {
            if (callback)
                wl_callback_destroy(callback);
            return false;
        }
        return pumpUntil([&state] { return state.done; });
    }

    bool pumpUntil(const std::function<bool()> &condition)
    {
        for (int turn = 0; turn < 500 && !condition(); ++turn)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        return condition();
    }

    bool issueEffectUpdate(const QVector<QRect> &region,
                           const std::optional<AstreaAnalyticBackdropCoverage> &coverage)
    {
        if (!m_client || !m_compositor || !m_effectManager)
            return false;
        wl_surface *surface = wl_compositor_create_surface(m_compositor);
        if (!surface)
            return false;
        auto *effect = ext_background_effect_manager_v1_get_background_effect(
            m_effectManager, surface);
        if (!effect)
            return false;

        if (coverage) {
            if (!m_coverageManager)
                return false;
            auto *analytic = astrea_background_effect_coverage_manager_v1_get_coverage(
                m_coverageManager, surface);
            if (!analytic)
                return false;
            const auto &rect = *coverage->roundedRect;
            astrea_background_effect_coverage_v1_set_rounded_rect(
                analytic, wl_fixed_from_double(rect.rect.x()),
                wl_fixed_from_double(rect.rect.y()), wl_fixed_from_double(rect.rect.width()),
                wl_fixed_from_double(rect.rect.height()), wl_fixed_from_double(rect.radius));
            const auto &triangle = *coverage->triangle;
            astrea_background_effect_coverage_v1_set_triangle(
                analytic, wl_fixed_from_double(triangle.a.x()),
                wl_fixed_from_double(triangle.a.y()), wl_fixed_from_double(triangle.b.x()),
                wl_fixed_from_double(triangle.b.y()), wl_fixed_from_double(triangle.c.x()),
                wl_fixed_from_double(triangle.c.y()));
        }

        wl_region *publicRegion = wl_compositor_create_region(m_compositor);
        if (!publicRegion)
            return false;
        for (const QRect &rectangle : region)
            wl_region_add(publicRegion, rectangle.x(), rectangle.y(),
                          rectangle.width(), rectangle.height());
        ext_background_effect_surface_v1_set_blur_region(effect, publicRegion);
        wl_region_destroy(publicRegion);
        wl_surface_commit(surface);
        if (wl_display_flush(m_client) < 0)
            return false;
        const bool synchronized = roundtrip();
        wl_surface_destroy(surface);
        return synchronized;
    }

    wl_client *authenticatedClient() const { return m_authenticatedClient; }
    wl_client *surfaceClient() const { return m_surfaceClient; }
    wl_client *effectClient() const { return m_effectClient; }
    wl_client *coverageClient() const { return m_coverageClient; }
    wl_display *clientDisplayForTest() const { return m_client; }
    wl_resource *effectSurface() const { return m_effectSurface; }
    wl_resource *coverageSurface() const { return m_coverageSurface; }
    const QVector<QRect> &publicRegion() const { return m_publicRegion; }
    const QStringList &requestTrace() const { return m_requestTrace; }
    int authenticationRequests() const { return m_authenticationRequests; }
    int coverageRequests() const { return m_coverageRequests; }
    bool clientDispatchFailed() const { return m_clientDispatchFailed; }
    bool privateRequestRejected() const { return m_privateRequestRejected; }
    bool sameClientAndSurfaceTargets() const
    {
        return m_authenticatedClient && m_authenticatedClient == m_surfaceClient
            && m_surfaceClient == m_effectClient && m_effectClient == m_coverageClient
            && m_effectSurface == m_coverageSurface;
    }

private:
    struct RegionData {
        FakeBackgroundEffectCompositor *owner = nullptr;
        QVector<QRect> rectangles;
    };

    static void restoreEnvironment(const char *name, const QByteArray &value)
    {
        if (value.isNull())
            qunsetenv(name);
        else
            qputenv(name, value);
    }

    static void registryGlobal(void *data, wl_registry *registry, uint32_t name,
                               const char *interface, uint32_t version)
    {
        auto *self = static_cast<FakeBackgroundEffectCompositor *>(data);
        if (std::strcmp(interface, "wl_compositor") == 0) {
            self->m_compositor = static_cast<wl_compositor *>(wl_registry_bind(
                registry, name, &wl_compositor_interface, std::min(version, 1u)));
        } else if (std::strcmp(interface, "ext_background_effect_manager_v1") == 0) {
            self->m_effectManager = static_cast<ext_background_effect_manager_v1 *>(
                wl_registry_bind(registry, name,
                                 &ext_background_effect_manager_v1_interface,
                                 std::min(version, 1u)));
            static const ext_background_effect_manager_v1_listener effectListener{
                &managerCapabilities,
            };
            if (self->m_effectManager)
                ext_background_effect_manager_v1_add_listener(
                    self->m_effectManager, &effectListener, self);
        } else if (std::strcmp(interface,
                               "astrea_background_effect_coverage_manager_v1") == 0) {
            self->m_coverageManager = static_cast<
                astrea_background_effect_coverage_manager_v1 *>(wl_registry_bind(
                registry, name,
                &astrea_background_effect_coverage_manager_v1_interface,
                std::min(version, 1u)));
        }
    }

    static void registryGlobalRemove(void *, wl_registry *, uint32_t) {}

    static void managerCapabilities(void *, ext_background_effect_manager_v1 *, uint32_t flags)
    {
        Q_UNUSED(flags);
    }

    static void bindCompositor(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *self = static_cast<FakeBackgroundEffectCompositor *>(data);
        wl_resource *resource = wl_resource_create(client, &wl_compositor_interface,
                                                  std::min(version, 1u), id);
        if (resource)
            wl_resource_set_implementation(resource, &kCompositorImplementation,
                                           self, nullptr);
    }

    static void createSurface(wl_client *client, wl_resource *resource, uint32_t id)
    {
        auto *self = static_cast<FakeBackgroundEffectCompositor *>(
            wl_resource_get_user_data(resource));
        wl_resource *surface = wl_resource_create(client, &wl_surface_interface, 1, id);
        if (!surface)
            return;
        self->m_surfaceClient = client;
        self->m_surface = surface;
        wl_resource_set_implementation(surface, &kSurfaceImplementation, self, nullptr);
    }

    static void createRegion(wl_client *client, wl_resource *resource, uint32_t id)
    {
        auto *self = static_cast<FakeBackgroundEffectCompositor *>(
            wl_resource_get_user_data(resource));
        auto *data = new RegionData{self, {}};
        wl_resource *region = wl_resource_create(client, &wl_region_interface, 1, id);
        if (!region) {
            delete data;
            return;
        }
        wl_resource_set_implementation(region, &kRegionImplementation, data,
                                       &destroyRegionResource);
    }

    static void destroySurface(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    static void commitSurface(wl_client *, wl_resource *resource)
    {
        auto *self = static_cast<FakeBackgroundEffectCompositor *>(
            wl_resource_get_user_data(resource));
        self->m_requestTrace.append(QStringLiteral("commit"));
    }

    static void destroyRegionResource(wl_resource *resource)
    {
        delete static_cast<RegionData *>(wl_resource_get_user_data(resource));
    }

    static void destroyRegion(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    static void addRegion(wl_client *, wl_resource *resource, int32_t x, int32_t y,
                          int32_t width, int32_t height)
    {
        auto *data = static_cast<RegionData *>(wl_resource_get_user_data(resource));
        if (data)
            data->rectangles.append(QRect(x, y, width, height));
    }

    static void bindAuthManager(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *self = static_cast<FakeBackgroundEffectCompositor *>(data);
        wl_resource *resource = wl_resource_create(
            client, &astrea_shell_auth_manager_v1_interface, std::min(version, 1u), id);
        if (resource)
            wl_resource_set_implementation(resource, &kAuthImplementation, self, nullptr);
    }

    static void authenticateRequest(wl_client *client, wl_resource *resource, const char *)
    {
        auto *self = static_cast<FakeBackgroundEffectCompositor *>(
            wl_resource_get_user_data(resource));
        ++self->m_authenticationRequests;
        self->m_requestTrace.append(QStringLiteral("authenticate"));
        if (self->m_acceptAuthentication) {
            self->m_authenticatedClient = client;
            astrea_shell_auth_manager_v1_send_authenticated(resource);
        } else {
            astrea_shell_auth_manager_v1_send_rejected(resource);
        }
    }

    static void destroyAuthManager(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    static void bindEffectManager(wl_client *client, void *data, uint32_t version,
                                  uint32_t id)
    {
        auto *self = static_cast<FakeBackgroundEffectCompositor *>(data);
        wl_resource *resource = wl_resource_create(
            client, &ext_background_effect_manager_v1_interface, std::min(version, 1u), id);
        if (!resource)
            return;
        wl_resource_set_implementation(resource, &kEffectManagerImplementation, self, nullptr);
        ext_background_effect_manager_v1_send_capabilities(resource, 1);
    }

    static void destroyEffectManager(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    static void getBackgroundEffect(wl_client *client, wl_resource *, uint32_t id,
                                    wl_resource *surface)
    {
        wl_resource *effect = wl_resource_create(
            client, &ext_background_effect_surface_v1_interface, 1, id);
        if (!effect)
            return;
        // The compositor has one client and one test surface per scenario.
        // The surface and manager are both owned by this fake compositor.
        auto *owner = static_cast<FakeBackgroundEffectCompositor *>(
            wl_resource_get_user_data(surface));
        if (!owner)
            return;
        owner->m_effectSurface = surface;
        owner->m_effectClient = client;
        wl_resource_set_implementation(effect, &kEffectImplementation, owner, nullptr);
    }

    static void setBlurRegion(wl_client *client, wl_resource *resource,
                              wl_resource *region)
    {
        auto *self = static_cast<FakeBackgroundEffectCompositor *>(
            wl_resource_get_user_data(resource));
        if (!self || client != self->m_effectClient
            || self->m_effectSurface != self->m_surface) {
            return;
        }
        self->m_requestTrace.append(QStringLiteral("public-region"));
        auto *data = region
            ? static_cast<RegionData *>(wl_resource_get_user_data(region)) : nullptr;
        self->m_publicRegion = data ? data->rectangles : QVector<QRect>{};
    }

    static void destroyEffect(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    static void bindCoverageManager(wl_client *client, void *data, uint32_t version,
                                    uint32_t id)
    {
        auto *self = static_cast<FakeBackgroundEffectCompositor *>(data);
        wl_resource *resource = wl_resource_create(
            client, &astrea_background_effect_coverage_manager_v1_interface,
            std::min(version, 1u), id);
        if (resource)
            wl_resource_set_implementation(resource, &kCoverageManagerImplementation,
                                           self, nullptr);
    }

    static void getCoverage(wl_client *client, wl_resource *manager, uint32_t id,
                            wl_resource *surface)
    {
        auto *self = static_cast<FakeBackgroundEffectCompositor *>(
            wl_resource_get_user_data(manager));
        ++self->m_coverageRequests;
        self->m_requestTrace.append(QStringLiteral("get-coverage"));
        if (!self->m_authenticatedClient || client != self->m_authenticatedClient
            || wl_resource_get_client(surface) != client || surface != self->m_surface) {
            self->m_privateRequestRejected = true;
            return;
        }
        self->m_coverageSurface = surface;
        self->m_coverageClient = client;
        wl_resource *coverage = wl_resource_create(
            client, &astrea_background_effect_coverage_v1_interface, 1, id);
        if (coverage)
            wl_resource_set_implementation(coverage, &kCoverageImplementation, self, nullptr);
    }

    static void destroyCoverageManager(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    static void setRoundedRect(wl_client *client, wl_resource *resource, wl_fixed_t,
                               wl_fixed_t, wl_fixed_t, wl_fixed_t, wl_fixed_t)
    {
        auto *self = static_cast<FakeBackgroundEffectCompositor *>(
            wl_resource_get_user_data(resource));
        if (client == self->m_coverageClient
            && self->m_coverageSurface == self->m_effectSurface)
            self->m_requestTrace.append(QStringLiteral("rounded-rect"));
    }

    static void unsetRoundedRect(wl_client *, wl_resource *) {}

    static void setTriangle(wl_client *client, wl_resource *resource, wl_fixed_t, wl_fixed_t,
                            wl_fixed_t, wl_fixed_t, wl_fixed_t, wl_fixed_t)
    {
        auto *self = static_cast<FakeBackgroundEffectCompositor *>(
            wl_resource_get_user_data(resource));
        if (client == self->m_coverageClient
            && self->m_coverageSurface == self->m_effectSurface)
            self->m_requestTrace.append(QStringLiteral("triangle"));
    }

    static void unsetTriangle(wl_client *, wl_resource *) {}
    static void clearCoverage(wl_client *, wl_resource *) {}
    static void destroyCoverage(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    void dispatchServerEvents()
    {
        if (!m_loop || !m_display)
            return;
        wl_event_loop_dispatch(m_loop, 0);
        wl_display_flush_clients(m_display);
    }

    static const struct wl_compositor_interface kCompositorImplementation;
    static const struct wl_surface_interface kSurfaceImplementation;
    static const struct wl_region_interface kRegionImplementation;
    static const struct astrea_shell_auth_manager_v1_interface kAuthImplementation;
    static const struct ext_background_effect_manager_v1_interface kEffectManagerImplementation;
    static const struct ext_background_effect_surface_v1_interface kEffectImplementation;
    static const struct astrea_background_effect_coverage_manager_v1_interface
        kCoverageManagerImplementation;
    static const struct astrea_background_effect_coverage_v1_interface kCoverageImplementation;

    QTemporaryDir m_runtime;
    QString m_socketName;
    QString m_capabilityPath;
    QByteArray m_previousRuntime;
    QByteArray m_previousDisplay;
    QByteArray m_previousCapability;
    wl_display *m_display = nullptr;
    wl_event_loop *m_loop = nullptr;
    wl_global *m_compositorGlobal = nullptr;
    wl_global *m_authGlobal = nullptr;
    wl_global *m_effectGlobal = nullptr;
    wl_global *m_coverageGlobal = nullptr;
    wl_client *m_authenticatedClient = nullptr;
    wl_client *m_surfaceClient = nullptr;
    wl_client *m_effectClient = nullptr;
    wl_client *m_coverageClient = nullptr;
    wl_resource *m_surface = nullptr;
    wl_resource *m_effectSurface = nullptr;
    wl_resource *m_coverageSurface = nullptr;
    wl_display *m_client = nullptr;
    wl_registry *m_registry = nullptr;
    wl_compositor *m_compositor = nullptr;
    ext_background_effect_manager_v1 *m_effectManager = nullptr;
    astrea_background_effect_coverage_manager_v1 *m_coverageManager = nullptr;
    QSocketNotifier *m_serverNotifier = nullptr;
    QSocketNotifier *m_clientNotifier = nullptr;
    QVector<QRect> m_publicRegion;
    QStringList m_requestTrace;
    int m_authenticationRequests = 0;
    int m_coverageRequests = 0;
    bool m_advertiseCoverage = false;
    bool m_acceptAuthentication = false;
    bool m_clientDispatchFailed = false;
    bool m_privateRequestRejected = false;
};

const struct wl_compositor_interface
    FakeBackgroundEffectCompositor::kCompositorImplementation = {
        &FakeBackgroundEffectCompositor::createSurface,
        &FakeBackgroundEffectCompositor::createRegion,
};
const struct wl_surface_interface FakeBackgroundEffectCompositor::kSurfaceImplementation = {
    &FakeBackgroundEffectCompositor::destroySurface,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &FakeBackgroundEffectCompositor::commitSurface,
};
const struct wl_region_interface FakeBackgroundEffectCompositor::kRegionImplementation = {
    &FakeBackgroundEffectCompositor::destroyRegion,
    &FakeBackgroundEffectCompositor::addRegion,
    nullptr,
};
const struct astrea_shell_auth_manager_v1_interface
    FakeBackgroundEffectCompositor::kAuthImplementation = {
        &FakeBackgroundEffectCompositor::authenticateRequest,
        &FakeBackgroundEffectCompositor::destroyAuthManager,
};
const struct ext_background_effect_manager_v1_interface
    FakeBackgroundEffectCompositor::kEffectManagerImplementation = {
        &FakeBackgroundEffectCompositor::destroyEffectManager,
        &FakeBackgroundEffectCompositor::getBackgroundEffect,
};
const struct ext_background_effect_surface_v1_interface
    FakeBackgroundEffectCompositor::kEffectImplementation = {
        &FakeBackgroundEffectCompositor::destroyEffect,
        &FakeBackgroundEffectCompositor::setBlurRegion,
};
const struct astrea_background_effect_coverage_manager_v1_interface
    FakeBackgroundEffectCompositor::kCoverageManagerImplementation = {
        &FakeBackgroundEffectCompositor::getCoverage,
        &FakeBackgroundEffectCompositor::destroyCoverageManager,
};
const struct astrea_background_effect_coverage_v1_interface
    FakeBackgroundEffectCompositor::kCoverageImplementation = {
        &FakeBackgroundEffectCompositor::setRoundedRect,
        &FakeBackgroundEffectCompositor::unsetRoundedRect,
        &FakeBackgroundEffectCompositor::setTriangle,
        &FakeBackgroundEffectCompositor::unsetTriangle,
        &FakeBackgroundEffectCompositor::clearCoverage,
        &FakeBackgroundEffectCompositor::destroyCoverage,
};
class BackgroundEffectCoverageProtocolIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void authenticatedCoverageAndPublicRegionTargetOneSurfaceBeforeCommit()
    {
        FakeBackgroundEffectCompositor compositor(true, true);
        QVERIFY(compositor.connectClient());
        QVERIFY(compositor.authenticate());
        const AstreaAnalyticBackdropCoverage shape{
            AstreaAnalyticRoundedRect{QRectF(12.0, 18.0, 72.0, 34.0), 17.0},
            AstreaAnalyticTriangle{QPointF(41.0, 52.0), QPointF(55.0, 52.0),
                                   QPointF(48.0, 60.0)}};
        QVERIFY(compositor.issueEffectUpdate({QRect(10, 16, 76, 46)}, shape));

        QVERIFY(compositor.sameClientAndSurfaceTargets());
        QCOMPARE(compositor.coverageRequests(), 1);
        const QVector<QRect> expectedRegion{{10, 16, 76, 46}};
        QCOMPARE(compositor.publicRegion(), expectedRegion);
        const auto trace = compositor.requestTrace();
        const int rounded = trace.indexOf(QStringLiteral("rounded-rect"));
        const int triangle = trace.indexOf(QStringLiteral("triangle"));
        const int publicRegion = trace.indexOf(QStringLiteral("public-region"));
        const int commit = trace.indexOf(QStringLiteral("commit"));
        QVERIFY(rounded >= 0);
        QVERIFY(triangle > rounded);
        QVERIFY(publicRegion > triangle);
        QVERIFY(commit > publicRegion);
        QVERIFY(!compositor.clientDispatchFailed());
        QVERIFY(!compositor.privateRequestRejected());
    }

    void missingPrivateGlobalKeepsPublicFallbackAvailable()
    {
        FakeBackgroundEffectCompositor compositor(false, true);
        QVERIFY(compositor.connectClient());
        QVector<QRect> fallback;
        for (int row = 0; row < 41; ++row)
            fallback.append(QRect(20, 30 + row, 72, 1));
        QVERIFY(compositor.issueEffectUpdate(fallback, std::nullopt));

        QCOMPARE(compositor.coverageRequests(), 0);
        QCOMPARE(compositor.authenticationRequests(), 0);
        QCOMPARE(compositor.publicRegion(), fallback);
        QVERIFY(compositor.requestTrace().contains(QStringLiteral("public-region")));
        QVERIFY(compositor.requestTrace().contains(QStringLiteral("commit")));
        QVERIFY(!compositor.clientDispatchFailed());
    }

    void rejectedAuthenticationNeverRequestsPrivateCoverage()
    {
        FakeBackgroundEffectCompositor compositor(true, false);
        QVERIFY(compositor.connectClient());
        QVERIFY(!compositor.authenticate());
        QVector<QRect> fallback;
        for (int row = 0; row < 42; ++row)
            fallback.append(QRect(20, 30 + row, 72, 1));
        QVERIFY(compositor.issueEffectUpdate(fallback, std::nullopt));

        QCOMPARE(compositor.authenticationRequests(), 1);
        QCOMPARE(compositor.coverageRequests(), 0);
        QCOMPARE(compositor.publicRegion(), fallback);
        QVERIFY(compositor.requestTrace().contains(QStringLiteral("public-region")));
        QCOMPARE(wl_display_get_error(compositor.clientDisplayForTest()), 0);
        QVERIFY(!compositor.clientDispatchFailed());
    }
};

QTEST_GUILESS_MAIN(BackgroundEffectCoverageProtocolIntegrationTest)
#include "BackgroundEffectCoverageProtocolIntegrationTest.moc"
