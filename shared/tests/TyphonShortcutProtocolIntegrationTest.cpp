#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QSignalSpy>
#include <QSocketNotifier>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <functional>

#include "TyphonShortcutProtocolServerCompat.hpp"
#include "astrea-shell-auth-v1-server-protocol.h"

#include "platform/typhon/TyphonShortcutClient.hpp"
#include "platform/typhon/TyphonSharedConnection.hpp"

class FakeShortcutCompositor final : public QObject {
public:
    struct Registration {
        FakeShortcutCompositor *owner = nullptr;
        wl_resource *resource = nullptr;
        QString namespaceName;
        QString name;
        bool active = false;
    };

    explicit FakeShortcutCompositor(std::uint32_t protocolVersion = 2,
                                    bool rejectShellAuthorization = false,
                                    QObject *parent = nullptr)
        : QObject(parent), m_runtime(QStringLiteral("/tmp/typhon-shortcuts-XXXXXX")),
          m_protocolVersion(std::min(protocolVersion, 2u)),
          m_rejectShellAuthorization(rejectShellAuthorization)
    {
        QVERIFY(m_runtime.isValid());
        m_previousRuntime = qgetenv("XDG_RUNTIME_DIR");
        m_previousDisplay = qgetenv("WAYLAND_DISPLAY");
        m_previousCapability = qgetenv("ASTREA_SHELL_CAPABILITY_FILE");
        qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
        m_socketName = QStringLiteral("wayland-typhon-shortcuts");
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
        m_serverNotifier = new QSocketNotifier(wl_event_loop_get_fd(m_loop), QSocketNotifier::Read,
                                               this);
        connect(m_serverNotifier, &QSocketNotifier::activated, this,
                [this](QSocketDescriptor, QSocketNotifier::Type) { dispatchServerEvents(); });
        m_authGlobal = wl_global_create(m_display, &astrea_shell_auth_manager_v1_interface, 1,
                                        this, &bindAuthManager);
        QVERIFY(m_authGlobal);
        m_global = wl_global_create(m_display, &astrea_shortcuts_manager_v1_interface,
                                     m_protocolVersion, this, &bindManager);
        QVERIFY(m_global);
    }

