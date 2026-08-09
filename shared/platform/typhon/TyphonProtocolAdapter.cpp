#include "platform/typhon/TyphonProtocolAdapter.hpp"
#include "platform/typhon/TyphonShellAuthenticator.hpp"
#include "platform/typhon/TyphonWaylandDisplay.hpp"

#include <QHash>
#include <QVector>
#include <QtAlgorithms>

using namespace Astrea::Typhon;

#if ASTREA_HAVE_TYPHON_PROTOCOL
#include "astrea-toplevel-management-v1-client-protocol.h"

namespace {

quint64 joinUint32(quint32 high, quint32 low)
{
    return (static_cast<quint64>(high) << 32) | static_cast<quint64>(low);
}

class GeneratedTyphonProtocolAdapter final : public TyphonProtocolAdapter {
public:
    explicit GeneratedTyphonProtocolAdapter(QObject *parent = nullptr)
        : TyphonProtocolAdapter(parent), m_display(new TyphonWaylandDisplay(this))
    {
        connect(m_display, &TyphonWaylandDisplay::protocolError, this,
                [this](const QString &diagnostic) { emit protocolError(diagnostic); });
        connect(m_display, &TyphonWaylandDisplay::disconnected, this,
                [this] { onDisplayDisconnected(); });
    }

    void start() override
    {
        if (m_running)
            return;
        m_running = true;
        m_terminal = false;
        m_protocolErrorReported = false;
        m_registryReady = false;
        m_manager = nullptr;
        m_registry = nullptr;
        m_registrySync = nullptr;
        m_managerVersion = 0;
        m_authenticated = false;
        setActionCapability(TyphonActionCapabilityState::AuthenticatingV2);

        if (!m_display->connectToDisplay()) {
            m_running = false;
            setActionCapability(TyphonActionCapabilityState::Disconnected);
            emit displayDisconnected();
            return;
        }

        wl_display *display = m_display->nativeDisplay();
        QString diagnostic;
        if (!TyphonShellAuthenticator::authenticate(display, &diagnostic)) {
            emit capabilityDiagnostic(diagnostic);
        } else {
            m_authenticated = true;
        }
        m_registry = wl_display_get_registry(display);
        if (!m_registry || wl_registry_add_listener(m_registry, &kRegistryListener, this) != 0) {
            failProtocol(QStringLiteral("Typhon registry setup failed"));
            return;
        }

        m_registrySync = wl_display_sync(display);
        if (!m_registrySync || wl_callback_add_listener(m_registrySync, &kSyncListener, this) != 0) {
            failProtocol(QStringLiteral("Typhon registry synchronization failed"));
            return;
        }
        m_display->flush();
    }

    void stop() override
    {
        if (!m_running && !m_display->isConnected())
            return;
        m_running = false;
        m_terminal = true;
        destroyHandles();
        if (m_registrySync) {
            wl_callback_destroy(m_registrySync);
            m_registrySync = nullptr;
        }
        if (m_manager) {
            astrea_toplevel_manager_v1_destroy(m_manager);
            m_manager = nullptr;
        }
        if (m_registry) {
            wl_registry_destroy(m_registry);
            m_registry = nullptr;
        }
        m_display->disconnectFromDisplay();
        m_registryReady = false;
        m_managerVersion = 0;
        m_authenticated = false;
        setActionCapability(TyphonActionCapabilityState::Disconnected);
    }

    bool isAvailable() const override { return m_running && m_registryReady && m_manager; }

    TyphonActionCapabilityState actionCapability() const override { return m_actionCapability; }
    quint32 managerVersion() const override { return m_managerVersion; }

