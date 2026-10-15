#include "platform/typhon/TyphonShortcutClient.hpp"

#include "platform/typhon/TyphonSharedConnection.hpp"
#include "platform/typhon/TyphonShellAuthenticator.hpp"
#include "platform/typhon/TyphonWaylandDisplay.hpp"

#include <QDebug>
#include <QTimer>
#include <QVector>

#include <algorithm>
#include <iterator>
#include <vector>

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
    void (*registered)(void *, struct astrea_shortcut_v1 *, std::uint32_t);
    void (*rejected)(void *, struct astrea_shortcut_v1 *, std::uint32_t, std::uint32_t);
};
}

namespace {

constexpr std::uint32_t kManagerDestroyOpcode = 0;
constexpr std::uint32_t kManagerRegisterShortcutOpcode = 1;
constexpr std::uint32_t kManagerRegisterShortcutIfAvailableOpcode = 2;
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
                                     const char *description, bool ifAvailable)
{
    const std::uint32_t opcode = ifAvailable ? kManagerRegisterShortcutIfAvailableOpcode
                                             : kManagerRegisterShortcutOpcode;
    return reinterpret_cast<astrea_shortcut_v1 *>(wl_proxy_marshal_flags(
        reinterpret_cast<wl_proxy *>(manager), opcode,
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
struct TyphonShortcutListenerContext;

enum class ShortcutRegistrationHealth {
    Pending,
    Registered,
    RecoveryPending,
    Cancelled,
    RejectedAuthorization,
    Occupied,
};
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
    std::uint32_t managerVersion = 1;
    QVector<TyphonShortcutRegistration *> registrations;
    std::vector<std::unique_ptr<TyphonShortcutListenerContext>> listenerContexts;

    void destroyProtocolObjects(bool sendDestroyRequests = true);
    void registerShortcuts();
    bool createRegistration(TyphonShortcutRegistration *registration, bool ifAvailable);
    TyphonShortcutRegistration *validatedRegistration(TyphonShortcutListenerContext *context,
                                                       astrea_shortcut_v1 *source) const;
    void handleInputEvent(TyphonShortcutListenerContext *context, astrea_shortcut_v1 *source,
                          TyphonShortcutPhase phase, std::uint32_t serial,
                          std::uint32_t timestamp);
    void handleCancelled(TyphonShortcutListenerContext *context, astrea_shortcut_v1 *source,
                         std::uint32_t serial);
    void handleRegistered(TyphonShortcutListenerContext *context, astrea_shortcut_v1 *source,
                          std::uint32_t serial);
    void handleRejected(TyphonShortcutListenerContext *context, astrea_shortcut_v1 *source,
                        std::uint32_t serial, std::uint32_t reason);
    void scheduleRecovery(TyphonShortcutRegistration *registration);
    void retryRecovery(TyphonShortcutRegistration *registration,
                       std::uint64_t expectedGeneration);
    void updateRegistrationState();
    QVariantMap registrationHealth() const;

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
    static void shortcutRegistered(void *data, astrea_shortcut_v1 *, std::uint32_t serial);
    static void shortcutRejected(void *data, astrea_shortcut_v1 *, std::uint32_t serial,
                                std::uint32_t reason);

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
    QString description;
    bool live = false;
    bool recoveryRequest = false;
    int recoveryAttempts = 0;
    std::uint64_t requestGeneration = 0;
    ShortcutRegistrationHealth health = ShortcutRegistrationHealth::Pending;
    QString lastOutcome = QStringLiteral("pending");
    QTimer recoveryTimer;
};

struct TyphonShortcutListenerContext {
    TyphonShortcutClientPrivate *privateState = nullptr;
    TyphonShortcutRegistration *registration = nullptr;
    std::uint64_t generation = 0;
    std::uint64_t requestGeneration = 0;
    astrea_shortcut_v1 *proxy = nullptr;
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
    {"screenshot_quick", "Capture the compositor-published screen"},
    {"screenshot_region_frozen", "Select a region from a frozen screenshot"},
    {"screenshot_region_live", "Select a region from the live desktop"},
};

} // namespace

namespace {

constexpr int kShortcutRecoveryAttempts = 3;
constexpr int kShortcutRecoveryDelaysMs[] = {500, 1500, 3500};

QString registrationHealthName(ShortcutRegistrationHealth health)
{
    switch (health) {
    case ShortcutRegistrationHealth::Pending:
        return QStringLiteral("pending");
    case ShortcutRegistrationHealth::Registered:
        return QStringLiteral("registered");
    case ShortcutRegistrationHealth::RecoveryPending:
        return QStringLiteral("recovery-pending");
    case ShortcutRegistrationHealth::Cancelled:
        return QStringLiteral("cancelled");
    case ShortcutRegistrationHealth::RejectedAuthorization:
        return QStringLiteral("rejected-authorization");
    case ShortcutRegistrationHealth::Occupied:
        return QStringLiteral("occupied");
    }
    return QStringLiteral("unknown");
}

} // namespace

bool TyphonShortcutClientPrivate::createRegistration(TyphonShortcutRegistration *registration,
                                                      bool ifAvailable)
{
    if (!registration || !manager || !display)
        return false;
    const QByteArray namespaceName = registration->namespaceName.toUtf8();
    const QByteArray name = registration->name.toUtf8();
    const QByteArray description = registration->description.toUtf8();
    auto context = std::make_unique<TyphonShortcutListenerContext>();
    context->privateState = this;
    context->registration = registration;
    context->generation = generation;
    context->requestGeneration = ++registration->requestGeneration;
    context->proxy = registerShortcut(manager, namespaceName.constData(), name.constData(),
                                      description.constData(), ifAvailable);
    if (!context->proxy)
        return false;
    if (shortcutAddListener(context->proxy, &shortcutListener, context.get()) != 0) {
        shortcutDestroy(context->proxy);
        context->proxy = nullptr;
        return false;
    }

    registration->proxy = context->proxy;
    registration->live = true;
    registration->recoveryRequest = ifAvailable;
    registration->health = managerVersion >= 2 ? ShortcutRegistrationHealth::Pending
                                                : ShortcutRegistrationHealth::Registered;
    listenerContexts.push_back(std::move(context));
    return true;
}

TyphonShortcutRegistration *TyphonShortcutClientPrivate::validatedRegistration(
    TyphonShortcutListenerContext *context, astrea_shortcut_v1 *source) const
{
    if (!started || !context || context->privateState != this || !context->registration
        || context->generation != generation || context->proxy != source)
        return nullptr;
    TyphonShortcutRegistration *registration = context->registration;
    if (registration->generation != generation
        || registration->requestGeneration != context->requestGeneration
        || registration->proxy != source)
        return nullptr;
    return registration;
}

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
        registration->description = QString::fromLatin1(spec.description);
        registration->recoveryTimer.setSingleShot(true);
        QObject::connect(&registration->recoveryTimer, &QTimer::timeout, owner,
                         [this, registration, expectedGeneration = generation] {
                             retryRecovery(registration, expectedGeneration);
                         });
        if (!createRegistration(registration, false)) {
            delete registration;
            owner->enterFailure(QStringLiteral("Typhon shortcut registration failed"),
                                !sharedConnection);
            return;
        }
        registrations.append(registration);
    }
    if (!display->flush()) {
        owner->enterFailure(QStringLiteral("Typhon shortcut registration flush failed"),
                            !sharedConnection);
        return;
    }
    if (managerVersion < 2) {
        for (TyphonShortcutRegistration *registration : std::as_const(registrations)) {
            registration->health = ShortcutRegistrationHealth::Registered;
            registration->lastOutcome = QStringLiteral("legacy-registration-requested");
        }
    }
    updateRegistrationState();
    emit owner->registrationHealthChanged();
}

