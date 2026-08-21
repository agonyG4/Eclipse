#include "platform/typhon/TyphonWorkspaceClient.hpp"

#include "platform/typhon/TyphonSharedConnection.hpp"

#if ASTREA_HAVE_TYPHON_PROTOCOL
#include "ext-workspace-v1-client-protocol.h"

#include <wayland-client-core.h>
#include <wayland-util.h>
#endif

#include <algorithm>
#include <cstring>
#include <utility>

#include <QHash>

struct TyphonWorkspaceClient::Private {
#if ASTREA_HAVE_TYPHON_PROTOCOL
    wl_registry *registry = nullptr;
    ext_workspace_manager_v1 *manager = nullptr;
    ext_workspace_group_handle_v1 *group = nullptr;
    QHash<ext_workspace_handle_v1 *, QString> handleIds;
    QHash<QString, ext_workspace_handle_v1 *> handlesById;
    QHash<QString, bool> activationById;
#endif
};

#if ASTREA_HAVE_TYPHON_PROTOCOL
void managerWorkspaceGroup(void *data, ext_workspace_manager_v1 *,
                           ext_workspace_group_handle_v1 *group);
void managerWorkspace(void *data, ext_workspace_manager_v1 *, ext_workspace_handle_v1 *workspace);
void managerDone(void *data, ext_workspace_manager_v1 *);
void managerFinished(void *data, ext_workspace_manager_v1 *);

const ext_workspace_manager_v1_listener kManagerListener{
    &managerWorkspaceGroup,
    &managerWorkspace,
    &managerDone,
    &managerFinished,
};

void groupCapabilities(void *, ext_workspace_group_handle_v1 *, uint32_t);
void groupOutputEnter(void *, ext_workspace_group_handle_v1 *, wl_output *);
void groupOutputLeave(void *, ext_workspace_group_handle_v1 *, wl_output *);
void groupWorkspaceEnter(void *, ext_workspace_group_handle_v1 *, ext_workspace_handle_v1 *);
void groupWorkspaceLeave(void *, ext_workspace_group_handle_v1 *, ext_workspace_handle_v1 *);
void groupRemoved(void *data, ext_workspace_group_handle_v1 *group);

const ext_workspace_group_handle_v1_listener kGroupListener{
    &groupCapabilities,
    &groupOutputEnter,
    &groupOutputLeave,
    &groupWorkspaceEnter,
    &groupWorkspaceLeave,
    &groupRemoved,
};

void workspaceId(void *data, ext_workspace_handle_v1 *workspace, const char *id);
void workspaceName(void *data, ext_workspace_handle_v1 *workspace, const char *name);
void workspaceCoordinates(void *data, ext_workspace_handle_v1 *, wl_array *coordinates);
void workspaceState(void *data, ext_workspace_handle_v1 *workspace, uint32_t state);
void workspaceCapabilities(void *data, ext_workspace_handle_v1 *workspace, uint32_t capabilities);
void workspaceRemoved(void *data, ext_workspace_handle_v1 *workspace);

const ext_workspace_handle_v1_listener kWorkspaceListener{
    &workspaceId,
    &workspaceName,
    &workspaceCoordinates,
    &workspaceState,
    &workspaceCapabilities,
    &workspaceRemoved,
};

void registryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface,
                    uint32_t version)
{
    auto *client = static_cast<TyphonWorkspaceClient *>(data);
    if (std::strcmp(interface, ext_workspace_manager_v1_interface.name) != 0)
        return;
    const uint32_t bindVersion = std::min(version, 1u);
    if (client->m_private->manager)
        return;
    client->m_private->manager = static_cast<ext_workspace_manager_v1 *>(
        wl_registry_bind(registry, name, &ext_workspace_manager_v1_interface, bindVersion));
    ext_workspace_manager_v1_add_listener(client->m_private->manager, &kManagerListener, client);
}

void registryGlobalRemove(void *, wl_registry *, uint32_t)
{
}

const wl_registry_listener kRegistryListener{&registryGlobal, &registryGlobalRemove};

QString handleId(TyphonWorkspaceClient *client, ext_workspace_handle_v1 *workspace)
{
    return client->m_private->handleIds.value(workspace);
}

void managerWorkspaceGroup(void *data, ext_workspace_manager_v1 *,
                           ext_workspace_group_handle_v1 *group)
{
    auto *client = static_cast<TyphonWorkspaceClient *>(data);
    client->m_private->group = group;
    ext_workspace_group_handle_v1_add_listener(group, &kGroupListener, client);
}

void managerWorkspace(void *data, ext_workspace_manager_v1 *, ext_workspace_handle_v1 *workspace)
{
    auto *client = static_cast<TyphonWorkspaceClient *>(data);
    ext_workspace_handle_v1_add_listener(workspace, &kWorkspaceListener, client);
}