    std::optional<ToplevelActionError> requestAction(
        quint64 handleToken, TyphonActionToken token, ToplevelAction action) override
    {
        if (!m_running || !m_manager)
            return ToplevelActionError::Disconnected;
        if (m_managerVersion < 2)
            return ToplevelActionError::UnsupportedProtocol;
        if (!m_authenticated || m_actionCapability != TyphonActionCapabilityState::ActionReadyV2)
            return ToplevelActionError::NotAuthenticated;
        if (!token.isValid())
            return ToplevelActionError::InvalidRequest;

        const auto it = m_handles.constFind(handleToken);
        if (it == m_handles.constEnd() || !it.value() || it.value()->closed
            || !it.value()->proxy || astrea_toplevel_v1_get_version(it.value()->proxy) < 2) {
            return ToplevelActionError::ToplevelNotLive;
        }

        switch (action) {
        case ToplevelAction::Activate:
            astrea_toplevel_v1_activate(it.value()->proxy, token.hi, token.lo);
            break;
        case ToplevelAction::Minimize:
            astrea_toplevel_v1_minimize(it.value()->proxy, token.hi, token.lo);
            break;
        case ToplevelAction::Restore:
            astrea_toplevel_v1_restore(it.value()->proxy, token.hi, token.lo);
            break;
        case ToplevelAction::Close:
            astrea_toplevel_v1_close(it.value()->proxy, token.hi, token.lo);
            break;
        }
        if (!m_display->flush())
            return ToplevelActionError::Disconnected;
        return std::nullopt;
    }

private:
    struct HandleState {
        GeneratedTyphonProtocolAdapter *owner = nullptr;
        quint64 token = 0;
        astrea_toplevel_v1 *proxy = nullptr;
        bool closed = false;
    };

    static void registryGlobal(void *data, wl_registry *registry, uint32_t name,
                               const char *interfaceName, uint32_t version)
    {
        auto *self = static_cast<GeneratedTyphonProtocolAdapter *>(data);
        if (!self->m_running || self->m_terminal || self->m_manager
            || qstrcmp(interfaceName, "astrea_toplevel_manager_v1") != 0 || version < 1) {
            return;
        }
        self->m_managerVersion = qMin(version, 2u);
        self->m_manager = static_cast<astrea_toplevel_manager_v1 *>(
            wl_registry_bind(registry, name, &astrea_toplevel_manager_v1_interface,
                             self->m_managerVersion));
        if (!self->m_manager
            || astrea_toplevel_manager_v1_add_listener(self->m_manager, &kManagerListener, self) != 0) {
            self->failProtocol(QStringLiteral("Typhon manager binding failed"));
        }
    }

    static void registryGlobalRemove(void *, wl_registry *, uint32_t)
    {
    }

    static void registrySynchronized(void *data, wl_callback *callback, uint32_t)
    {
        auto *self = static_cast<GeneratedTyphonProtocolAdapter *>(data);
        if (self->m_registrySync == callback)
            self->m_registrySync = nullptr;
        wl_callback_destroy(callback);
        if (!self->m_running || self->m_terminal)
            return;
        self->m_registryReady = true;
        if (!self->m_manager) {
            self->setActionCapability(TyphonActionCapabilityState::Degraded);
        } else if (self->m_managerVersion < 2) {
            self->setActionCapability(TyphonActionCapabilityState::ReadOnlyV1);
        } else if (self->m_authenticated) {
            self->setActionCapability(TyphonActionCapabilityState::ActionReadyV2);
        } else {
            self->setActionCapability(TyphonActionCapabilityState::ReadOnlyV2);
        }
        emit self->registryDiscovered(self->m_manager != nullptr);
        self->m_display->flush();
    }

    static void managerToplevel(void *data, astrea_toplevel_manager_v1 *, astrea_toplevel_v1 *proxy)
    {
        auto *self = static_cast<GeneratedTyphonProtocolAdapter *>(data);
        if (!self->m_running || self->m_terminal || !proxy) {
            self->failProtocol(QStringLiteral("toplevel event after manager terminal state"));
            return;
        }
        auto *handle = new HandleState;
        handle->owner = self;
        handle->token = self->m_nextToken++;
        handle->proxy = proxy;
        self->m_handles.insert(handle->token, handle);
        if (astrea_toplevel_v1_add_listener(proxy, &kHandleListener, handle) != 0) {
            self->m_handles.remove(handle->token);
            astrea_toplevel_v1_destroy(proxy);
            delete handle;
            self->failProtocol(QStringLiteral("Typhon handle listener setup failed"));
            return;
        }
        emit self->handleCreated(handle->token);
    }

