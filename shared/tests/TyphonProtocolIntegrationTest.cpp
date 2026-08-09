#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QSocketNotifier>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <functional>
#include <memory>

#include <wayland-client-core.h>
#include <wayland-server-core.h>
#include "astrea-shell-auth-v1-server-protocol.h"
#include "astrea-toplevel-management-v1-server-protocol.h"

#include "platform/typhon/TyphonToplevelConnection.hpp"
#include "platform/typhon/TyphonSharedConnection.hpp"

using namespace Astrea::Typhon;

class FakeTyphonCompositor final : public QObject {
public:
    struct ServerWindow {
        FakeTyphonCompositor *owner = nullptr;
        wl_resource *resource = nullptr;
        QString id;
    };

    struct ActionRequest {
        ServerWindow *window = nullptr;
        wl_client *client = nullptr;
        quint32 action = 0;
        quint32 tokenHi = 0;
        quint32 tokenLo = 0;
    };

    explicit FakeTyphonCompositor(bool advertiseManager, quint32 managerVersion = 1,
                                  bool authenticate = true,
                                  bool authenticateOnlyFirstClient = false,
                                  QObject *parent = nullptr)
        : QObject(parent)
        , m_managerAdvertisedVersion(managerVersion)
        , m_authenticate(authenticate)
        , m_authenticateOnlyFirstClient(authenticateOnlyFirstClient)
    {
        QVERIFY(m_runtime.isValid());
        m_previousRuntime = qgetenv("XDG_RUNTIME_DIR");
        m_previousDisplay = qgetenv("WAYLAND_DISPLAY");
        m_previousCapability = qgetenv("ASTREA_SHELL_CAPABILITY_FILE");
        qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
        m_socketName = QStringLiteral("wayland-typhon-m6");
        qputenv("WAYLAND_DISPLAY", m_socketName.toUtf8());
        m_capabilityPath = m_runtime.filePath(QStringLiteral("capability"));
        QFile capability(m_capabilityPath);
        QVERIFY(capability.open(QIODevice::WriteOnly));
        QVERIFY(capability.write(QByteArray(64, 'a') + '\n') == 65);
        capability.close();
        qputenv("ASTREA_SHELL_CAPABILITY_FILE", m_capabilityPath.toUtf8());

        m_display = wl_display_create();
        QVERIFY(m_display);
        m_loop = wl_display_get_event_loop(m_display);
        QVERIFY(m_loop);
        QCOMPARE(wl_display_add_socket(m_display, m_socketName.toUtf8().constData()), 0);
        m_serverNotifier = new QSocketNotifier(wl_event_loop_get_fd(m_loop), QSocketNotifier::Read, this);
        connect(m_serverNotifier, &QSocketNotifier::activated, this,
                [this](QSocketDescriptor, QSocketNotifier::Type) { dispatchServerEvents(); });
        m_authGlobal = wl_global_create(m_display, &astrea_shell_auth_manager_v1_interface, 1,
                                        this, &bindAuthManager);
        QVERIFY(m_authGlobal);
        if (advertiseManager)
            m_global = wl_global_create(m_display, &astrea_toplevel_manager_v1_interface,
                                        m_managerAdvertisedVersion, this, &bindManager);
        QVERIFY(advertiseManager ? m_global != nullptr : true);
    }