void TyphonShortcutClientPrivate::handleInputEvent(TyphonShortcutListenerContext *context,
                                                    astrea_shortcut_v1 *source,
                                                    TyphonShortcutPhase phase,
                                                    std::uint32_t serial,
                                                    std::uint32_t timestamp)
{
    TyphonShortcutRegistration *registration = validatedRegistration(context, source);
    if (!registration || !registration->live)
        return;
    owner->shortcutEvent(registration->namespaceName, registration->name, phase, serial,
                         timestamp);
}

void TyphonShortcutClientPrivate::handleCancelled(TyphonShortcutListenerContext *context,
                                                   astrea_shortcut_v1 *source,
                                                   std::uint32_t serial)
{
    TyphonShortcutRegistration *registration = validatedRegistration(context, source);
    if (!registration || !registration->live)
        return;

    const QString shortcutName = registration->name;
    const QString namespaceName = registration->namespaceName;
    registration->live = false;
    registration->lastOutcome = QStringLiteral("cancelled");
    registration->health = managerVersion >= 2
            && registration->recoveryAttempts < kShortcutRecoveryAttempts
        ? ShortcutRegistrationHealth::RecoveryPending
        : ShortcutRegistrationHealth::Cancelled;
    if (registration->proxy) {
        shortcutDestroy(registration->proxy);
        registration->proxy = nullptr;
    }
    if (registration->health == ShortcutRegistrationHealth::RecoveryPending)
        scheduleRecovery(registration);
    if (!display || !display->flush()) {
        owner->enterFailure(QStringLiteral("Cancelled Typhon shortcut release could not be flushed"),
                            !sharedConnection);
        return;
    }
    updateRegistrationState();
    owner->registrationHealthChanged();
    owner->diagnostic(QStringLiteral("Typhon shortcut registration cancelled: %1")
                          .arg(shortcutName));
    owner->shortcutEvent(namespaceName, shortcutName, TyphonShortcutPhase::Cancelled, serial, 0);
}