    static void managerDone(void *data, astrea_toplevel_manager_v1 *, uint32_t revisionHigh,
                            uint32_t revisionLow, uint32_t total, uint32_t flags)
    {
        auto *self = static_cast<GeneratedTyphonProtocolAdapter *>(data);
        if (!self->liveManagerEvent())
            return;
        emit self->managerCompleted(joinUint32(revisionHigh, revisionLow), total,
                                    (flags & ASTREA_TOPLEVEL_MANAGER_V1_DONE_FLAGS_TRUNCATED) != 0);
    }

    static void managerFailed(void *data, astrea_toplevel_manager_v1 *, uint32_t reason)
    {
        auto *self = static_cast<GeneratedTyphonProtocolAdapter *>(data);
        if (!self->liveManagerEvent())
            return;
        self->m_terminal = true;
        self->setActionCapability(TyphonActionCapabilityState::Degraded);
        emit static_cast<TyphonProtocolAdapter *>(self)->managerFailed(
            QStringLiteral("Typhon manager failed (%1)").arg(reason));
    }

    static void managerActionDone(void *data, astrea_toplevel_manager_v1 *,
                                  uint32_t tokenHigh, uint32_t tokenLow,
                                  uint32_t action, uint32_t result)
    {
        auto *self = static_cast<GeneratedTyphonProtocolAdapter *>(data);
        if (!self->liveManagerEvent())
            return;

        ToplevelAction translatedAction;
        switch (action) {
        case ASTREA_TOPLEVEL_MANAGER_V1_ACTION_ACTIVATE:
            translatedAction = ToplevelAction::Activate;
            break;
        case ASTREA_TOPLEVEL_MANAGER_V1_ACTION_MINIMIZE:
            translatedAction = ToplevelAction::Minimize;
            break;
        case ASTREA_TOPLEVEL_MANAGER_V1_ACTION_RESTORE:
            translatedAction = ToplevelAction::Restore;
            break;
        case ASTREA_TOPLEVEL_MANAGER_V1_ACTION_CLOSE:
            translatedAction = ToplevelAction::Close;
            break;
        default:
            self->failProtocol(QStringLiteral("unknown Typhon action"));
            return;
        }

        ToplevelActionResult translatedResult;
        switch (result) {
        case ASTREA_TOPLEVEL_MANAGER_V1_ACTION_RESULT_ACCEPTED:
            translatedResult = ToplevelActionResult::Accepted;
            break;
        case ASTREA_TOPLEVEL_MANAGER_V1_ACTION_RESULT_NO_CHANGE:
            translatedResult = ToplevelActionResult::NoChange;
            break;
        case ASTREA_TOPLEVEL_MANAGER_V1_ACTION_RESULT_UNAVAILABLE:
            translatedResult = ToplevelActionResult::Unavailable;
            break;
        default:
            self->failProtocol(QStringLiteral("unknown Typhon action result"));
            return;
        }
        emit static_cast<TyphonProtocolAdapter *>(self)->actionCompleted(
            tokenHigh, tokenLow, translatedAction, translatedResult);
    }

    static void handleIdentifier(void *data, astrea_toplevel_v1 *, const char *identifier)
    {
        auto *handle = static_cast<HandleState *>(data);
        if (!handle->owner->liveHandleEvent(handle))
            return;
        emit handle->owner->identifierChanged(handle->token, QString::fromUtf8(identifier ? identifier : ""));
    }

    static void handleAppId(void *data, astrea_toplevel_v1 *, const char *appId)
    {
        auto *handle = static_cast<HandleState *>(data);
        if (!handle->owner->liveHandleEvent(handle))
            return;
        emit handle->owner->appIdChanged(handle->token, QString::fromUtf8(appId ? appId : ""));
    }

    static void handleTitle(void *data, astrea_toplevel_v1 *, const char *title)
    {
        auto *handle = static_cast<HandleState *>(data);
        if (!handle->owner->liveHandleEvent(handle))
            return;
        emit handle->owner->titleChanged(handle->token, QString::fromUtf8(title ? title : ""));
    }

    static void handlePid(void *data, astrea_toplevel_v1 *, uint32_t pid)
    {
        auto *handle = static_cast<HandleState *>(data);
        if (!handle->owner->liveHandleEvent(handle))
            return;
        emit handle->owner->pidChanged(handle->token, pid);
    }

