#include "platform/typhon/TyphonShortcutClient.hpp"

#include "platform/typhon/TyphonSharedConnection.hpp"
#include "platform/typhon/TyphonShellAuthenticator.hpp"
#include "platform/typhon/TyphonWaylandDisplay.hpp"

#include <QDebug>
#include <QTimer>
#include <QVector>

#include <algorithm>

#if ASTREA_HAVE_TYPHON_PROTOCOL
#include <wayland-client.h>

extern "C" {
struct astrea_shortcuts_manager_v1;
struct astrea_shortcut_v1;

extern const struct wl_interface astrea_shortcuts_manager_v1_interface;
extern const struct wl_interface astrea_shortcut_v1_interface;

struct astrea_shortcut_v1_listener {
    void (*pressed)(void *, struct astrea_shortcut_v1 *, std::uint32_t, std::uint32_t);
    void (*repeated)(void *, struct astrea_shortcut_v1 *, std::uint32_t, std::uint32_t);
    void (*released)(void *, struct astrea_shortcut_v1 *, std::uint32_t, std::uint32_t);
    void (*cancelled)(void *, struct astrea_shortcut_v1 *, std::uint32_t);
};
}

namespace {

constexpr std::uint32_t kManagerDestroyOpcode = 0;
constexpr std::uint32_t kManagerRegisterShortcutOpcode = 1;
constexpr std::uint32_t kShortcutDestroyOpcode = 0;

int shortcutAddListener(astrea_shortcut_v1 *shortcut,
                        const astrea_shortcut_v1_listener *listener, void *data)
{
    return wl_proxy_add_listener(reinterpret_cast<wl_proxy *>(shortcut),
                                 reinterpret_cast<void (**)(void)>(
                                     const_cast<astrea_shortcut_v1_listener *>(listener)),
                                 data);
}

void shortcutDestroy(astrea_shortcut_v1 *shortcut)
{
    wl_proxy_marshal_flags(reinterpret_cast<wl_proxy *>(shortcut), kShortcutDestroyOpcode,
                           nullptr, wl_proxy_get_version(reinterpret_cast<wl_proxy *>(shortcut)),
                           WL_MARSHAL_FLAG_DESTROY);
}

astrea_shortcut_v1 *registerShortcut(astrea_shortcuts_manager_v1 *manager,
                                     const char *namespaceName, const char *name,
                                     const char *description)
{
    return reinterpret_cast<astrea_shortcut_v1 *>(wl_proxy_marshal_flags(
        reinterpret_cast<wl_proxy *>(manager), kManagerRegisterShortcutOpcode,
        &astrea_shortcut_v1_interface,
        wl_proxy_get_version(reinterpret_cast<wl_proxy *>(manager)), 0, nullptr, namespaceName,
        name, description));
}

void managerDestroy(astrea_shortcuts_manager_v1 *manager)
{
    wl_proxy_marshal_flags(reinterpret_cast<wl_proxy *>(manager), kManagerDestroyOpcode, nullptr,
                           wl_proxy_get_version(reinterpret_cast<wl_proxy *>(manager)),
                           WL_MARSHAL_FLAG_DESTROY);
}

} // namespace
#endif

#if ASTREA_HAVE_TYPHON_PROTOCOL
struct TyphonShortcutRegistration;
#endif

struct TyphonShortcutClientPrivate {
    explicit TyphonShortcutClientPrivate(TyphonShortcutClient *owner,
                                         TyphonSharedConnection *sharedConnection)
        : owner(owner), sharedConnection(sharedConnection)
    {
        if (sharedConnection)
            display = sharedConnection->waylandDisplay();
        else {
            ownedDisplay = std::make_unique<TyphonWaylandDisplay>();
            display = ownedDisplay.get();
        }
    }