void TyphonShortcutClientPrivate::handleRegistered(TyphonShortcutListenerContext *context,
                                                    astrea_shortcut_v1 *source,
                                                    std::uint32_t serial)
{
    TyphonShortcutRegistration *registration = validatedRegistration(context, source);
    if (!registration || !registration->live)
        return;
    const QString shortcutName = registration->name;
    registration->health = ShortcutRegistrationHealth::Registered;
    registration->recoveryRequest = false;
    registration->recoveryAttempts = 0;
    registration->lastOutcome = QStringLiteral("registered");
    registration->recoveryTimer.stop();
    updateRegistrationState();
    owner->registrationHealthChanged();
    owner->diagnostic(QStringLiteral("Typhon shortcut registration accepted: %1 (%2)")
                          .arg(shortcutName)
                          .arg(serial));
}

void TyphonShortcutClientPrivate::handleRejected(TyphonShortcutListenerContext *context,
                                                  astrea_shortcut_v1 *source,
                                                  std::uint32_t serial,
                                                  std::uint32_t reason)
{
    TyphonShortcutRegistration *registration = validatedRegistration(context, source);
    if (!registration || !registration->live)
        return;

    const QString shortcutName = registration->name;
    registration->live = false;
    registration->lastOutcome = reason == 1 ? QStringLiteral("authorization-rejected")
                                             : QStringLiteral("registration-rejected");
    if (registration->proxy) {
        shortcutDestroy(registration->proxy);
        registration->proxy = nullptr;
    }
    const bool occupiedRecovery = reason == 2 && registration->recoveryRequest;
    if (reason == 1) {
        registration->health = ShortcutRegistrationHealth::RejectedAuthorization;
    } else if (occupiedRecovery
               && registration->recoveryAttempts < kShortcutRecoveryAttempts) {
        registration->health = ShortcutRegistrationHealth::RecoveryPending;
        scheduleRecovery(registration);
    } else {
        registration->health = ShortcutRegistrationHealth::Occupied;
    }
    if (!display || !display->flush()) {
        owner->enterFailure(QStringLiteral("Rejected Typhon shortcut release could not be flushed"),
                            !sharedConnection);
        return;
    }
    updateRegistrationState();
    owner->registrationHealthChanged();
    owner->diagnostic(QStringLiteral("Typhon shortcut registration rejected: %1, reason %2, serial %3")
                          .arg(shortcutName)
                          .arg(reason)
                          .arg(serial));
}