void managerDone(void *data, ext_workspace_manager_v1 *)
{
    auto *client = static_cast<TyphonWorkspaceClient *>(data);
    client->publishDone(client->m_generation);
}

void managerFinished(void *data, ext_workspace_manager_v1 *)
{
    auto *client = static_cast<TyphonWorkspaceClient *>(data);
    client->m_private->manager = nullptr;
    client->clearGeneration(client->m_generation);
}

void groupCapabilities(void *, ext_workspace_group_handle_v1 *, uint32_t)
{
}

void groupOutputEnter(void *, ext_workspace_group_handle_v1 *, wl_output *)
{
}

void groupOutputLeave(void *, ext_workspace_group_handle_v1 *, wl_output *)
{
}

void groupWorkspaceEnter(void *, ext_workspace_group_handle_v1 *, ext_workspace_handle_v1 *)
{
}

void groupWorkspaceLeave(void *, ext_workspace_group_handle_v1 *, ext_workspace_handle_v1 *)
{
}

void groupRemoved(void *data, ext_workspace_group_handle_v1 *group)
{
    auto *client = static_cast<TyphonWorkspaceClient *>(data);
    if (client->m_private->group == group)
        client->m_private->group = nullptr;
}

void workspaceId(void *data, ext_workspace_handle_v1 *workspace, const char *id)
{
    auto *client = static_cast<TyphonWorkspaceClient *>(data);
    const QString stableId = QString::fromUtf8(id ? id : "");
    if (stableId.isEmpty())
        return;
    client->m_private->handleIds.insert(workspace, stableId);
    client->m_private->handlesById.insert(stableId, workspace);
    client->m_state.beginWorkspace(stableId);
}

void workspaceName(void *data, ext_workspace_handle_v1 *workspace, const char *name)
{
    auto *client = static_cast<TyphonWorkspaceClient *>(data);
    const QString stableId = handleId(client, workspace);
    if (stableId.isEmpty())
        return;
    client->m_state.beginWorkspace(stableId);
    client->m_state.setWorkspaceName(QString::fromUtf8(name ? name : ""));
}

void workspaceCoordinates(void *data, ext_workspace_handle_v1 *workspace, wl_array *coordinates)
{
    auto *client = static_cast<TyphonWorkspaceClient *>(data);
    const QString stableId = handleId(client, workspace);
    if (stableId.isEmpty())
        return;
    QVector<std::uint32_t> values;
    const auto *bytes = static_cast<const unsigned char *>(coordinates->data);
    for (std::size_t offset = 0; offset + sizeof(std::uint32_t) <= coordinates->size;
         offset += sizeof(std::uint32_t)) {
        std::uint32_t value = 0;
        std::memcpy(&value, bytes + offset, sizeof(value));
        values.append(value);
    }
    client->m_state.beginWorkspace(stableId);
    client->m_state.setWorkspaceCoordinates(std::move(values));
}

void workspaceState(void *data, ext_workspace_handle_v1 *workspace, uint32_t state)
{
    auto *client = static_cast<TyphonWorkspaceClient *>(data);
    const QString stableId = handleId(client, workspace);
    if (stableId.isEmpty())
        return;
    client->m_state.beginWorkspace(stableId);
    client->m_state.setWorkspaceState(state);
}

void workspaceCapabilities(void *data, ext_workspace_handle_v1 *workspace, uint32_t capabilities)
{
    auto *client = static_cast<TyphonWorkspaceClient *>(data);
    const QString stableId = handleId(client, workspace);
    if (stableId.isEmpty())
        return;
    client->m_private->activationById.insert(stableId, (capabilities & 1u) != 0);
    client->m_state.beginWorkspace(stableId);
    client->m_state.setWorkspaceCapabilities(capabilities);
}

void workspaceRemoved(void *data, ext_workspace_handle_v1 *workspace)
{
    auto *client = static_cast<TyphonWorkspaceClient *>(data);
    const QString stableId = client->m_private->handleIds.take(workspace);
    if (!stableId.isEmpty()) {
        client->m_private->handlesById.remove(stableId);
        client->m_private->activationById.remove(stableId);
    }
}

#endif

TyphonWorkspaceClient::TyphonWorkspaceClient(TyphonSharedConnection *sharedConnection,
                                             QObject *parent)
    : QObject(parent), m_private(std::make_unique<Private>()), m_sharedConnection(sharedConnection)
{
    if (!m_sharedConnection)
        return;
    connect(m_sharedConnection, &TyphonSharedConnection::ready, this,
            [this](std::uint64_t generation) { beginGeneration(generation); });
    connect(m_sharedConnection, &TyphonSharedConnection::disconnected, this,
            [this](std::uint64_t generation) { handleDisconnected(generation); });
}

TyphonWorkspaceClient::~TyphonWorkspaceClient()
{
    stop();
}