    TyphonShortcutClient *owner = nullptr;
    TyphonSharedConnection *sharedConnection = nullptr;
    std::unique_ptr<TyphonWaylandDisplay> ownedDisplay;
    TyphonWaylandDisplay *display = nullptr;
    QTimer reconnectTimer;
    TyphonShortcutConnectionState state = TyphonShortcutConnectionState::Stopped;
    std::uint64_t generation = 0;
    int backoffIndex = 0;
    bool started = false;
    bool failureInProgress = false;

#if ASTREA_HAVE_TYPHON_PROTOCOL
    wl_registry *registry = nullptr;
    wl_callback *registrySync = nullptr;
    astrea_shortcuts_manager_v1 *manager = nullptr;
    bool registryReady = false;
    QVector<TyphonShortcutRegistration *> registrations;

    void destroyProtocolObjects(bool sendDestroyRequests = true);
    void registerShortcuts();
    void handleEvent(TyphonShortcutRegistration *registration, TyphonShortcutPhase phase,
                     std::uint32_t serial, std::uint32_t timestamp);

    static void registryGlobal(void *data, wl_registry *registry, std::uint32_t name,
                               const char *interfaceName, std::uint32_t version);
    static void registryGlobalRemove(void *, wl_registry *, std::uint32_t);
    static void registrySynchronized(void *data, wl_callback *callback, std::uint32_t);
    static void shortcutPressed(void *data, astrea_shortcut_v1 *, std::uint32_t serial,
                                std::uint32_t timestamp);
    static void shortcutRepeated(void *data, astrea_shortcut_v1 *, std::uint32_t serial,
                                 std::uint32_t timestamp);
    static void shortcutReleased(void *data, astrea_shortcut_v1 *, std::uint32_t serial,
                                 std::uint32_t timestamp);
    static void shortcutCancelled(void *data, astrea_shortcut_v1 *, std::uint32_t serial);

    static const wl_registry_listener registryListener;
    static const wl_callback_listener registrySyncListener;
    static const astrea_shortcut_v1_listener shortcutListener;
#endif
};

#if ASTREA_HAVE_TYPHON_PROTOCOL
struct TyphonShortcutRegistration {
    TyphonShortcutClientPrivate *privateState = nullptr;
    std::uint64_t generation = 0;
    astrea_shortcut_v1 *proxy = nullptr;
    QString namespaceName;
    QString name;
    bool live = false;
};

namespace {

struct ShortcutSpec {
    const char *name;
    const char *description;
};

constexpr ShortcutSpec kShortcutSpecs[] = {
    {"alt_tab_next", "Advance the native Alt+Tab selection"},
    {"alt_tab_previous", "Reverse the native Alt+Tab selection"},
    {"alt_tab_commit", "Commit the native Alt+Tab selection"},
    {"spotlight_toggle", "Toggle the Astrea Spotlight shell surface"},
};

} // namespace

void TyphonShortcutClientPrivate::registerShortcuts()
{
    if (!started || !registryReady || !manager)
        return;

    owner->setState(TyphonShortcutConnectionState::Registering);
    for (const ShortcutSpec &spec : kShortcutSpecs) {
        auto *registration = new TyphonShortcutRegistration;
        registration->privateState = this;
        registration->generation = generation;
        registration->namespaceName = QStringLiteral("astrea-shell");
        registration->name = QString::fromLatin1(spec.name);
        const QByteArray namespaceName = registration->namespaceName.toUtf8();
        registration->proxy = registerShortcut(manager, namespaceName.constData(), spec.name,
                                               spec.description);
        if (!registration->proxy
            || shortcutAddListener(registration->proxy, &shortcutListener, registration) != 0) {
            if (registration->proxy)
                shortcutDestroy(registration->proxy);
            delete registration;
            owner->enterFailure(QStringLiteral("Typhon shortcut registration failed"), true);
            return;
        }
        registration->live = true;
        registrations.append(registration);
    }
    if (!owner->m_private->display->flush())
        owner->enterFailure(QStringLiteral("Typhon shortcut registration flush failed"), true);
    else
        owner->setState(TyphonShortcutConnectionState::Ready);
}