void TyphonShortcutClientPrivate::scheduleRecovery(TyphonShortcutRegistration *registration)
{
    if (!started || !registration || registration->generation != generation
        || managerVersion < 2 || registration->recoveryAttempts >= kShortcutRecoveryAttempts)
        return;
    const int delayIndex = std::min(registration->recoveryAttempts,
                                    kShortcutRecoveryAttempts - 1);
    registration->recoveryTimer.start(kShortcutRecoveryDelaysMs[delayIndex]);
}

void TyphonShortcutClientPrivate::retryRecovery(TyphonShortcutRegistration *registration,
                                                 std::uint64_t expectedGeneration)
{
    if (!started || expectedGeneration != generation || !registration
        || registration->generation != generation || managerVersion < 2 || !manager
        || registration->health != ShortcutRegistrationHealth::RecoveryPending
        || registration->recoveryAttempts >= kShortcutRecoveryAttempts
        || !registrations.contains(registration))
        return;

    ++registration->recoveryAttempts;
    const QString shortcutName = registration->name;
    const int attempt = registration->recoveryAttempts;
    registration->health = ShortcutRegistrationHealth::Pending;
    registration->lastOutcome = QStringLiteral("conditional-claim-%1")
                                    .arg(registration->recoveryAttempts);
    if (!createRegistration(registration, true)) {
        owner->enterFailure(QStringLiteral("Conditional Typhon shortcut recovery failed"),
                            !sharedConnection);
        return;
    }
    if (!display || !display->flush()) {
        owner->enterFailure(QStringLiteral("Conditional Typhon shortcut recovery flush failed"),
                            !sharedConnection);
        return;
    }
    updateRegistrationState();
    owner->registrationHealthChanged();
    owner->diagnostic(QStringLiteral("Typhon shortcut conditional recovery requested: %1, attempt %2")
                          .arg(shortcutName)
                          .arg(attempt));
}

void TyphonShortcutClientPrivate::updateRegistrationState()
{
    if (!started || !registryReady)
        return;
    bool allRegistered = registrations.size() == static_cast<qsizetype>(std::size(kShortcutSpecs));
    bool recoveryOrTerminal = false;
    for (const TyphonShortcutRegistration *registration : std::as_const(registrations)) {
        if (!registration || registration->health != ShortcutRegistrationHealth::Registered)
            allRegistered = false;
        if (registration
            && (registration->health == ShortcutRegistrationHealth::RecoveryPending
                || registration->health == ShortcutRegistrationHealth::Cancelled
                || registration->health == ShortcutRegistrationHealth::RejectedAuthorization
                || registration->health == ShortcutRegistrationHealth::Occupied
                || (registration->health == ShortcutRegistrationHealth::Pending
                    && registration->recoveryRequest)))
            recoveryOrTerminal = true;
    }
    if (allRegistered)
        owner->setState(TyphonShortcutConnectionState::Ready);
    else if (recoveryOrTerminal)
        owner->setState(TyphonShortcutConnectionState::Degraded);
    else
        owner->setState(TyphonShortcutConnectionState::Registering);
}

QVariantMap TyphonShortcutClientPrivate::registrationHealth() const
{
    QVariantMap result;
    for (const TyphonShortcutRegistration *registration : std::as_const(registrations)) {
        if (!registration)
            continue;
        QVariantMap entry;
        entry.insert(QStringLiteral("state"), registrationHealthName(registration->health));
        entry.insert(QStringLiteral("last_outcome"), registration->lastOutcome);
        entry.insert(QStringLiteral("recovery_pending"),
                     registration->recoveryTimer.isActive()
                         || registration->health == ShortcutRegistrationHealth::RecoveryPending
                         || (registration->recoveryRequest && registration->live));
        entry.insert(QStringLiteral("recovery_attempts"), registration->recoveryAttempts);
        entry.insert(QStringLiteral("recovery_attempt_limit"), kShortcutRecoveryAttempts);
        entry.insert(QStringLiteral("protocol_version"), managerVersion);
        result.insert(registration->name, entry);
    }
    return result;
}

