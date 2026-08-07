#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QSocketNotifier>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <functional>

#include "TyphonShortcutProtocolServerCompat.hpp"
#include "astrea-shell-auth-v1-server-protocol.h"

#include "platform/typhon/TyphonShortcutClient.hpp"

class FakeShortcutCompositor final : public QObject {
public:
    struct Registration {
        FakeShortcutCompositor *owner = nullptr;
        wl_resource *resource = nullptr;
        QString namespaceName;
        QString name;
    };

    explicit FakeShortcutCompositor(QObject *parent = nullptr)
        : QObject(parent), m_runtime(QStringLiteral("/tmp/typhon-shortcuts-XXXXXX"))
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
        m_global = wl_global_create(m_display, &astrea_shortcuts_manager_v1_interface, 1, this,
                                     &bindManager);
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

    QStringList registrationNames() const
    {
        QStringList names;
        for (const Registration *registration : m_registrations)
            if (registration->resource)
                names.append(registration->name);
        std::sort(names.begin(), names.end());
        return names;
    }

    Registration *registration(const QString &name) const
    {
        const auto it = std::find_if(m_registrations.cbegin(), m_registrations.cend(),
                                     [&name](const Registration *registration) {
                                         return registration->resource && registration->name == name;
                                     });
        return it == m_registrations.cend() ? nullptr : *it;
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

    void sendCancelled(const QString &name, quint32 serial)
    {
        if (Registration *item = registration(name)) {
            astrea_shortcut_v1_send_cancelled(item->resource, serial);
            flushClients();
        }
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

    static void authenticateRequest(wl_client *, wl_resource *resource, const char *capability)
    {
        const QByteArray value = QByteArray(capability ? capability : "");
        bool valid = value.size() == 64;
        for (const char byte : value) {
            valid = valid && ((byte >= '0' && byte <= '9')
                              || (byte >= 'a' && byte <= 'f'));
        }
        if (valid)
            astrea_shell_auth_manager_v1_send_authenticated(resource);
        else
            astrea_shell_auth_manager_v1_send_rejected(resource);
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
                                               std::min(version, 1u), id);
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
                                        const char *)
    {
        auto *manager = static_cast<Registration *>(wl_resource_get_user_data(managerResource));
        auto *self = manager ? manager->owner : nullptr;
        if (!self)
            return;
        auto *registration = new Registration;
        registration->owner = self;
        registration->namespaceName = QString::fromUtf8(namespaceName ? namespaceName : "");
        registration->name = QString::fromUtf8(name ? name : "");
        registration->resource = wl_resource_create(client, &astrea_shortcut_v1_interface, 1, id);
        if (!registration->resource) {
            delete registration;
            return;
        }
        wl_resource_set_implementation(registration->resource, &kShortcutImplementation,
                                       registration, &destroyShortcut);
        self->m_registrations.append(registration);
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
    Registration *m_authManager = nullptr;
};

const struct astrea_shortcuts_manager_v1_interface FakeShortcutCompositor::kManagerImplementation = {
    &FakeShortcutCompositor::destroyManagerRequest,
    &FakeShortcutCompositor::registerShortcutRequest,
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
    void cancellationDoesNotReregister();
};

void TyphonShortcutProtocolIntegrationTest::registersReservedShortcutsAndDeliversLifecycle()
{
    FakeShortcutCompositor compositor;
    TyphonShortcutClient client;
    QSignalSpy eventSpy(&client, &TyphonShortcutClient::shortcutEvent);

    client.start();
    QVERIFY(compositor.pumpUntil([&client] { return client.isReady(); }));
    QCOMPARE(client.registeredShortcutCount(), 3);
    QCOMPARE(compositor.registrationNames(),
             QStringList({QStringLiteral("alt_tab_commit"), QStringLiteral("alt_tab_next"),
                          QStringLiteral("alt_tab_previous")}));

    compositor.sendPressed(QStringLiteral("alt_tab_next"), 11, 101);
    compositor.sendRepeated(QStringLiteral("alt_tab_next"), 12, 102);
    compositor.sendReleased(QStringLiteral("alt_tab_next"), 13, 103);
    QVERIFY(compositor.pumpUntil([&eventSpy] { return eventSpy.count() == 3; }));
    QCOMPARE(qvariant_cast<TyphonShortcutPhase>(eventSpy.at(0).at(2)),
             TyphonShortcutPhase::Pressed);
    QCOMPARE(qvariant_cast<TyphonShortcutPhase>(eventSpy.at(1).at(2)),
             TyphonShortcutPhase::Repeated);
    QCOMPARE(qvariant_cast<TyphonShortcutPhase>(eventSpy.at(2).at(2)),
             TyphonShortcutPhase::Released);
}

void TyphonShortcutProtocolIntegrationTest::cancellationDoesNotReregister()
{
    FakeShortcutCompositor compositor;
    TyphonShortcutClient client;
    QSignalSpy eventSpy(&client, &TyphonShortcutClient::shortcutEvent);

    client.start();
    QVERIFY(compositor.pumpUntil([&client] { return client.isReady(); }));
    compositor.sendCancelled(QStringLiteral("alt_tab_next"), 99);
    QVERIFY(compositor.pumpUntil([&eventSpy] { return eventSpy.count() == 1; }));
    QCOMPARE(qvariant_cast<TyphonShortcutPhase>(eventSpy.at(0).at(2)),
             TyphonShortcutPhase::Cancelled);
    QCOMPARE(client.registeredShortcutCount(), 2);
    QTest::qWait(20);
    QCOMPARE(compositor.registrationNames().size(), 2);
}

QTEST_GUILESS_MAIN(TyphonShortcutProtocolIntegrationTest)
#include "TyphonShortcutProtocolIntegrationTest.moc"