void TyphonShortcutClientPrivate::handleEvent(TyphonShortcutRegistration *registration,
                                              TyphonShortcutPhase phase,
                                              std::uint32_t serial,
                                              std::uint32_t timestamp)
{
    if (!registration || registration->privateState != this || !registration->live
        || registration->generation != generation || !started)
        return;

    if (phase == TyphonShortcutPhase::Cancelled) {
        registration->live = false;
        if (registration->proxy) {
            shortcutDestroy(registration->proxy);
            registration->proxy = nullptr;
        }
        owner->diagnostic(QStringLiteral("Typhon shortcut registration cancelled: %1")
                              .arg(registration->name));
        owner->shortcutEvent(registration->namespaceName, registration->name, phase, serial,
                             timestamp);
        if (owner->registeredShortcutCount() == 0)
            owner->setState(TyphonShortcutConnectionState::Degraded);
        return;
    }

    owner->shortcutEvent(registration->namespaceName, registration->name, phase, serial,
                         timestamp);
}

void TyphonShortcutClientPrivate::registryGlobal(void *data, wl_registry *registry,
                                                 std::uint32_t name, const char *interfaceName,
                                                 std::uint32_t version)
{
    auto *self = static_cast<TyphonShortcutClientPrivate *>(data);
    if (!self || !self->started || self->registry != registry || self->manager
        || qstrcmp(interfaceName, "astrea_shortcuts_manager_v1") != 0 || version < 1)
        return;

    self->manager = static_cast<astrea_shortcuts_manager_v1 *>(
        wl_registry_bind(registry, name, &astrea_shortcuts_manager_v1_interface, 1));
    if (!self->manager)
        self->owner->enterFailure(QStringLiteral("Typhon shortcuts manager binding failed"),
                                  true);
}

void TyphonShortcutClientPrivate::registryGlobalRemove(void *, wl_registry *, std::uint32_t)
{
}

void TyphonShortcutClientPrivate::registrySynchronized(void *data, wl_callback *callback,
                                                        std::uint32_t)
{
    auto *self = static_cast<TyphonShortcutClientPrivate *>(data);
    if (!self || self->registrySync != callback)
        return;
    self->registrySync = nullptr;
    wl_callback_destroy(callback);
    if (!self->started || !self->registry)
        return;
    self->registryReady = true;
    if (!self->manager) {
        self->owner->enterUnsupported(QStringLiteral(
            "Typhon shortcuts manager is unavailable in this compositor session"));
        return;
    }
    self->registerShortcuts();
}

void TyphonShortcutClientPrivate::shortcutPressed(void *data, astrea_shortcut_v1 *,
                                                  std::uint32_t serial,
                                                  std::uint32_t timestamp)
{
    auto *registration = static_cast<TyphonShortcutRegistration *>(data);
    if (registration && registration->privateState)
        registration->privateState->handleEvent(registration, TyphonShortcutPhase::Pressed,
                                                serial, timestamp);
}

void TyphonShortcutClientPrivate::shortcutRepeated(void *data, astrea_shortcut_v1 *,
                                                   std::uint32_t serial,
                                                   std::uint32_t timestamp)
{
    auto *registration = static_cast<TyphonShortcutRegistration *>(data);
    if (registration && registration->privateState)
        registration->privateState->handleEvent(registration, TyphonShortcutPhase::Repeated,
                                                serial, timestamp);
}

void TyphonShortcutClientPrivate::shortcutReleased(void *data, astrea_shortcut_v1 *,
                                                   std::uint32_t serial,
                                                   std::uint32_t timestamp)
{
    auto *registration = static_cast<TyphonShortcutRegistration *>(data);
    if (registration && registration->privateState)
        registration->privateState->handleEvent(registration, TyphonShortcutPhase::Released,
                                                serial, timestamp);
}