void TyphonWorkspaceClient::start()
{
    if (m_started)
        return;
    m_started = true;
    if (!m_sharedConnection)
        return;
    if (m_sharedConnection->isReady())
        beginGeneration(m_sharedConnection->connectionGeneration());
    else if (m_sharedConnection->state() == TyphonSharedConnection::State::Stopped)
        m_sharedConnection->start();
}

void TyphonWorkspaceClient::stop()
{
    if (!m_started)
        return;
    m_started = false;
#if ASTREA_HAVE_TYPHON_PROTOCOL
    destroyProtocolObjects();
#endif
    clearGeneration(m_generation);
}

void TyphonWorkspaceClient::beginGeneration(std::uint64_t generation)
{
    if (!m_started || generation == 0)
        return;
#if ASTREA_HAVE_TYPHON_PROTOCOL
    destroyProtocolObjects();
#endif
    clearGeneration(m_generation);
    m_generation = generation;
    m_state.beginGeneration(generation);
#if ASTREA_HAVE_TYPHON_PROTOCOL
    if (!m_sharedConnection->nativeDisplay())
        return;
    m_private->registry = wl_display_get_registry(m_sharedConnection->nativeDisplay());
    wl_registry_add_listener(m_private->registry, &kRegistryListener, this);
    m_sharedConnection->flush();
#else
    emit diagnostic(QStringLiteral("Typhon workspace protocol is unavailable in this build"));
#endif
    setActivationAvailable(false);
}

void TyphonWorkspaceClient::handleDisconnected(std::uint64_t generation)
{
    if (!m_started || generation != m_generation)
        return;
    clearGeneration(generation);
}

void TyphonWorkspaceClient::clearGeneration(std::uint64_t generation)
{
    if (generation == 0 || generation != m_generation)
        return;
#if ASTREA_HAVE_TYPHON_PROTOCOL
    m_private->manager = nullptr;
    m_private->group = nullptr;
    m_private->registry = nullptr;
    m_private->handleIds.clear();
    m_private->handlesById.clear();
    m_private->activationById.clear();
#endif
    m_state.clear(generation);
    setActivationAvailable(false);
    emit snapshotChanged({});
}

#if ASTREA_HAVE_TYPHON_PROTOCOL
void TyphonWorkspaceClient::destroyProtocolObjects()
{
    for (auto *workspace : std::as_const(m_private->handlesById))
        ext_workspace_handle_v1_destroy(workspace);
    m_private->handleIds.clear();
    m_private->handlesById.clear();
    m_private->activationById.clear();
    if (m_private->group)
        ext_workspace_group_handle_v1_destroy(m_private->group);
    if (m_private->manager)
        ext_workspace_manager_v1_destroy(m_private->manager);
    if (m_private->registry)
        wl_registry_destroy(m_private->registry);
    m_private->group = nullptr;
    m_private->manager = nullptr;
    m_private->registry = nullptr;
}
#endif

void TyphonWorkspaceClient::publishDone(std::uint64_t generation)
{
    if (generation != m_generation || !m_state.commitDone(generation))
        return;
    const bool canActivate = std::any_of(
        m_state.committedWorkspaces().cbegin(), m_state.committedWorkspaces().cend(),
        [](const TyphonWorkspaceRecord &workspace) { return workspace.activationAvailable; });
    setActivationAvailable(canActivate);
    emit snapshotChanged(m_state.committedWorkspaces());
}

void TyphonWorkspaceClient::setActivationAvailable(bool available)
{
    if (m_activationAvailable == available)
        return;
    m_activationAvailable = available;
    emit activationAvailableChanged();
}

bool TyphonWorkspaceClient::activateWorkspace(const QString &stableId)
{
#if ASTREA_HAVE_TYPHON_PROTOCOL
    if (!m_started || !m_activationAvailable || !m_private->manager)
        return false;
    auto *workspace = m_private->handlesById.value(stableId);
    if (!workspace || !m_private->activationById.value(stableId, false))
        return false;
    ext_workspace_handle_v1_activate(workspace);
    ext_workspace_manager_v1_commit(m_private->manager);
    return m_sharedConnection->flush();
#else
    Q_UNUSED(stableId)
    return false;
#endif
}

TyphonWorkspaceController::TyphonWorkspaceController(TyphonWorkspaceClient *client, QObject *parent)
    : QObject(parent), m_client(client)
{
    if (m_client)
        connect(m_client, &TyphonWorkspaceClient::activationAvailableChanged, this,
                &TyphonWorkspaceController::activationAvailableChanged);
}

bool TyphonWorkspaceController::activationAvailable() const
{
    return m_client && m_client->activationAvailable();
}

bool TyphonWorkspaceController::activateWorkspace(const QString &stableId)
{
    return m_client && m_client->activateWorkspace(stableId);
}