    ~FakeShortcutCompositor() override
    {
        if (m_serverNotifier)
            m_serverNotifier->setEnabled(false);
        delete m_serverNotifier;
        m_serverNotifier = nullptr;
        if (m_display) {
            wl_display_destroy_clients(m_display);
            qDeleteAll(m_registrations);
            m_registrations.clear();
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

    bool waitUntil(const std::function<bool()> &condition, int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        while (!condition() && timer.elapsed() < timeoutMs)
            QTest::qWait(10);
        return condition();
    }

    QStringList registrationNames() const
    {
        QStringList names;
        for (const Registration *registration : m_owners)
            names.append(registration->name);
        std::sort(names.begin(), names.end());
        return names;
    }

    Registration *registration(const QString &name) const
    {
        return m_owners.value(ownerKey(QStringLiteral("astrea-shell"), name), nullptr);
    }

    int authenticationCount() const
    {
        return m_authenticatedClients.size();
    }

    int clientCount() const
    {
        int count = 0;
        wl_client *client = nullptr;
        wl_client_for_each(client, wl_display_get_client_list(m_display))
            ++count;
        return count;
    }

    void disconnectClients()
    {
        if (m_display)
            wl_display_destroy_clients(m_display);
    }

    void sendPressed(const QString &name, quint32 serial, quint32 timestamp)
    {
        if (Registration *item = registration(name)) {
            astrea_shortcut_v1_send_pressed(item->resource, serial, timestamp);
            flushClients();
        }
    }

    void sendRepeated(const QString &name, quint32 serial, quint32 timestamp)
    {
        if (Registration *item = registration(name)) {
            astrea_shortcut_v1_send_repeated(item->resource, serial, timestamp);
            flushClients();
        }
    }

    void sendReleased(const QString &name, quint32 serial, quint32 timestamp)
    {
        if (Registration *item = registration(name)) {
            astrea_shortcut_v1_send_released(item->resource, serial, timestamp);
            flushClients();
        }
    }

    void sendCancelled(const QString &name, quint32 serial, bool releaseOwner = false)
    {
        if (Registration *item = registration(name)) {
            if (releaseOwner) {
                m_owners.remove(ownerKey(item->namespaceName, item->name));
                item->active = false;
            }
            astrea_shortcut_v1_send_cancelled(item->resource, serial);
            flushClients();
        }
    }

    int conditionalRequestCount(const QString &name) const
    {
        return m_conditionalRequestCounts.value(name);
    }

private:
    static void bindAuthManager(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *self = static_cast<FakeShortcutCompositor *>(data);
        auto *manager = new Registration;
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
        auto *manager = static_cast<Registration *>(wl_resource_get_user_data(resource));
        if (manager && manager->owner && manager->owner->m_authManager == manager)
            manager->owner->m_authManager = nullptr;
        delete manager;
    }

    static void authenticateRequest(wl_client *client, wl_resource *resource,
                                    const char *capability)
    {
        auto *manager = static_cast<Registration *>(wl_resource_get_user_data(resource));
        auto *self = manager ? manager->owner : nullptr;
        const QByteArray value = QByteArray(capability ? capability : "");
        bool valid = value.size() == 64;
        for (const char byte : value) {
            valid = valid && ((byte >= '0' && byte <= '9')
                              || (byte >= 'a' && byte <= 'f'));
        }
        if (valid) {
            if (self)
                self->m_authenticatedClients.append(client);
            astrea_shell_auth_manager_v1_send_authenticated(resource);
        } else {
            astrea_shell_auth_manager_v1_send_rejected(resource);
        }
    }

    static void destroyAuthManagerRequest(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    static void bindManager(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *self = static_cast<FakeShortcutCompositor *>(data);
        auto *manager = new Registration;
        manager->owner = self;
        manager->resource = wl_resource_create(client, &astrea_shortcuts_manager_v1_interface,
                                               std::min(version, self->m_protocolVersion), id);
        if (!manager->resource) {
            delete manager;
            return;
        }
        wl_resource_set_implementation(manager->resource, &kManagerImplementation, manager,
                                       &destroyManager);
    }

    static void destroyManager(wl_resource *resource)
    {
        delete static_cast<Registration *>(wl_resource_get_user_data(resource));
    }

    static void destroyShortcut(wl_resource *resource)
    {
        auto *registration = static_cast<Registration *>(wl_resource_get_user_data(resource));
        if (!registration)
            return;
        if (registration->active && registration->owner) {
            const QString key = ownerKey(registration->namespaceName, registration->name);
            if (registration->owner->m_owners.value(key) == registration)
                registration->owner->m_owners.remove(key);
        }
        registration->active = false;
        registration->resource = nullptr;
    }

    static void destroyManagerRequest(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    static void destroyShortcutRequest(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    static void registerShortcutRequest(wl_client *client, wl_resource *managerResource, uint32_t id,
                                        const char *namespaceName, const char *name,
                                        const char *description)
    {
        registerRequest(client, managerResource, id, namespaceName, name, description, false);
    }

    static void registerShortcutIfAvailableRequest(wl_client *client, wl_resource *managerResource,
                                                    uint32_t id, const char *namespaceName,
                                                    const char *name, const char *description)
    {
        registerRequest(client, managerResource, id, namespaceName, name, description, true);
    }

    static void registerRequest(wl_client *client, wl_resource *managerResource, uint32_t id,
                                const char *namespaceName, const char *name, const char *,
                                bool ifAvailable)
    {
        auto *manager = static_cast<Registration *>(wl_resource_get_user_data(managerResource));
        auto *self = manager ? manager->owner : nullptr;
        if (!self)
            return;
        auto *registration = new Registration;
        registration->owner = self;
        registration->namespaceName = QString::fromUtf8(namespaceName ? namespaceName : "");
        registration->name = QString::fromUtf8(name ? name : "");
        const std::uint32_t version = static_cast<std::uint32_t>(
            std::min(wl_resource_get_version(managerResource), 2));
        registration->resource = wl_resource_create(client, &astrea_shortcut_v1_interface,
                                                     version, id);
        if (!registration->resource) {
            delete registration;
            return;
        }
        wl_resource_set_implementation(registration->resource, &kShortcutImplementation,
                                       registration, &destroyShortcut);
        self->m_registrations.append(registration);

        if (ifAvailable)
            self->m_conditionalRequestCounts[registration->name] += 1;
        if (registration->namespaceName == QStringLiteral("astrea-shell")
            && self->m_rejectShellAuthorization) {
            if (version >= 2)
                astrea_shortcut_v1_send_rejected(registration->resource, 1, 1);
            else
                astrea_shortcut_v1_send_cancelled(registration->resource, 1);
            self->flushClients();
            return;
        }

        const QString key = ownerKey(registration->namespaceName, registration->name);
        Registration *currentOwner = self->m_owners.value(key, nullptr);
        if (ifAvailable && currentOwner) {
            if (version >= 2)
                astrea_shortcut_v1_send_rejected(registration->resource, 2, 2);
            else
                astrea_shortcut_v1_send_cancelled(registration->resource, 2);
            self->flushClients();
            return;
        }
        if (!ifAvailable && currentOwner) {
            self->m_owners.remove(key);
            currentOwner->active = false;
            if (currentOwner->resource)
                astrea_shortcut_v1_send_cancelled(currentOwner->resource, 3);
        }
        registration->active = true;
        self->m_owners.insert(key, registration);
        if (version >= 2)
            astrea_shortcut_v1_send_registered(registration->resource, 4);
        self->flushClients();
    }

    static QString ownerKey(const QString &namespaceName, const QString &name)
    {
        return namespaceName + QLatin1Char('\n') + name;
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

    static const struct astrea_shortcuts_manager_v1_interface kManagerImplementation;
    static const struct astrea_shortcut_v1_interface kShortcutImplementation;
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
    QSocketNotifier *m_serverNotifier = nullptr;
    QVector<Registration *> m_registrations;
    QHash<QString, Registration *> m_owners;
    QHash<QString, int> m_conditionalRequestCounts;
    QVector<wl_client *> m_authenticatedClients;
    Registration *m_authManager = nullptr;
    std::uint32_t m_protocolVersion = 2;
    bool m_rejectShellAuthorization = false;
};

const struct astrea_shortcuts_manager_v1_interface FakeShortcutCompositor::kManagerImplementation = {
    &FakeShortcutCompositor::destroyManagerRequest,
    &FakeShortcutCompositor::registerShortcutRequest,
    &FakeShortcutCompositor::registerShortcutIfAvailableRequest,
};

const struct astrea_shortcut_v1_interface FakeShortcutCompositor::kShortcutImplementation = {
    &FakeShortcutCompositor::destroyShortcutRequest,
};

const struct astrea_shell_auth_manager_v1_interface
    FakeShortcutCompositor::kAuthManagerImplementation = {
        &FakeShortcutCompositor::authenticateRequest,
        &FakeShortcutCompositor::destroyAuthManagerRequest,
    };

class TyphonShortcutProtocolIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void registersReservedShortcutsAndDeliversLifecycle();
    void legacyCancellationIsObservableWithoutRetry();
    void v2CancellationRecoversOnlyIntoVacantOwnerSlot();
    void v2RecoveryCannotTakeOverOrPingPongWithAnotherOwner();
    void v2AuthorizationRejectionIsTerminal();
    void sharedSessionOwnsShortcutTransport();
    void sharedSessionRestoresShortcutOwnershipAcrossGenerations();
};

void TyphonShortcutProtocolIntegrationTest::registersReservedShortcutsAndDeliversLifecycle()
{
    FakeShortcutCompositor compositor;
    TyphonShortcutClient client;
    QSignalSpy eventSpy(&client, &TyphonShortcutClient::shortcutEvent);

    client.start();
    QVERIFY(compositor.pumpUntil([&client] { return client.isReady(); }));
    QCOMPARE(client.registeredShortcutCount(), 7);
    QCOMPARE(compositor.registrationNames(),
             QStringList({QStringLiteral("alt_tab_commit"), QStringLiteral("alt_tab_next"),
                          QStringLiteral("alt_tab_previous"),
                          QStringLiteral("screenshot_quick"),
                          QStringLiteral("screenshot_region_frozen"),
                          QStringLiteral("screenshot_region_live"),
                          QStringLiteral("spotlight_toggle")}));

    compositor.sendPressed(QStringLiteral("alt_tab_next"), 11, 101);
    compositor.sendRepeated(QStringLiteral("alt_tab_next"), 12, 102);
    compositor.sendReleased(QStringLiteral("alt_tab_next"), 13, 103);
    compositor.sendPressed(QStringLiteral("spotlight_toggle"), 14, 104);
    QVERIFY(compositor.pumpUntil([&eventSpy] { return eventSpy.count() == 4; }));
    QCOMPARE(qvariant_cast<TyphonShortcutPhase>(eventSpy.at(0).at(2)),
             TyphonShortcutPhase::Pressed);
    QCOMPARE(qvariant_cast<TyphonShortcutPhase>(eventSpy.at(1).at(2)),
             TyphonShortcutPhase::Repeated);
    QCOMPARE(qvariant_cast<TyphonShortcutPhase>(eventSpy.at(2).at(2)),
             TyphonShortcutPhase::Released);
    QCOMPARE(eventSpy.at(3).at(1).toString(), QStringLiteral("spotlight_toggle"));
    QCOMPARE(qvariant_cast<TyphonShortcutPhase>(eventSpy.at(3).at(2)),
             TyphonShortcutPhase::Pressed);
}

void TyphonShortcutProtocolIntegrationTest::legacyCancellationIsObservableWithoutRetry()
{
    FakeShortcutCompositor compositor(1);
    TyphonShortcutClient client;
    QSignalSpy eventSpy(&client, &TyphonShortcutClient::shortcutEvent);

    client.start();
    QVERIFY(compositor.pumpUntil([&client] { return client.isReady(); }));
    compositor.sendCancelled(QStringLiteral("alt_tab_next"), 99, true);
    QVERIFY(compositor.pumpUntil([&eventSpy] { return eventSpy.count() == 1; }));
    QCOMPARE(qvariant_cast<TyphonShortcutPhase>(eventSpy.at(0).at(2)),
             TyphonShortcutPhase::Cancelled);
    QCOMPARE(client.registeredShortcutCount(), 6);
    QCOMPARE(compositor.registrationNames().size(), 6);
    const QVariantMap health = client.registrationHealth()
                                   .value(QStringLiteral("alt_tab_next")).toMap();
    QCOMPARE(health.value(QStringLiteral("state")).toString(), QStringLiteral("cancelled"));
    QCOMPARE(health.value(QStringLiteral("recovery_pending")).toBool(), false);
    QCOMPARE(health.value(QStringLiteral("recovery_attempts")).toInt(), 0);
    QTest::qWait(600);
    QCOMPARE(compositor.conditionalRequestCount(QStringLiteral("alt_tab_next")), 0);
}

void TyphonShortcutProtocolIntegrationTest::v2CancellationRecoversOnlyIntoVacantOwnerSlot()
{
    FakeShortcutCompositor compositor;
    TyphonShortcutClient client;
    QSignalSpy eventSpy(&client, &TyphonShortcutClient::shortcutEvent);

    client.start();
    QVERIFY(compositor.pumpUntil([&client] { return client.isReady(); }));
    compositor.sendCancelled(QStringLiteral("screenshot_quick"), 99, true);
    QVERIFY(compositor.pumpUntil([&eventSpy] { return eventSpy.count() == 1; }));
    QCOMPARE(client.registeredShortcutCount(), 6);
    QCOMPARE(compositor.registrationNames().size(), 6);
    QCOMPARE(client.state(), TyphonShortcutConnectionState::Degraded);
    const QVariantMap pendingHealth = client.registrationHealth()
                                          .value(QStringLiteral("screenshot_quick")).toMap();
    QCOMPARE(pendingHealth.value(QStringLiteral("state")).toString(),
             QStringLiteral("recovery-pending"));
    QVERIFY(pendingHealth.value(QStringLiteral("recovery_pending")).toBool());

    eventSpy.clear();
    compositor.sendPressed(QStringLiteral("alt_tab_next"), 100, 200);
    compositor.sendPressed(QStringLiteral("spotlight_toggle"), 101, 201);
    QVERIFY(compositor.pumpUntil([&eventSpy] { return eventSpy.count() == 2; }));
    QCOMPARE(eventSpy.at(0).at(1).toString(), QStringLiteral("alt_tab_next"));
    QCOMPARE(eventSpy.at(1).at(1).toString(), QStringLiteral("spotlight_toggle"));

    QVERIFY(compositor.waitUntil([&client, &compositor] {
        const QVariantMap entry = client.registrationHealth()
                                      .value(QStringLiteral("screenshot_quick")).toMap();
        return client.isReady() && client.registeredShortcutCount() == 7
            && compositor.registrationNames().size() == 7
            && entry.value(QStringLiteral("state")).toString() == QStringLiteral("registered");
    }, 2000));

    const QVariantMap health = client.registrationHealth()
                                  .value(QStringLiteral("screenshot_quick")).toMap();
    QCOMPARE(health.value(QStringLiteral("recovery_attempts")).toInt(), 0);
    QCOMPARE(compositor.conditionalRequestCount(QStringLiteral("screenshot_quick")), 1);

    eventSpy.clear();
    compositor.sendPressed(QStringLiteral("alt_tab_next"), 101, 201);
    compositor.sendPressed(QStringLiteral("spotlight_toggle"), 102, 202);
    compositor.sendPressed(QStringLiteral("screenshot_quick"), 103, 203);
    QVERIFY(compositor.pumpUntil([&eventSpy] { return eventSpy.count() == 3; }));
    QCOMPARE(eventSpy.at(0).at(1).toString(), QStringLiteral("alt_tab_next"));
    QCOMPARE(eventSpy.at(1).at(1).toString(), QStringLiteral("spotlight_toggle"));
    QCOMPARE(eventSpy.at(2).at(1).toString(), QStringLiteral("screenshot_quick"));

    compositor.sendCancelled(QStringLiteral("screenshot_quick"), 104, true);
    QVERIFY(compositor.waitUntil([&client, &compositor] {
        return client.registeredShortcutCount() == 7
            && compositor.registrationNames().size() == 7
            && compositor.conditionalRequestCount(QStringLiteral("screenshot_quick")) == 2;
    }, 2000));
}

void TyphonShortcutProtocolIntegrationTest::v2RecoveryCannotTakeOverOrPingPongWithAnotherOwner()
{
    FakeShortcutCompositor compositor;
    TyphonShortcutClient firstClient;
    TyphonShortcutClient secondClient;
    QSignalSpy secondEventSpy(&secondClient, &TyphonShortcutClient::shortcutEvent);

    firstClient.start();
    QVERIFY(compositor.pumpUntil([&firstClient] { return firstClient.isReady(); }));
    secondClient.start();
    QVERIFY(compositor.pumpUntil([&secondClient] { return secondClient.isReady(); }));
    QVERIFY(compositor.waitUntil([&firstClient, &compositor] {
        const QVariantMap entry = firstClient.registrationHealth()
                                      .value(QStringLiteral("screenshot_quick")).toMap();
        return entry.value(QStringLiteral("state")).toString() == QStringLiteral("occupied")
            && entry.value(QStringLiteral("recovery_attempts")).toInt() == 3
            && compositor.conditionalRequestCount(QStringLiteral("screenshot_quick")) == 3;
    }, 7000));

    QVERIFY(!firstClient.isReady());
    QCOMPARE(firstClient.registeredShortcutCount(), 0);
    QCOMPARE(secondClient.registeredShortcutCount(), 7);
    QCOMPARE(compositor.registrationNames().size(), 7);
    for (const QString &name : {QStringLiteral("alt_tab_next"),
                                QStringLiteral("spotlight_toggle"),
                                QStringLiteral("screenshot_quick")}) {
        compositor.sendPressed(name, 301, 401);
    }
    QVERIFY(compositor.pumpUntil([&secondEventSpy] { return secondEventSpy.count() == 3; }));
    QCOMPARE(secondEventSpy.at(0).at(1).toString(), QStringLiteral("alt_tab_next"));
    QCOMPARE(secondEventSpy.at(1).at(1).toString(), QStringLiteral("spotlight_toggle"));
    QCOMPARE(secondEventSpy.at(2).at(1).toString(), QStringLiteral("screenshot_quick"));
    QCOMPARE(compositor.conditionalRequestCount(QStringLiteral("screenshot_quick")), 3);
}

void TyphonShortcutProtocolIntegrationTest::v2AuthorizationRejectionIsTerminal()
{
    FakeShortcutCompositor compositor(2, true);
    TyphonShortcutClient client;

    client.start();
    QVERIFY(compositor.pumpUntil([&client] {
        const QVariantMap entries = client.registrationHealth();
        return client.state() == TyphonShortcutConnectionState::Degraded && entries.size() == 7
            && entries.value(QStringLiteral("screenshot_quick")).toMap()
                   .value(QStringLiteral("state")).toString()
                == QStringLiteral("rejected-authorization");
    }));
    QCOMPARE(client.registeredShortcutCount(), 0);
    QCOMPARE(client.registrationHealth()
                 .value(QStringLiteral("screenshot_quick")).toMap()
                 .value(QStringLiteral("recovery_attempts")).toInt(),
             0);
    QTest::qWait(600);
    QCOMPARE(compositor.conditionalRequestCount(QStringLiteral("screenshot_quick")), 0);
}

void TyphonShortcutProtocolIntegrationTest::sharedSessionOwnsShortcutTransport()
{
    FakeShortcutCompositor compositor;
    TyphonSharedConnection session;
    TyphonShortcutClient client(&session);

    session.start();
    client.start();
    QVERIFY(compositor.pumpUntil([&client] { return client.isReady(); }));

    QCOMPARE(compositor.authenticationCount(), 1);
    QCOMPARE(compositor.clientCount(), 1);
    QCOMPARE(client.connectionGeneration(), session.connectionGeneration());

    client.stop();
    session.stop();
}

void TyphonShortcutProtocolIntegrationTest::sharedSessionRestoresShortcutOwnershipAcrossGenerations()
{
    FakeShortcutCompositor compositor;
    TyphonSharedConnection session;
    TyphonShortcutClient client(&session);
    QSignalSpy eventSpy(&client, &TyphonShortcutClient::shortcutEvent);

    session.start();
    client.start();
    QVERIFY(compositor.pumpUntil([&client] { return client.isReady(); }));
    QCOMPARE(session.connectionGeneration(), quint64(1));
    QCOMPARE(client.registeredShortcutCount(), 7);

    compositor.disconnectClients();
    QVERIFY(compositor.pumpUntil([&session] {
        return session.state() == TyphonSharedConnection::State::Disconnected
            || session.state() == TyphonSharedConnection::State::Degraded;
    }));
    QCOMPARE(client.registeredShortcutCount(), 0);

    for (int expectedGeneration = 2; expectedGeneration <= 101; ++expectedGeneration) {
        session.reconnectNowForTest();
        QVERIFY(compositor.pumpUntil([&client, expectedGeneration] {
            return client.isReady()
                && client.connectionGeneration() == static_cast<quint64>(expectedGeneration);
        }));
        QCOMPARE(client.registeredShortcutCount(), 7);
        QCOMPARE(compositor.clientCount(), 1);
        QCOMPARE(compositor.registrationNames().size(), 7);

        eventSpy.clear();
        compositor.sendPressed(QStringLiteral("spotlight_toggle"), expectedGeneration, 1);
        QVERIFY(compositor.pumpUntil([&eventSpy] { return eventSpy.count() == 1; }));
        QCOMPARE(eventSpy.at(0).at(1).toString(), QStringLiteral("spotlight_toggle"));

        if (expectedGeneration != 101) {
            compositor.disconnectClients();
            QVERIFY(compositor.pumpUntil([&session] {
                return session.state() == TyphonSharedConnection::State::Disconnected
                    || session.state() == TyphonSharedConnection::State::Degraded;
            }));
            QCOMPARE(client.registeredShortcutCount(), 0);
        }
    }

    QCOMPARE(session.connectionGeneration(), quint64(101));
    QCOMPARE(session.authenticationGeneration(), quint64(101));
    QCOMPARE(compositor.authenticationCount(), 101);
    QCOMPARE(compositor.clientCount(), 1);
    client.stop();
    session.stop();
}

QTEST_GUILESS_MAIN(TyphonShortcutProtocolIntegrationTest)
#include "TyphonShortcutProtocolIntegrationTest.moc"