void TyphonShortcutClientPrivate::shortcutCancelled(void *data, astrea_shortcut_v1 *,
                                                    std::uint32_t serial)
{
    auto *registration = static_cast<TyphonShortcutRegistration *>(data);
    if (registration && registration->privateState)
        registration->privateState->handleEvent(registration, TyphonShortcutPhase::Cancelled,
                                                serial, 0);
}

void TyphonShortcutClientPrivate::destroyProtocolObjects(bool sendDestroyRequests)
{
    for (TyphonShortcutRegistration *registration : std::as_const(registrations)) {
        if (registration->proxy) {
            if (sendDestroyRequests)
                shortcutDestroy(registration->proxy);
            else
                wl_proxy_destroy(reinterpret_cast<wl_proxy *>(registration->proxy));
        }
        registration->proxy = nullptr;
        registration->live = false;
        delete registration;
    }
    registrations.clear();
    if (registrySync) {
        if (sendDestroyRequests)
            wl_callback_destroy(registrySync);
        else
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(registrySync));
    }
    registrySync = nullptr;
    if (manager) {
        if (sendDestroyRequests)
            managerDestroy(manager);
        else
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(manager));
    }
    manager = nullptr;
    if (registry) {
        if (sendDestroyRequests)
            wl_registry_destroy(registry);
        else
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(registry));
    }
    registry = nullptr;
    registryReady = false;
}

const wl_registry_listener TyphonShortcutClientPrivate::registryListener = {
    &TyphonShortcutClientPrivate::registryGlobal,
    &TyphonShortcutClientPrivate::registryGlobalRemove,
};

const wl_callback_listener TyphonShortcutClientPrivate::registrySyncListener = {
    &TyphonShortcutClientPrivate::registrySynchronized,
};

const astrea_shortcut_v1_listener TyphonShortcutClientPrivate::shortcutListener = {
    &TyphonShortcutClientPrivate::shortcutPressed,
    &TyphonShortcutClientPrivate::shortcutRepeated,
    &TyphonShortcutClientPrivate::shortcutReleased,
    &TyphonShortcutClientPrivate::shortcutCancelled,
};
#endif

TyphonShortcutClient::TyphonShortcutClient(QObject *parent)
    : TyphonShortcutClient(nullptr, parent)
{
}

TyphonShortcutClient::TyphonShortcutClient(TyphonSharedConnection *sharedConnection,
                                           QObject *parent)
    : QObject(parent), m_private(std::make_unique<TyphonShortcutClientPrivate>(this,
                                                                                sharedConnection))
{
    m_private->reconnectTimer.setSingleShot(true);
    connect(&m_private->reconnectTimer, &QTimer::timeout, this, [this] {
        if (m_private->started)
            beginConnection();
    });
    if (sharedConnection) {
        connect(sharedConnection, &TyphonSharedConnection::ready, this,
                [this](quint64 generation) { beginSharedGeneration(generation); });
        connect(sharedConnection, &TyphonSharedConnection::disconnected, this,
                [this](quint64 generation) { handleSharedDisconnected(generation); });
    } else {
        connect(m_private->display, &TyphonWaylandDisplay::disconnected, this,
                &TyphonShortcutClient::handleDisplayDisconnected);
        connect(m_private->display, &TyphonWaylandDisplay::protocolError, this,
                [this](const QString &message) { enterFailure(message, true); });
    }
}

TyphonShortcutClient::~TyphonShortcutClient()
{
    stop();
}

void TyphonShortcutClient::start()
{
    if (m_private->started)
        return;
    m_private->started = true;
    m_private->backoffIndex = 0;
    m_private->reconnectTimer.stop();

    if (m_private->sharedConnection) {
        if (m_private->sharedConnection->isReady())
            beginSharedGeneration(m_private->sharedConnection->connectionGeneration());
        else if (m_private->sharedConnection->state() == TyphonSharedConnection::State::Stopped)
            m_private->sharedConnection->start();
        return;
    }

    beginConnection();
}