    static void handleKind(void *data, astrea_toplevel_v1 *, uint32_t kind)
    {
        auto *handle = static_cast<HandleState *>(data);
        if (!handle->owner->liveHandleEvent(handle))
            return;
        ToplevelKind translated;
        switch (kind) {
        case ASTREA_TOPLEVEL_V1_KIND_XDG_TOPLEVEL: translated = ToplevelKind::XdgToplevel; break;
        case ASTREA_TOPLEVEL_V1_KIND_X11_TOPLEVEL: translated = ToplevelKind::X11Toplevel; break;
        case ASTREA_TOPLEVEL_V1_KIND_X11_DIALOG: translated = ToplevelKind::X11Dialog; break;
        default:
            handle->owner->failProtocol(QStringLiteral("unknown Typhon toplevel kind"));
            return;
        }
        emit handle->owner->kindChanged(handle->token, translated);
    }

    static void handleState(void *data, astrea_toplevel_v1 *, uint32_t rawState)
    {
        auto *handle = static_cast<HandleState *>(data);
        if (!handle->owner->liveHandleEvent(handle))
            return;
        ToplevelStates states;
        if (rawState & ASTREA_TOPLEVEL_V1_STATE_ACTIVE)
            states |= ToplevelStateFlag::Active;
        if (rawState & ASTREA_TOPLEVEL_V1_STATE_MINIMIZED)
            states |= ToplevelStateFlag::Minimized;
        if (rawState & ASTREA_TOPLEVEL_V1_STATE_MAXIMIZED)
            states |= ToplevelStateFlag::Maximized;
        if (rawState & ASTREA_TOPLEVEL_V1_STATE_FULLSCREEN)
            states |= ToplevelStateFlag::Fullscreen;
        emit handle->owner->stateChanged(handle->token, states, rawState);
    }

    static void handleFocusSerial(void *data, astrea_toplevel_v1 *, uint32_t serialHigh,
                                  uint32_t serialLow)
    {
        auto *handle = static_cast<HandleState *>(data);
        if (!handle->owner->liveHandleEvent(handle))
            return;
        emit handle->owner->focusSerialChanged(handle->token, joinUint32(serialHigh, serialLow));
    }

    static void handleDone(void *data, astrea_toplevel_v1 *, uint32_t revisionHigh,
                           uint32_t revisionLow)
    {
        auto *handle = static_cast<HandleState *>(data);
        if (!handle->owner->liveHandleEvent(handle))
            return;
        emit handle->owner->handleCompleted(handle->token, joinUint32(revisionHigh, revisionLow));
    }

    static void handleClosed(void *data, astrea_toplevel_v1 *)
    {
        auto *handle = static_cast<HandleState *>(data);
        auto *self = handle->owner;
        if (!self->m_running || self->m_terminal || handle->closed) {
            self->failProtocol(QStringLiteral("duplicate or late Typhon closed event"));
            return;
        }
        handle->closed = true;
        emit static_cast<TyphonProtocolAdapter *>(self)->handleClosed(handle->token);
        self->destroyHandle(handle);
    }

    bool liveManagerEvent() const
    {
        if (m_running && !m_terminal && m_manager && m_registryReady)
            return true;
        const_cast<GeneratedTyphonProtocolAdapter *>(this)->failProtocol(
            QStringLiteral("manager event after terminal state"));
        return false;
    }

    bool liveHandleEvent(HandleState *handle) const
    {
        if (m_running && !m_terminal && handle && !handle->closed)
            return true;
        const_cast<GeneratedTyphonProtocolAdapter *>(this)->failProtocol(
            QStringLiteral("toplevel event after terminal state"));
        return false;
    }

    void destroyHandle(HandleState *handle)
    {
        if (!handle)
            return;
        m_handles.remove(handle->token);
        if (handle->proxy)
            astrea_toplevel_v1_destroy(handle->proxy);
        handle->proxy = nullptr;
        delete handle;
    }

    void destroyHandles()
    {
        const QVector<HandleState *> handles = m_handles.values().toVector();
        for (HandleState *handle : handles) {
            if (handle->proxy)
                astrea_toplevel_v1_destroy(handle->proxy);
            handle->proxy = nullptr;
            delete handle;
        }
        m_handles.clear();
    }