    ~FakeTyphonCompositor() override
    {
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

    bool pumpUntil(const std::function<bool()> &condition, int maxTurns = 500)
    {
        for (int turn = 0; turn < maxTurns; ++turn) {
            if (condition())
                return true;
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        }
        return condition();
    }

    ServerWindow *beginWindow(const QString &id)
    {
        if (!m_manager)
            return nullptr;
        auto *window = new ServerWindow;
        window->owner = this;
        window->id = id;
        window->resource = wl_resource_create(wl_resource_get_client(m_manager->resource),
                                               &astrea_toplevel_v1_interface,
                                               wl_resource_get_version(m_manager->resource), 0);
        if (!window->resource) {
            delete window;
            return nullptr;
        }
        wl_resource_set_implementation(window->resource, &kWindowImplementation, window, &destroyWindow);
        m_windows.append(window);
        astrea_toplevel_manager_v1_send_toplevel(m_manager->resource, window->resource);
        astrea_toplevel_v1_send_identifier(window->resource, id.toUtf8().constData());
        return window;
    }

    void sendMetadata(ServerWindow *window, const QString &appId, const QString &title,
                      quint32 pid, quint32 kind, quint32 state, FocusSerial focusSerial)
    {
        if (!window || !window->resource)
            return;
        astrea_toplevel_v1_send_app_id(window->resource, appId.toUtf8().constData());
        astrea_toplevel_v1_send_title(window->resource, title.toUtf8().constData());
        astrea_toplevel_v1_send_pid(window->resource, pid);
        astrea_toplevel_v1_send_kind(window->resource, kind);
        astrea_toplevel_v1_send_state(window->resource, state);
        astrea_toplevel_v1_send_focus_serial(window->resource,
                                             static_cast<uint32_t>(focusSerial >> 32),
                                             static_cast<uint32_t>(focusSerial));
        flushClients();
    }

    void sendDone(ServerWindow *window, Revision revision)
    {
        if (!window || !window->resource)
            return;
        astrea_toplevel_v1_send_done(window->resource,
                                      static_cast<uint32_t>(revision >> 32),
                                      static_cast<uint32_t>(revision));
        flushClients();
    }

    void sendClosed(ServerWindow *window)
    {
        if (!window || !window->resource)
            return;
        astrea_toplevel_v1_send_closed(window->resource);
        flushClients();
    }

    void sendManagerDone(Revision revision, quint32 total, bool truncated = false)
    {
        if (!m_manager)
            return;
        astrea_toplevel_manager_v1_send_done(m_manager->resource,
                                             static_cast<uint32_t>(revision >> 32),
                                             static_cast<uint32_t>(revision), total,
                                             truncated ? ASTREA_TOPLEVEL_MANAGER_V1_DONE_FLAGS_TRUNCATED : 0);
        flushClients();
    }

    void sendUnknownKind(ServerWindow *window)
    {
        if (!window || !window->resource)
            return;
        astrea_toplevel_v1_send_kind(window->resource, 99);
        flushClients();
    }

    void sendManagerFailure(quint32 reason)
    {
        if (!m_manager)
            return;
        astrea_toplevel_manager_v1_send_failed(m_manager->resource, reason);
        flushClients();
    }

    void sendActionDone(const ActionRequest &request, quint32 result)
    {
        if (!m_manager || !request.window)
            return;
        astrea_toplevel_manager_v1_send_action_done(
            m_manager->resource, request.tokenHi, request.tokenLo,
            request.action, result);
        flushClients();
    }

    void disconnectClients()
    {
        if (m_display)
            wl_display_destroy_clients(m_display);
    }

    wl_display *connectIndependentClient() const
    {
        return wl_display_connect(m_socketName.toUtf8().constData());
    }

    int clientCount() const
    {
        int count = 0;
        wl_client *client = nullptr;
        wl_client_for_each(client, wl_display_get_client_list(m_display))
            ++count;
        return count;
    }

    int liveWindowCount() const
    {
        return m_windows.size();
    }

    const QVector<ActionRequest> &actionRequests() const
    {
        return m_actionRequests;
    }

    wl_client *authenticatedClient() const { return m_authenticatedClient; }
    wl_client *managerClient() const { return m_managerClient; }
    wl_client *lastRejectedClient() const { return m_lastRejectedClient; }
    int authenticationCount() const { return m_authenticatedClients.size(); }

private:
    struct AuthenticatedClientRecord {
        FakeTyphonCompositor *owner = nullptr;
        wl_client *client = nullptr;
        wl_listener listener{};
    };

    static void authenticatedClientDestroyed(wl_listener *listener, void *)
    {
        AuthenticatedClientRecord *record = wl_container_of(
            listener, static_cast<AuthenticatedClientRecord *>(nullptr), listener);
        if (record->owner->m_authenticatedClient == record->client)
            record->owner->m_authenticatedClient = nullptr;
        delete record;
    }

    static void bindAuthManager(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *self = static_cast<FakeTyphonCompositor *>(data);
        auto *manager = new ServerWindow;
        manager->owner = self;
        manager->resource = wl_resource_create(client, &astrea_shell_auth_manager_v1_interface,
                                               std::min(version, 1u), id);
        if (!manager->resource) {
            delete manager;
            return;
        }
        wl_resource_set_implementation(manager->resource, &kAuthManagerImplementation, manager,
                                       &destroyAuthManager);
        self->m_authManager = manager;
    }

    static void destroyAuthManager(wl_resource *resource)
    {
        auto *manager = static_cast<ServerWindow *>(wl_resource_get_user_data(resource));
        if (manager && manager->owner) {
            auto *owner = manager->owner;
            if (owner->m_authManager == manager)
                owner->m_authManager = nullptr;
        }
        delete manager;
    }

    static void authenticateRequest(wl_client *client, wl_resource *resource, const char *capability)
    {
        auto *manager = static_cast<ServerWindow *>(wl_resource_get_user_data(resource));
        const QByteArray value = QByteArray(capability ? capability : "");
        bool valid = value.size() == 64;
        for (const char byte : value) {
            valid = valid && ((byte >= '0' && byte <= '9')
                              || (byte >= 'a' && byte <= 'f'));
        }
        auto *owner = manager ? manager->owner : nullptr;
        const bool allowed = valid && owner && owner->m_authenticate
            && (!owner->m_authenticateOnlyFirstClient || owner->m_authenticatedClient == nullptr);
        if (allowed) {
            owner->m_authenticatedClient = client;
            owner->m_authenticatedClients.append(client);
            auto *record = new AuthenticatedClientRecord;
            record->owner = owner;
            record->client = client;
            record->listener.notify = &authenticatedClientDestroyed;
            wl_client_add_destroy_listener(client, &record->listener);
            astrea_shell_auth_manager_v1_send_authenticated(resource);
        } else {
            if (owner)
                owner->m_lastRejectedClient = client;
            astrea_shell_auth_manager_v1_send_rejected(resource);
        }
    }

    static void destroyAuthManagerRequest(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    static void bindManager(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *self = static_cast<FakeTyphonCompositor *>(data);
        auto *manager = new ServerWindow;
        manager->owner = self;
        manager->resource = wl_resource_create(client, &astrea_toplevel_manager_v1_interface,
                                               std::min(version, self->m_managerAdvertisedVersion), id);
        if (!manager->resource) {
            delete manager;
            return;
        }
        self->m_managerClient = client;
        wl_resource_set_implementation(manager->resource, &kManagerImplementation,
                                       manager, &destroyManager);
        self->m_manager = manager;
    }

    static void destroyManager(wl_resource *resource)
    {
        auto *manager = static_cast<ServerWindow *>(wl_resource_get_user_data(resource));
        if (manager && manager->owner) {
            auto *owner = manager->owner;
            if (owner->m_manager == manager)
                owner->m_manager = nullptr;
            if (owner->m_managerClient == wl_resource_get_client(resource))
                owner->m_managerClient = nullptr;
        }
        delete manager;
    }

    static void destroyWindow(wl_resource *resource)
    {
        auto *window = static_cast<ServerWindow *>(wl_resource_get_user_data(resource));
        if (!window)
            return;
        auto *owner = window->owner;
        if (owner)
            owner->m_windows.removeAll(window);
        delete window;
    }

    static void destroyManagerRequest(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    static void destroyWindowRequest(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    static void activateRequest(wl_client *, wl_resource *resource,
                                uint32_t tokenHi, uint32_t tokenLo)
    {
        recordAction(resource, ASTREA_TOPLEVEL_MANAGER_V1_ACTION_ACTIVATE, tokenHi, tokenLo);
    }

    static void minimizeRequest(wl_client *, wl_resource *resource,
                                uint32_t tokenHi, uint32_t tokenLo)
    {
        recordAction(resource, ASTREA_TOPLEVEL_MANAGER_V1_ACTION_MINIMIZE, tokenHi, tokenLo);
    }

    static void restoreRequest(wl_client *, wl_resource *resource,
                               uint32_t tokenHi, uint32_t tokenLo)
    {
        recordAction(resource, ASTREA_TOPLEVEL_MANAGER_V1_ACTION_RESTORE, tokenHi, tokenLo);
    }

    static void closeRequest(wl_client *, wl_resource *resource,
                             uint32_t tokenHi, uint32_t tokenLo)
    {
        recordAction(resource, ASTREA_TOPLEVEL_MANAGER_V1_ACTION_CLOSE, tokenHi, tokenLo);
    }

    static void recordAction(wl_resource *resource, quint32 action,
                             quint32 tokenHi, quint32 tokenLo)
    {
        auto *window = static_cast<ServerWindow *>(wl_resource_get_user_data(resource));
        if (!window || !window->owner)
            return;
        window->owner->m_actionRequests.append(
            {window, wl_resource_get_client(resource), action, tokenHi, tokenLo});
    }

    void dispatchServerEvents()
    {
        if (!m_loop || wl_event_loop_dispatch(m_loop, 0) < 0)
            return;
        flushClients();
    }

    void flushClients()
    {
        if (m_display)
            wl_display_flush_clients(m_display);
    }

    static void restoreEnvironment(const char *name, const QByteArray &previous)
    {
        if (previous.isNull())
            qunsetenv(name);
        else
            qputenv(name, previous);
    }

    static const struct astrea_toplevel_manager_v1_interface kManagerImplementation;
    static const struct astrea_toplevel_v1_interface kWindowImplementation;
    static const struct astrea_shell_auth_manager_v1_interface kAuthManagerImplementation;

    QTemporaryDir m_runtime;
    QByteArray m_previousRuntime;
    QByteArray m_previousDisplay;
    QByteArray m_previousCapability;
    QString m_socketName;
    QString m_capabilityPath;
    wl_display *m_display = nullptr;
    wl_event_loop *m_loop = nullptr;
    wl_global *m_global = nullptr;
    wl_global *m_authGlobal = nullptr;
    ServerWindow *m_manager = nullptr;
    ServerWindow *m_authManager = nullptr;
    wl_client *m_authenticatedClient = nullptr;
    wl_client *m_managerClient = nullptr;
    wl_client *m_lastRejectedClient = nullptr;
    QVector<wl_client *> m_authenticatedClients;
    QVector<ServerWindow *> m_windows;
    QVector<ActionRequest> m_actionRequests;
    quint32 m_managerAdvertisedVersion = 1;
    bool m_authenticate = true;
    bool m_authenticateOnlyFirstClient = false;
    QSocketNotifier *m_serverNotifier = nullptr;
};

const struct astrea_toplevel_manager_v1_interface FakeTyphonCompositor::kManagerImplementation = {
    &FakeTyphonCompositor::destroyManagerRequest
};

const struct astrea_toplevel_v1_interface FakeTyphonCompositor::kWindowImplementation = {
    &FakeTyphonCompositor::destroyWindowRequest,
    &FakeTyphonCompositor::activateRequest,
    &FakeTyphonCompositor::minimizeRequest,
    &FakeTyphonCompositor::restoreRequest,
    &FakeTyphonCompositor::closeRequest
};

const struct astrea_shell_auth_manager_v1_interface
    FakeTyphonCompositor::kAuthManagerImplementation = {
        &FakeTyphonCompositor::authenticateRequest,
        &FakeTyphonCompositor::destroyAuthManagerRequest,
    };

class TyphonProtocolIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void registryDiscoveryAndInitialSnapshot();
    void splitRevisionRemainsAtomic();
    void incrementalRevisionsOnOneLiveHandle();
    void hundredIncrementalMetadataStateRevisionsOnOneLiveProtocolHandle();
    void twoHundredFiftySevenWindowRevisionCommitsOnce();
    void managerFailureDegradesWithoutCrashing();
    void missingGlobalIsUnsupported();
    void unknownKindDegradesConnection();
    void v2ActionUsesSameAuthenticatedConnectionAndManagerCompletion();
    void crossClientAuthenticationCannotBorrowCapability();
    void reconnectRequiresFreshAuthenticationAndClientIdentity();
    void v2WithoutAuthenticationRemainsReadOnly();
    void sharedSessionOwnsToplevelTransport();
};

void TyphonProtocolIntegrationTest::registryDiscoveryAndInitialSnapshot()
{
    FakeTyphonCompositor compositor(true);
    TyphonToplevelConnection connection;
    QSignalSpy snapshots(&connection, &TyphonToplevelConnection::snapshotChanged);
    connection.start();
    QVERIFY(compositor.pumpUntil([&] {
        return connection.state() == TyphonConnectionState::WaitingForInitialSnapshot;
    }));

    auto *window = compositor.beginWindow(QStringLiteral("18446744073709551615"));
    QVERIFY(window);
    compositor.sendMetadata(window, QStringLiteral("org.example.App"), QStringLiteral("Example"),
                            42, ASTREA_TOPLEVEL_V1_KIND_XDG_TOPLEVEL,
                            ASTREA_TOPLEVEL_V1_STATE_ACTIVE, 10);
    compositor.sendDone(window, 1);
    compositor.sendManagerDone(1, 1);
    QVERIFY(compositor.pumpUntil([&] { return connection.state() == TyphonConnectionState::Ready; }));
    QCOMPARE(snapshots.count(), 1);
    QCOMPARE(connection.snapshot().windows.first().id, QStringLiteral("18446744073709551615"));
    QCOMPARE(connection.snapshot().windows.first().pid, quint32(42));
    connection.stop();
}

void TyphonProtocolIntegrationTest::splitRevisionRemainsAtomic()
{
    FakeTyphonCompositor compositor(true);
    TyphonToplevelConnection connection;
    QSignalSpy snapshots(&connection, &TyphonToplevelConnection::snapshotChanged);
    connection.start();
    QVERIFY(compositor.pumpUntil([&] {
        return connection.state() == TyphonConnectionState::WaitingForInitialSnapshot;
    }));
    auto *window = compositor.beginWindow(QStringLiteral("7"));
    QVERIFY(window);
    compositor.pumpUntil([&] { return false; }, 10);
    QCOMPARE(snapshots.count(), 0);
    compositor.sendMetadata(window, QStringLiteral("org.example.App"), QStringLiteral("Split"),
                            7, ASTREA_TOPLEVEL_V1_KIND_XDG_TOPLEVEL, 0, 1);
    compositor.pumpUntil([&] { return false; }, 10);
    QCOMPARE(snapshots.count(), 0);
    compositor.sendDone(window, 9);
    compositor.sendManagerDone(9, 1);
    QVERIFY(compositor.pumpUntil([&] { return snapshots.count() == 1; }));
    QCOMPARE(connection.snapshot().revision, quint64(9));
    connection.stop();
}

void TyphonProtocolIntegrationTest::incrementalRevisionsOnOneLiveHandle()
{
    FakeTyphonCompositor compositor(true);
    TyphonToplevelConnection connection;
    QSignalSpy snapshots(&connection, &TyphonToplevelConnection::snapshotChanged);
    connection.start();
    QVERIFY(compositor.pumpUntil([&] {
        return connection.state() == TyphonConnectionState::WaitingForInitialSnapshot;
    }));

    auto *window = compositor.beginWindow(QStringLiteral("7"));
    QVERIFY(window);
    compositor.sendMetadata(window, QStringLiteral("org.example.App"), QStringLiteral("Initial"),
                            7, ASTREA_TOPLEVEL_V1_KIND_XDG_TOPLEVEL,
                            ASTREA_TOPLEVEL_V1_STATE_ACTIVE, 1);
    compositor.sendDone(window, 1);
    compositor.sendManagerDone(1, 1);
    QVERIFY(compositor.pumpUntil([&] { return snapshots.count() == 1; }));
    QCOMPARE(connection.state(), TyphonConnectionState::Ready);

    compositor.sendMetadata(window, QStringLiteral("org.example.App"), QStringLiteral("Second"),
                            7, ASTREA_TOPLEVEL_V1_KIND_XDG_TOPLEVEL,
                            ASTREA_TOPLEVEL_V1_STATE_MINIMIZED, 2);
    compositor.sendDone(window, 2);
    compositor.pumpUntil([&] { return false; }, 20);
    QCOMPARE(snapshots.count(), 1);
    QCOMPARE(compositor.liveWindowCount(), 1);

    compositor.sendManagerDone(2, 1);
    QVERIFY(compositor.pumpUntil([&] { return snapshots.count() == 2; }));
    QCOMPARE(connection.snapshot().windows.first().title, QStringLiteral("Second"));
    QVERIFY(hasState(connection.snapshot().windows.first().states, ToplevelStateFlag::Minimized));

    compositor.sendMetadata(window, QStringLiteral("org.example.App"), QStringLiteral("Third"),
                            7, ASTREA_TOPLEVEL_V1_KIND_XDG_TOPLEVEL,
                            ASTREA_TOPLEVEL_V1_STATE_ACTIVE, 3);
    compositor.sendDone(window, 3);
    compositor.sendManagerDone(3, 1);
    QVERIFY(compositor.pumpUntil([&] { return snapshots.count() == 3; }));
    QCOMPARE(connection.snapshot().windows.first().title, QStringLiteral("Third"));
    QCOMPARE(compositor.liveWindowCount(), 1);

    compositor.sendClosed(window);
    compositor.sendManagerDone(4, 0);
    QVERIFY(compositor.pumpUntil([&] { return snapshots.count() == 4; }));
    QCOMPARE(connection.snapshot().windows.size(), 0);
    QCOMPARE(connection.state(), TyphonConnectionState::Ready);
    QVERIFY(!connection.reconnectPending());
    QCOMPARE(compositor.liveWindowCount(), 0);
    connection.stop();
}

void TyphonProtocolIntegrationTest::hundredIncrementalMetadataStateRevisionsOnOneLiveProtocolHandle()
{
    FakeTyphonCompositor compositor(true);
    TyphonToplevelConnection connection;
    QSignalSpy snapshots(&connection, &TyphonToplevelConnection::snapshotChanged);
    connection.start();
    QVERIFY(compositor.pumpUntil([&] {
        return connection.state() == TyphonConnectionState::WaitingForInitialSnapshot;
    }));

    auto *window = compositor.beginWindow(QStringLiteral("8"));
    QVERIFY(window);
    compositor.sendMetadata(window, QStringLiteral("org.example.App"), QStringLiteral("Revision 0"),
                            8, ASTREA_TOPLEVEL_V1_KIND_XDG_TOPLEVEL, 0, 0);
    compositor.sendDone(window, 0);
    compositor.sendManagerDone(0, 1);
    QVERIFY(compositor.pumpUntil([&] { return snapshots.count() == 1; }));

    for (Revision revision = 1; revision <= 100; ++revision) {
        compositor.sendMetadata(window, QStringLiteral("org.example.App"),
                                QStringLiteral("Revision %1").arg(revision), 8,
                                ASTREA_TOPLEVEL_V1_KIND_XDG_TOPLEVEL,
                                (revision % 2) ? ASTREA_TOPLEVEL_V1_STATE_ACTIVE : 0,
                                revision);
        compositor.sendDone(window, revision);
        compositor.sendManagerDone(revision, 1);
        QVERIFY(compositor.pumpUntil([&] {
            return snapshots.count() == static_cast<int>(revision + 1);
        }));
        QCOMPARE(connection.snapshot().revision, revision);
    }

    QCOMPARE(snapshots.count(), 101);
    QCOMPARE(connection.state(), TyphonConnectionState::Ready);
    QVERIFY(!connection.reconnectPending());
    QCOMPARE(compositor.liveWindowCount(), 1);

    compositor.sendClosed(window);
    compositor.sendManagerDone(101, 0);
    QVERIFY(compositor.pumpUntil([&] { return snapshots.count() == 102; }));
    QCOMPARE(compositor.liveWindowCount(), 0);
    QCOMPARE(connection.state(), TyphonConnectionState::Ready);
    connection.stop();
}

void TyphonProtocolIntegrationTest::twoHundredFiftySevenWindowRevisionCommitsOnce()
{
    FakeTyphonCompositor compositor(true);
    TyphonToplevelConnection connection;
    QSignalSpy snapshots(&connection, &TyphonToplevelConnection::snapshotChanged);
    connection.start();
    QVERIFY(compositor.pumpUntil([&] {
        return connection.state() == TyphonConnectionState::WaitingForInitialSnapshot;
    }));
    for (int index = 0; index < 257; ++index) {
        auto *window = compositor.beginWindow(QString::number(index + 1));
        QVERIFY(window);
        compositor.sendMetadata(window, QStringLiteral("org.example.App"), QStringLiteral("Stress"),
                                static_cast<quint32>(index + 1), ASTREA_TOPLEVEL_V1_KIND_XDG_TOPLEVEL,
                                0, static_cast<FocusSerial>(index + 1));
        compositor.sendDone(window, 3);
        if ((index + 1) % 16 == 0)
            compositor.pumpUntil([&] { return false; }, 20);
    }
    compositor.sendManagerDone(3, 257);
    QVERIFY(compositor.pumpUntil([&] { return snapshots.count() == 1; }, 2000));
    QCOMPARE(connection.snapshot().windows.size(), 257);
    connection.stop();
}

void TyphonProtocolIntegrationTest::managerFailureDegradesWithoutCrashing()
{
    FakeTyphonCompositor compositor(true);
    TyphonToplevelConnection connection;
    connection.start();
    QVERIFY(compositor.pumpUntil([&] {
        return connection.state() == TyphonConnectionState::WaitingForInitialSnapshot;
    }));
    compositor.sendManagerFailure(ASTREA_TOPLEVEL_MANAGER_V1_FAILURE_REASON_PUBLICATION_FAILURE);
    QVERIFY(compositor.pumpUntil([&] { return connection.state() == TyphonConnectionState::Degraded; }));
    QVERIFY(!connection.hasSnapshot());
    connection.stop();
}

void TyphonProtocolIntegrationTest::missingGlobalIsUnsupported()
{
    FakeTyphonCompositor compositor(false);
    TyphonToplevelConnection connection;
    connection.start();
    QVERIFY(compositor.pumpUntil([&] { return connection.state() == TyphonConnectionState::Unsupported; }));
    connection.stop();
}

void TyphonProtocolIntegrationTest::unknownKindDegradesConnection()
{
    FakeTyphonCompositor compositor(true);
    TyphonToplevelConnection connection;
    connection.start();
    QVERIFY(compositor.pumpUntil([&] {
        return connection.state() == TyphonConnectionState::WaitingForInitialSnapshot;
    }));
    auto *window = compositor.beginWindow(QStringLiteral("1"));
    QVERIFY(window);
    compositor.sendUnknownKind(window);
    QVERIFY(compositor.pumpUntil([&] { return connection.state() == TyphonConnectionState::Degraded; }));
    QVERIFY(connection.reconnectPending());
    connection.stop();
}

void TyphonProtocolIntegrationTest::v2ActionUsesSameAuthenticatedConnectionAndManagerCompletion()
{
    FakeTyphonCompositor compositor(true, 2);
    TyphonToplevelConnection connection;
    QSignalSpy completed(&connection, &TyphonToplevelConnection::actionFinished);
    connection.start();
    QVERIFY(compositor.pumpUntil([&] {
        return connection.state() == TyphonConnectionState::WaitingForInitialSnapshot;
    }));
    QCOMPARE(connection.actionCapability(), TyphonActionCapabilityState::ActionReadyV2);

    auto *window = compositor.beginWindow(QStringLiteral("77"));
    QVERIFY(window);
    compositor.sendMetadata(window, QStringLiteral("org.example.App"), QStringLiteral("Example"),
                            77, ASTREA_TOPLEVEL_V1_KIND_XDG_TOPLEVEL, 0, 77);
    compositor.sendDone(window, 1);
    auto *managedWindow = compositor.beginWindow(QStringLiteral("78"));
    QVERIFY(managedWindow);
    compositor.sendMetadata(managedWindow, QStringLiteral("org.example.X11"),
                            QStringLiteral("Managed"), 78,
                            ASTREA_TOPLEVEL_V1_KIND_X11_TOPLEVEL, 0, 78);
    compositor.sendDone(managedWindow, 1);
    compositor.sendManagerDone(1, 2);
    QVERIFY(compositor.pumpUntil([&] { return connection.state() == TyphonConnectionState::Ready; }));

    QVERIFY(!connection.requestAction(QStringLiteral("77"), ToplevelAction::Activate, 123).has_value());
    QVERIFY(compositor.pumpUntil([&] { return compositor.actionRequests().size() == 1; }));
    const auto request = compositor.actionRequests().first();
    QCOMPARE(request.window->id, QStringLiteral("77"));
    QCOMPARE(request.action, ASTREA_TOPLEVEL_MANAGER_V1_ACTION_ACTIVATE);
    QCOMPARE(compositor.managerClient(), compositor.authenticatedClient());
    QCOMPARE(request.client, compositor.authenticatedClient());
    compositor.sendActionDone(request, ASTREA_TOPLEVEL_MANAGER_V1_ACTION_RESULT_ACCEPTED);
    QVERIFY(compositor.pumpUntil([&] { return completed.count() == 1; }));
    QCOMPARE(completed.at(0).at(0).value<quint64>(), quint64(123));
    QCOMPARE(completed.at(0).at(1).value<ToplevelAction>(), ToplevelAction::Activate);
    QCOMPARE(completed.at(0).at(2).value<ToplevelActionResult>(), ToplevelActionResult::Accepted);

    const auto submitManagedAction = [&](ToplevelAction action, quint32 wireAction,
                                         ToplevelActionResult result, quint64 consumerToken) {
        const int expectedRequestCount = compositor.actionRequests().size() + 1;
        QVERIFY(!connection.requestAction(QStringLiteral("78"), action, consumerToken).has_value());
        QVERIFY(compositor.pumpUntil([&] {
            return compositor.actionRequests().size() == expectedRequestCount;
        }));
        const auto managedRequest = compositor.actionRequests().last();
        QCOMPARE(managedRequest.window->id, QStringLiteral("78"));
        QCOMPARE(managedRequest.action, wireAction);
        compositor.sendActionDone(managedRequest,
                                  result == ToplevelActionResult::Accepted
                                      ? ASTREA_TOPLEVEL_MANAGER_V1_ACTION_RESULT_ACCEPTED
                                      : result == ToplevelActionResult::NoChange
                                          ? ASTREA_TOPLEVEL_MANAGER_V1_ACTION_RESULT_NO_CHANGE
                                          : ASTREA_TOPLEVEL_MANAGER_V1_ACTION_RESULT_UNAVAILABLE);
        const int expectedCompletionCount = completed.count() + 1;
        QVERIFY(compositor.pumpUntil([&] { return completed.count() == expectedCompletionCount; }));
        const auto completion = completed.last();
        QCOMPARE(completion.at(0).value<quint64>(), consumerToken);
        QCOMPARE(completion.at(1).value<ToplevelAction>(), action);
        QCOMPARE(completion.at(2).value<ToplevelActionResult>(), result);
    };
    submitManagedAction(ToplevelAction::Minimize,
                        ASTREA_TOPLEVEL_MANAGER_V1_ACTION_MINIMIZE,
                        ToplevelActionResult::NoChange, 124);
    submitManagedAction(ToplevelAction::Restore,
                        ASTREA_TOPLEVEL_MANAGER_V1_ACTION_RESTORE,
                        ToplevelActionResult::Unavailable, 125);
    submitManagedAction(ToplevelAction::Close,
                        ASTREA_TOPLEVEL_MANAGER_V1_ACTION_CLOSE,
                        ToplevelActionResult::Accepted, 126);

    compositor.sendClosed(managedWindow);
    compositor.sendClosed(window);
    compositor.sendManagerDone(2, 0);
    QVERIFY(compositor.pumpUntil([&] { return connection.snapshot().windows.isEmpty(); }));
    connection.stop();
}

void TyphonProtocolIntegrationTest::crossClientAuthenticationCannotBorrowCapability()
{
    FakeTyphonCompositor compositor(true, 2, true, true);
    TyphonToplevelConnection authenticatedConnection;
    TyphonToplevelConnection unauthenticatedConnection;

    authenticatedConnection.start();
    QVERIFY(compositor.pumpUntil([&] {
        return authenticatedConnection.state() == TyphonConnectionState::WaitingForInitialSnapshot;
    }));
    QCOMPARE(authenticatedConnection.actionCapability(), TyphonActionCapabilityState::ActionReadyV2);
    wl_client *authenticatedClient = compositor.authenticatedClient();
    QVERIFY(authenticatedClient);
    QCOMPARE(compositor.authenticationCount(), 1);

    unauthenticatedConnection.start();
    QVERIFY(compositor.pumpUntil([&] {
        return unauthenticatedConnection.state() == TyphonConnectionState::WaitingForInitialSnapshot;
    }));
    QCOMPARE(unauthenticatedConnection.actionCapability(), TyphonActionCapabilityState::ReadOnlyV2);
    QVERIFY(compositor.managerClient());
    QVERIFY(compositor.managerClient() != authenticatedClient);
    QCOMPARE(compositor.lastRejectedClient(), compositor.managerClient());

    auto *window = compositor.beginWindow(QStringLiteral("88"));
    QVERIFY(window);
    compositor.sendMetadata(window, QStringLiteral("org.example.App"), QStringLiteral("Example"),
                            88, ASTREA_TOPLEVEL_V1_KIND_XDG_TOPLEVEL, 0, 88);
    compositor.sendDone(window, 1);
    compositor.sendManagerDone(1, 1);
    QVERIFY(compositor.pumpUntil([&] {
        return unauthenticatedConnection.state() == TyphonConnectionState::Ready;
    }));

    const auto error = unauthenticatedConnection.requestAction(
        QStringLiteral("88"), ToplevelAction::Activate, 1);
    QVERIFY(error.has_value());
    QCOMPARE(error.value(), ToplevelActionError::NotAuthenticated);
    QCOMPARE(compositor.actionRequests().size(), 0);
    authenticatedConnection.stop();
    unauthenticatedConnection.stop();
}

void TyphonProtocolIntegrationTest::reconnectRequiresFreshAuthenticationAndClientIdentity()
{
    FakeTyphonCompositor compositor(true, 2);
    TyphonToplevelConnection connection;
    connection.start();
    QVERIFY(compositor.pumpUntil([&] {
        return connection.state() == TyphonConnectionState::WaitingForInitialSnapshot;
    }));
    QCOMPARE(connection.actionCapability(), TyphonActionCapabilityState::ActionReadyV2);
    wl_client *oldClient = compositor.authenticatedClient();
    QVERIFY(oldClient);
    const quint64 oldGeneration = connection.connectionGeneration();

    compositor.disconnectClients();
    QVERIFY(compositor.pumpUntil([&] {
        return connection.state() == TyphonConnectionState::Degraded
            || connection.state() == TyphonConnectionState::Disconnected;
    }));
    std::unique_ptr<wl_display, decltype(&wl_display_disconnect)> parkingClient(
        compositor.connectIndependentClient(), &wl_display_disconnect);
    QVERIFY(parkingClient);
    QVERIFY(compositor.pumpUntil([&] { return compositor.clientCount() == 1; }));
    QTRY_VERIFY_WITH_TIMEOUT(connection.connectionGeneration() == oldGeneration + 1, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(
        connection.actionCapability() == TyphonActionCapabilityState::ActionReadyV2, 3000);

    wl_client *newClient = compositor.authenticatedClient();
    QVERIFY(newClient);
    QVERIFY(newClient != oldClient);
    QCOMPARE(compositor.authenticationCount(), 2);
    QCOMPARE(connection.managerVersion(), quint32(2));
    connection.stop();
}

void TyphonProtocolIntegrationTest::v2WithoutAuthenticationRemainsReadOnly()
{
    FakeTyphonCompositor compositor(true, 2, false);
    TyphonToplevelConnection connection;
    connection.start();
    QVERIFY(compositor.pumpUntil([&] {
        return connection.state() == TyphonConnectionState::WaitingForInitialSnapshot;
    }));
    QCOMPARE(connection.actionCapability(), TyphonActionCapabilityState::ReadOnlyV2);

    auto *window = compositor.beginWindow(QStringLiteral("88"));
    QVERIFY(window);
    compositor.sendMetadata(window, QStringLiteral("org.example.App"), QStringLiteral("Example"),
                            88, ASTREA_TOPLEVEL_V1_KIND_XDG_TOPLEVEL, 0, 88);
    compositor.sendDone(window, 1);
    compositor.sendManagerDone(1, 1);
    QVERIFY(compositor.pumpUntil([&] { return connection.state() == TyphonConnectionState::Ready; }));
    const auto error = connection.requestAction(QStringLiteral("88"), ToplevelAction::Activate, 1);
    QVERIFY(error.has_value());
    QCOMPARE(error.value(), ToplevelActionError::NotAuthenticated);
    QCOMPARE(compositor.actionRequests().size(), 0);
    connection.stop();
}

void TyphonProtocolIntegrationTest::sharedSessionOwnsToplevelTransport()
{
    FakeTyphonCompositor compositor(true, 2);
    TyphonSharedConnection session;
    TyphonToplevelConnection connection(&session);

    session.start();
    connection.start();
    QVERIFY(compositor.pumpUntil([&] {
        return connection.state() == TyphonConnectionState::WaitingForInitialSnapshot;
    }));

    QCOMPARE(compositor.authenticationCount(), 1);
    QCOMPARE(compositor.clientCount(), 1);
    QCOMPARE(session.authenticationGeneration(), session.connectionGeneration());

    connection.stop();
    session.stop();
}

QTEST_MAIN(TyphonProtocolIntegrationTest)
#include "TyphonProtocolIntegrationTest.moc"