void TyphonShortcutClient::stop()
{
    if (!m_private->started && m_private->state == TyphonShortcutConnectionState::Stopped)
        return;
    m_private->started = false;
    m_private->reconnectTimer.stop();
#if ASTREA_HAVE_TYPHON_PROTOCOL
    m_private->destroyProtocolObjects();
#endif
    if (!m_private->sharedConnection && m_private->display->isConnected())
        m_private->display->disconnectFromDisplay();
    ++m_private->generation;
    setState(TyphonShortcutConnectionState::Stopped);
}

TyphonShortcutConnectionState TyphonShortcutClient::state() const
{
    return m_private->state;
}

bool TyphonShortcutClient::isReady() const
{
    return m_private->state == TyphonShortcutConnectionState::Ready;
}

int TyphonShortcutClient::registeredShortcutCount() const
{
#if ASTREA_HAVE_TYPHON_PROTOCOL
    return static_cast<int>(std::count_if(
        m_private->registrations.cbegin(), m_private->registrations.cend(),
        [](const TyphonShortcutRegistration *registration) {
            return registration && registration->live && registration->proxy;
        }));
#else
    return 0;
#endif
}

std::uint64_t TyphonShortcutClient::connectionGeneration() const
{
    return m_private->generation;
}

void TyphonShortcutClient::beginConnection()
{
    if (!m_private->started)
        return;

    ++m_private->generation;
    m_private->backoffIndex = std::min(m_private->backoffIndex, 4);
    setState(TyphonShortcutConnectionState::Connecting);

#if !ASTREA_HAVE_TYPHON_PROTOCOL
    enterUnsupported(QStringLiteral("Typhon shortcuts protocol is unavailable in this build"));
    return;
#else
    if (!m_private->display->connectToDisplay()) {
        enterFailure(QStringLiteral("Typhon shortcuts could not connect to Wayland"), true);
        return;
    }

    QString diagnostic;
    if (!Astrea::Typhon::TyphonShellAuthenticator::authenticate(
            m_private->display->nativeDisplay(), &diagnostic)) {
        enterFailure(diagnostic, true);
        return;
    }

    m_private->registry = wl_display_get_registry(m_private->display->nativeDisplay());
    if (!m_private->registry
        || wl_registry_add_listener(m_private->registry, &TyphonShortcutClientPrivate::registryListener,
                                    m_private.get()) != 0) {
        enterFailure(QStringLiteral("Typhon shortcuts registry setup failed"), true);
        return;
    }
    m_private->registrySync = wl_display_sync(m_private->display->nativeDisplay());
    if (!m_private->registrySync
        || wl_callback_add_listener(m_private->registrySync,
                                    &TyphonShortcutClientPrivate::registrySyncListener,
                                    m_private.get()) != 0) {
        enterFailure(QStringLiteral("Typhon shortcuts registry synchronization failed"), true);
        return;
    }
    setState(TyphonShortcutConnectionState::WaitingForManager);
    if (!m_private->display->flush())
        enterFailure(QStringLiteral("Typhon shortcuts registry flush failed"), true);
#endif
}