    void onDisplayDisconnected()
    {
        const bool notify = m_running;
        m_running = false;
        m_terminal = true;
        m_registry = nullptr;
        m_manager = nullptr;
        m_registrySync = nullptr;
        m_managerVersion = 0;
        m_authenticated = false;
        setActionCapability(TyphonActionCapabilityState::Disconnected);
        qDeleteAll(m_handles);
        m_handles.clear();
        if (notify)
            emit displayDisconnected();
    }

    void failProtocol(const QString &diagnostic)
    {
        if (m_protocolErrorReported)
            return;
        m_protocolErrorReported = true;
        m_terminal = true;
        emit protocolError(diagnostic);
    }

    void setActionCapability(TyphonActionCapabilityState state)
    {
        if (m_actionCapability == state)
            return;
        m_actionCapability = state;
        emit static_cast<TyphonProtocolAdapter *>(this)->actionCapabilityChanged(state);
    }

    static const wl_registry_listener kRegistryListener;
    static const wl_callback_listener kSyncListener;
    static const astrea_toplevel_manager_v1_listener kManagerListener;
    static const astrea_toplevel_v1_listener kHandleListener;

    TyphonWaylandDisplay *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    wl_callback *m_registrySync = nullptr;
    astrea_toplevel_manager_v1 *m_manager = nullptr;
    QHash<quint64, HandleState *> m_handles;
    quint64 m_nextToken = 1;
    bool m_running = false;
    bool m_terminal = false;
    bool m_registryReady = false;
    bool m_protocolErrorReported = false;
    quint32 m_managerVersion = 0;
    bool m_authenticated = false;
    TyphonActionCapabilityState m_actionCapability =
        TyphonActionCapabilityState::Disconnected;
};

const wl_registry_listener GeneratedTyphonProtocolAdapter::kRegistryListener = {
    &GeneratedTyphonProtocolAdapter::registryGlobal,
    &GeneratedTyphonProtocolAdapter::registryGlobalRemove
};

const wl_callback_listener GeneratedTyphonProtocolAdapter::kSyncListener = {
    &GeneratedTyphonProtocolAdapter::registrySynchronized
};

const astrea_toplevel_manager_v1_listener GeneratedTyphonProtocolAdapter::kManagerListener = {
    &GeneratedTyphonProtocolAdapter::managerToplevel,
    &GeneratedTyphonProtocolAdapter::managerDone,
    &GeneratedTyphonProtocolAdapter::managerFailed,
    &GeneratedTyphonProtocolAdapter::managerActionDone
};

const astrea_toplevel_v1_listener GeneratedTyphonProtocolAdapter::kHandleListener = {
    &GeneratedTyphonProtocolAdapter::handleIdentifier,
    &GeneratedTyphonProtocolAdapter::handleAppId,
    &GeneratedTyphonProtocolAdapter::handleTitle,
    &GeneratedTyphonProtocolAdapter::handlePid,
    &GeneratedTyphonProtocolAdapter::handleKind,
    &GeneratedTyphonProtocolAdapter::handleState,
    &GeneratedTyphonProtocolAdapter::handleFocusSerial,
    &GeneratedTyphonProtocolAdapter::handleDone,
    &GeneratedTyphonProtocolAdapter::handleClosed
};

} // namespace
#endif

namespace {

class UnavailableTyphonProtocolAdapter final : public TyphonProtocolAdapter {
public:
    explicit UnavailableTyphonProtocolAdapter(QObject *parent = nullptr)
        : TyphonProtocolAdapter(parent)
    {
    }

    void start() override { emit registryDiscovered(false); }
    void stop() override {}
    bool isAvailable() const override { return false; }
    TyphonActionCapabilityState actionCapability() const override
    { return TyphonActionCapabilityState::Disconnected; }
    quint32 managerVersion() const override { return 0; }
    std::optional<ToplevelActionError> requestAction(
        quint64, TyphonActionToken, ToplevelAction) override
    { return ToplevelActionError::UnsupportedProtocol; }
};

} // namespace

TyphonProtocolAdapter *createDefaultTyphonProtocolAdapter(QObject *parent)
{
#if ASTREA_HAVE_TYPHON_PROTOCOL
    return new GeneratedTyphonProtocolAdapter(parent);
#else
    return new UnavailableTyphonProtocolAdapter(parent);
#endif
}
