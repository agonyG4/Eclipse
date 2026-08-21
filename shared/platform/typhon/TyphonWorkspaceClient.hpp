#pragma once

#include "platform/typhon/TyphonWorkspaceState.hpp"

#include <QObject>
#include <QString>
#include <QVector>

#include <cstdint>
#include <memory>

class TyphonSharedConnection;

#if ASTREA_HAVE_TYPHON_PROTOCOL
struct ext_workspace_group_handle_v1;
struct ext_workspace_handle_v1;
struct ext_workspace_manager_v1;
struct wl_array;
struct wl_output;
struct wl_registry;
#endif

class TyphonWorkspaceClient final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool activationAvailable READ activationAvailable NOTIFY activationAvailableChanged)

public:
    explicit TyphonWorkspaceClient(TyphonSharedConnection *sharedConnection,
                                   QObject *parent = nullptr);
    ~TyphonWorkspaceClient() override;

    void start();
    void stop();

    bool activationAvailable() const { return m_activationAvailable; }
    std::uint64_t connectionGeneration() const { return m_generation; }
    const QVector<TyphonWorkspaceRecord> &workspaces() const
    { return m_state.committedWorkspaces(); }

    bool activateWorkspace(const QString &stableId);

signals:
    void activationAvailableChanged();
    void snapshotChanged(QVector<TyphonWorkspaceRecord> workspaces);
    void diagnostic(QString message);

private:
#if ASTREA_HAVE_TYPHON_PROTOCOL
    friend void managerWorkspaceGroup(void *, ext_workspace_manager_v1 *,
                                      ext_workspace_group_handle_v1 *);
    friend void registryGlobal(void *, wl_registry *, std::uint32_t, const char *, std::uint32_t);
    friend void managerWorkspace(void *, ext_workspace_manager_v1 *, ext_workspace_handle_v1 *);
    friend void managerDone(void *, ext_workspace_manager_v1 *);
    friend void managerFinished(void *, ext_workspace_manager_v1 *);
    friend QString handleId(TyphonWorkspaceClient *, ext_workspace_handle_v1 *);
    friend void groupRemoved(void *, ext_workspace_group_handle_v1 *);
    friend void workspaceId(void *, ext_workspace_handle_v1 *, const char *);
    friend void workspaceName(void *, ext_workspace_handle_v1 *, const char *);
    friend void workspaceCoordinates(void *, ext_workspace_handle_v1 *, wl_array *);
    friend void workspaceState(void *, ext_workspace_handle_v1 *, std::uint32_t);
    friend void workspaceCapabilities(void *, ext_workspace_handle_v1 *, std::uint32_t);
    friend void workspaceRemoved(void *, ext_workspace_handle_v1 *);
#endif
    struct Private;

    void beginGeneration(std::uint64_t generation);
    void handleDisconnected(std::uint64_t generation);
    void clearGeneration(std::uint64_t generation);
    void publishDone(std::uint64_t generation);
    void setActivationAvailable(bool available);
#if ASTREA_HAVE_TYPHON_PROTOCOL
    void destroyProtocolObjects();
#endif

    std::unique_ptr<Private> m_private;
    TyphonSharedConnection *m_sharedConnection = nullptr;
    TyphonWorkspaceState m_state;
    std::uint64_t m_generation = 0;
    bool m_started = false;
    bool m_activationAvailable = false;
};

class TyphonWorkspaceController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool activationAvailable READ activationAvailable NOTIFY activationAvailableChanged)

public:
    explicit TyphonWorkspaceController(TyphonWorkspaceClient *client,
                                       QObject *parent = nullptr);

    bool activationAvailable() const;
    Q_INVOKABLE bool activateWorkspace(const QString &stableId);

signals:
    void activationAvailableChanged();

private:
    TyphonWorkspaceClient *m_client = nullptr;
};