void TyphonShortcutClientPrivate::registryGlobal(void *data, wl_registry *registry,
                                                 std::uint32_t name, const char *interfaceName,
                                                 std::uint32_t version)
{
    auto *self = static_cast<TyphonShortcutClientPrivate *>(data);
    if (!self || !self->started || self->registry != registry || self->manager
        || qstrcmp(interfaceName, "astrea_shortcuts_manager_v1") != 0 || version < 1)
        return;

    self->managerVersion = std::min(version, 2u);
    self->manager = static_cast<astrea_shortcuts_manager_v1 *>(
        wl_registry_bind(registry, name, &astrea_shortcuts_manager_v1_interface,
                         self->managerVersion));
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

void TyphonShortcutClientPrivate::shortcutPressed(void *data, astrea_shortcut_v1 *source,
                                                  std::uint32_t serial,
                                                  std::uint32_t timestamp)
{
    auto *context = static_cast<TyphonShortcutListenerContext *>(data);
    if (context && context->privateState)
        context->privateState->handleInputEvent(context, source, TyphonShortcutPhase::Pressed,
                                                serial, timestamp);
}

void TyphonShortcutClientPrivate::shortcutRepeated(void *data, astrea_shortcut_v1 *source,
                                                   std::uint32_t serial,
                                                   std::uint32_t timestamp)
{
    auto *context = static_cast<TyphonShortcutListenerContext *>(data);
    if (context && context->privateState)
        context->privateState->handleInputEvent(context, source, TyphonShortcutPhase::Repeated,
                                                serial, timestamp);
}

void TyphonShortcutClientPrivate::shortcutReleased(void *data, astrea_shortcut_v1 *source,
                                                   std::uint32_t serial,
                                                   std::uint32_t timestamp)
{
    auto *context = static_cast<TyphonShortcutListenerContext *>(data);
    if (context && context->privateState)
        context->privateState->handleInputEvent(context, source, TyphonShortcutPhase::Released,
                                                serial, timestamp);
}

void TyphonShortcutClientPrivate::shortcutCancelled(void *data, astrea_shortcut_v1 *source,
                                                    std::uint32_t serial)
{
    auto *context = static_cast<TyphonShortcutListenerContext *>(data);
    if (context && context->privateState)
        context->privateState->handleCancelled(context, source, serial);
}

void TyphonShortcutClientPrivate::shortcutRegistered(void *data, astrea_shortcut_v1 *source,
                                                     std::uint32_t serial)
{
    auto *context = static_cast<TyphonShortcutListenerContext *>(data);
    if (context && context->privateState)
        context->privateState->handleRegistered(context, source, serial);
}

void TyphonShortcutClientPrivate::shortcutRejected(void *data, astrea_shortcut_v1 *source,
                                                    std::uint32_t serial,
                                                    std::uint32_t reason)
{
    auto *context = static_cast<TyphonShortcutListenerContext *>(data);
    if (context && context->privateState)
        context->privateState->handleRejected(context, source, serial, reason);
}

void TyphonShortcutClientPrivate::destroyProtocolObjects(bool sendDestroyRequests)
{
    for (const auto &context : listenerContexts) {
        context->registration = nullptr;
        context->proxy = nullptr;
    }
    for (TyphonShortcutRegistration *registration : std::as_const(registrations)) {
        registration->recoveryTimer.stop();
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
    listenerContexts.clear();
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
    managerVersion = 1;
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
    &TyphonShortcutClientPrivate::shortcutRegistered,
    &TyphonShortcutClientPrivate::shortcutRejected,
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
    if (m_private->display && m_private->display->isConnected()
        && !m_private->display->flush())
        qWarning("Typhon shortcut teardown requests could not be flushed");
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
            return registration && registration->live && registration->proxy
                && registration->health == ShortcutRegistrationHealth::Registered;
        }));
#else
    return 0;
#endif
}

QVariantMap TyphonShortcutClient::registrationHealth() const
{
#if ASTREA_HAVE_TYPHON_PROTOCOL
    return m_private->registrationHealth();
#else
    return {};
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
    m_private->destroyProtocolObjects(false);
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