void TyphonShortcutClient::beginSharedGeneration(std::uint64_t generation)
{
    if (!m_private->started || !m_private->sharedConnection || generation == 0)
        return;

#if !ASTREA_HAVE_TYPHON_PROTOCOL
    enterUnsupported(QStringLiteral("Typhon shortcuts protocol is unavailable in this build"));
    return;
#else
    m_private->generation = generation;
    m_private->backoffIndex = 0;
    m_private->destroyProtocolObjects();
    m_private->display = m_private->sharedConnection->waylandDisplay();
    setState(TyphonShortcutConnectionState::Connecting);

    if (!m_private->display || !m_private->sharedConnection->nativeDisplay()) {
        enterFailure(QStringLiteral("Shared Typhon display is unavailable"), false);
        return;
    }

    m_private->registry = wl_display_get_registry(m_private->sharedConnection->nativeDisplay());
    if (!m_private->registry
        || wl_registry_add_listener(m_private->registry,
                                    &TyphonShortcutClientPrivate::registryListener,
                                    m_private.get()) != 0) {
        enterFailure(QStringLiteral("Typhon shared shortcut registry setup failed"), false);
        return;
    }
    m_private->registrySync = wl_display_sync(m_private->sharedConnection->nativeDisplay());
    if (!m_private->registrySync
        || wl_callback_add_listener(m_private->registrySync,
                                    &TyphonShortcutClientPrivate::registrySyncListener,
                                    m_private.get()) != 0) {
        enterFailure(QStringLiteral("Typhon shared shortcut synchronization failed"), false);
        return;
    }
    setState(TyphonShortcutConnectionState::WaitingForManager);
    if (!m_private->sharedConnection->flush())
        enterFailure(QStringLiteral("Typhon shared shortcut registry flush failed"), false);
#endif
}

void TyphonShortcutClient::handleSharedDisconnected(std::uint64_t generation)
{
    if (!m_private->started || !m_private->sharedConnection
        || generation != m_private->generation)
        return;
#if ASTREA_HAVE_TYPHON_PROTOCOL
    // Invalidate the old generation before the shared connection closes the
    // transport; marshalling destroy requests is not safe during teardown.
    m_private->destroyProtocolObjects(false);
#endif
    setState(TyphonShortcutConnectionState::Disconnected);
}

void TyphonShortcutClient::setState(TyphonShortcutConnectionState state)
{
    if (m_private->state == state)
        return;
    m_private->state = state;
    emit stateChanged(state);
}

void TyphonShortcutClient::enterFailure(const QString &message, bool reconnect)
{
    if (!m_private->started)
        return;
    emit diagnostic(message);
    qWarning("Typhon shortcut client degraded: %s", qPrintable(message));
    m_private->failureInProgress = true;
#if ASTREA_HAVE_TYPHON_PROTOCOL
    m_private->destroyProtocolObjects();
#endif
    if (!m_private->sharedConnection && m_private->display->isConnected())
        m_private->display->disconnectFromDisplay();
    m_private->failureInProgress = false;
    setState(TyphonShortcutConnectionState::Degraded);
    if (reconnect)
        scheduleReconnect();
}

void TyphonShortcutClient::enterUnsupported(const QString &message)
{
    if (!m_private->started)
        return;
    emit diagnostic(message);
    m_private->failureInProgress = true;
#if ASTREA_HAVE_TYPHON_PROTOCOL
    m_private->destroyProtocolObjects();
#endif
    if (!m_private->sharedConnection && m_private->display->isConnected())
        m_private->display->disconnectFromDisplay();
    m_private->failureInProgress = false;
    setState(TyphonShortcutConnectionState::Unsupported);
    scheduleReconnect();
}

void TyphonShortcutClient::handleDisplayDisconnected()
{
    if (!m_private->started || m_private->failureInProgress)
        return;
#if ASTREA_HAVE_TYPHON_PROTOCOL
    m_private->registry = nullptr;
    m_private->registrySync = nullptr;
    m_private->manager = nullptr;
    qDeleteAll(m_private->registrations);
    m_private->registrations.clear();
    m_private->registryReady = false;
#endif
    setState(TyphonShortcutConnectionState::Disconnected);
    scheduleReconnect();
}

void TyphonShortcutClient::scheduleReconnect()
{
    if (!m_private->started || m_private->reconnectTimer.isActive())
        return;
    m_private->reconnectTimer.start(reconnectDelay());
    m_private->backoffIndex = std::min(m_private->backoffIndex + 1, 4);
}

int TyphonShortcutClient::reconnectDelay() const
{
    static constexpr int delays[] = {250, 500, 1000, 2000, 5000};
    return delays[std::clamp(m_private->backoffIndex, 0, 4)];
}
