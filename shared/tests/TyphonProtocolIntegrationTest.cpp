#include <QCoreApplication>
#include <QSignalSpy>
#include <QSocketNotifier>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <functional>

#include <wayland-server-core.h>
#include "astrea-toplevel-management-v1-server-protocol.h"

#include "platform/typhon/TyphonToplevelConnection.hpp"

using namespace Astrea::Typhon;

class FakeTyphonCompositor final : public QObject {
public:
    struct ServerWindow {
        FakeTyphonCompositor *owner = nullptr;
        wl_resource *resource = nullptr;
    };

    explicit FakeTyphonCompositor(bool advertiseManager, QObject *parent = nullptr)
        : QObject(parent)
    {
        QVERIFY(m_runtime.isValid());
        m_previousRuntime = qgetenv("XDG_RUNTIME_DIR");
        m_previousDisplay = qgetenv("WAYLAND_DISPLAY");
        qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
        m_socketName = QStringLiteral("wayland-typhon-m6");
        qputenv("WAYLAND_DISPLAY", m_socketName.toUtf8());

        m_display = wl_display_create();
        QVERIFY(m_display);
        m_loop = wl_display_get_event_loop(m_display);
        QVERIFY(m_loop);
        QCOMPARE(wl_display_add_socket(m_display, m_socketName.toUtf8().constData()), 0);
        m_serverNotifier = new QSocketNotifier(wl_event_loop_get_fd(m_loop), QSocketNotifier::Read, this);
        connect(m_serverNotifier, &QSocketNotifier::activated, this,
                [this](QSocketDescriptor, QSocketNotifier::Type) { dispatchServerEvents(); });
        if (advertiseManager)
            m_global = wl_global_create(m_display, &astrea_toplevel_manager_v1_interface, 1, this, &bindManager);
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
        window->resource = wl_resource_create(wl_resource_get_client(m_manager->resource),
                                               &astrea_toplevel_v1_interface, 1, 0);
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

    void disconnectClients()
    {
        if (m_display)
            wl_display_destroy_clients(m_display);
    }

    int liveWindowCount() const
    {
        return m_windows.size();
    }

private:
    static void bindManager(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *self = static_cast<FakeTyphonCompositor *>(data);
        auto *manager = new ServerWindow;
        manager->owner = self;
        manager->resource = wl_resource_create(client, &astrea_toplevel_manager_v1_interface,
                                               std::min(version, 1u), id);
        if (!manager->resource) {
            delete manager;
            return;
        }
        wl_resource_set_implementation(manager->resource, &kManagerImplementation,
                                       manager, &destroyManager);
        self->m_manager = manager;
    }

    static void destroyManager(wl_resource *resource)
    {
        auto *manager = static_cast<ServerWindow *>(wl_resource_get_user_data(resource));
        if (manager && manager->owner && manager->owner->m_manager == manager)
            manager->owner->m_manager = nullptr;
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

    QTemporaryDir m_runtime;
    QByteArray m_previousRuntime;
    QByteArray m_previousDisplay;
    QString m_socketName;
    wl_display *m_display = nullptr;
    wl_event_loop *m_loop = nullptr;
    wl_global *m_global = nullptr;
    ServerWindow *m_manager = nullptr;
    QVector<ServerWindow *> m_windows;
    QSocketNotifier *m_serverNotifier = nullptr;
};

const struct astrea_toplevel_manager_v1_interface FakeTyphonCompositor::kManagerImplementation = {
    &FakeTyphonCompositor::destroyManagerRequest
};

const struct astrea_toplevel_v1_interface FakeTyphonCompositor::kWindowImplementation = {
    &FakeTyphonCompositor::destroyWindowRequest
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

QTEST_MAIN(TyphonProtocolIntegrationTest)
#include "TyphonProtocolIntegrationTest.moc"
