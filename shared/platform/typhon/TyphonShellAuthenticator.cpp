#include "platform/typhon/TyphonShellAuthenticator.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QString>

#if ASTREA_HAVE_TYPHON_PROTOCOL
#include <wayland-client-core.h>

#include "astrea-shell-auth-v1-client-protocol.h"

namespace Astrea::Typhon {
namespace {

constexpr int kCapabilityLength = 64;

struct AuthenticationState {
    astrea_shell_auth_manager_v1 *manager = nullptr;
    bool authenticated = false;
    bool rejected = false;
};

struct RoundtripState {
    bool done = false;
};

void registryGlobal(void *data, wl_registry *registry, std::uint32_t name,
                    const char *interfaceName, std::uint32_t version)
{
    auto *state = static_cast<AuthenticationState *>(data);
    if (state->manager || qstrcmp(interfaceName, "astrea_shell_auth_manager_v1") != 0
        || version < 1) {
        return;
    }
    state->manager = static_cast<astrea_shell_auth_manager_v1 *>(
        wl_registry_bind(registry, name, &astrea_shell_auth_manager_v1_interface, 1));
}

void registryGlobalRemove(void *, wl_registry *, std::uint32_t)
{
}

void authenticated(void *data, astrea_shell_auth_manager_v1 *)
{
    static_cast<AuthenticationState *>(data)->authenticated = true;
}

void rejected(void *data, astrea_shell_auth_manager_v1 *)
{
    static_cast<AuthenticationState *>(data)->rejected = true;
}

const wl_registry_listener kRegistryListener = {
    &registryGlobal,
    &registryGlobalRemove,
};

const astrea_shell_auth_manager_v1_listener kAuthenticationListener = {
    &authenticated,
    &rejected,
};

void roundtripDone(void *data, wl_callback *callback, std::uint32_t)
{
    static_cast<RoundtripState *>(data)->done = true;
    wl_callback_destroy(callback);
}

const wl_callback_listener kRoundtripListener = {
    &roundtripDone,
};

bool processRoundtrip(wl_display *display)
{
    if (!QCoreApplication::instance())
        return wl_display_roundtrip(display) >= 0;

    RoundtripState state;
    wl_callback *callback = wl_display_sync(display);
    if (!callback
        || wl_callback_add_listener(callback, &kRoundtripListener, &state) != 0
        || wl_display_flush(display) < 0) {
        if (callback)
            wl_callback_destroy(callback);
        return false;
    }

    QElapsedTimer timeout;
    timeout.start();
    while (!state.done && timeout.elapsed() < 5000) {
        if (wl_display_dispatch_pending(display) < 0)
            return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    return state.done;
}

void setDiagnostic(QString *diagnostic, const QString &message)
{
    if (diagnostic)
        *diagnostic = message;
}

QString capabilityPath()
{
    const QString explicitPath = qEnvironmentVariable("ASTREA_SHELL_CAPABILITY_FILE");
    if (!explicitPath.isEmpty())
        return explicitPath;
    const QString runtimeDirectory = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtimeDirectory.isEmpty())
        return {};
    return runtimeDirectory + QStringLiteral("/astrea-shell/capability");
}

bool readCapability(QByteArray *capability, QString *diagnostic)
{
    const QString path = capabilityPath();
    if (path.isEmpty()) {
        setDiagnostic(diagnostic, QStringLiteral("Astrea shell capability handoff is unavailable"));
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setDiagnostic(diagnostic, QStringLiteral("Astrea shell capability handoff is unavailable"));
        return false;
    }
    *capability = file.read(kCapabilityLength + 1);
    if (!file.atEnd()) {
        setDiagnostic(diagnostic, QStringLiteral("Astrea shell capability is invalid"));
        return false;
    }
    if (capability->endsWith('\n'))
        capability->chop(1);
    if (capability->size() != kCapabilityLength) {
        setDiagnostic(diagnostic, QStringLiteral("Astrea shell capability is invalid"));
        return false;
    }
    for (const char value : *capability) {
        const bool hexadecimal = (value >= '0' && value <= '9')
            || (value >= 'a' && value <= 'f');
        if (!hexadecimal) {
            setDiagnostic(diagnostic, QStringLiteral("Astrea shell capability is invalid"));
            return false;
        }
    }
    return true;
}

} // namespace

bool TyphonShellAuthenticator::authenticate(wl_display *display, QString *diagnostic)
{
    if (!display) {
        setDiagnostic(diagnostic, QStringLiteral("Typhon Wayland display is unavailable"));
        return false;
    }

    QByteArray capability;
    if (!readCapability(&capability, diagnostic))
        return false;

    AuthenticationState state;
    wl_registry *registry = wl_display_get_registry(display);
    if (!registry || wl_registry_add_listener(registry, &kRegistryListener, &state) != 0) {
        if (registry)
            wl_registry_destroy(registry);
        setDiagnostic(diagnostic, QStringLiteral("Astrea shell authentication registry failed"));
        return false;
    }

    if (!processRoundtrip(display) || !state.manager) {
        if (state.manager)
            astrea_shell_auth_manager_v1_destroy(state.manager);
        wl_registry_destroy(registry);
        setDiagnostic(diagnostic, QStringLiteral("Astrea shell authentication is unavailable"));
        return false;
    }

    if (astrea_shell_auth_manager_v1_add_listener(
            state.manager, &kAuthenticationListener, &state)
        != 0) {
        astrea_shell_auth_manager_v1_destroy(state.manager);
        wl_registry_destroy(registry);
        setDiagnostic(diagnostic, QStringLiteral("Astrea shell authentication setup failed"));
        return false;
    }
    astrea_shell_auth_manager_v1_authenticate(state.manager, capability.constData());
    const bool roundtripSucceeded = processRoundtrip(display);
    astrea_shell_auth_manager_v1_destroy(state.manager);
    wl_registry_destroy(registry);

    if (!roundtripSucceeded || !state.authenticated || state.rejected) {
        setDiagnostic(diagnostic, QStringLiteral("Astrea shell capability was rejected"));
        return false;
    }
    return true;
}

} // namespace Astrea::Typhon

#else

namespace Astrea::Typhon {

bool TyphonShellAuthenticator::authenticate(wl_display *, QString *diagnostic)
{
    if (diagnostic)
        *diagnostic = QStringLiteral("Typhon shell authentication is unavailable in this build");
    return false;
}

} // namespace Astrea::Typhon

#endif
