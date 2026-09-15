#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QSocketNotifier>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>

#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include "astrea-screen-capture-v1-server-protocol.h"
#include "astrea-shell-auth-v1-server-protocol.h"

#include "platform/typhon/TyphonScreenCaptureClient.hpp"
#include "platform/typhon/TyphonSharedConnection.hpp"

namespace {

constexpr std::uint32_t kRgba8888 = 0x34324152U;

class FakeScreenCaptureCompositor final : public QObject {
public:
    struct AuthManager {
        FakeScreenCaptureCompositor *owner = nullptr;
        wl_resource *resource = nullptr;
    };

    struct CaptureManager {
        FakeScreenCaptureCompositor *owner = nullptr;
        wl_resource *resource = nullptr;
    };

    struct Capture {
        FakeScreenCaptureCompositor *owner = nullptr;
        wl_resource *resource = nullptr;
        wl_resource *output = nullptr;
    };

    struct Output {
        FakeScreenCaptureCompositor *owner = nullptr;
        wl_resource *resource = nullptr;
    };

    explicit FakeScreenCaptureCompositor(QObject *parent = nullptr)
        : QObject(parent)
    {
        QVERIFY(m_runtime.isValid());
        m_previousRuntime = qgetenv("XDG_RUNTIME_DIR");
        m_previousDisplay = qgetenv("WAYLAND_DISPLAY");
        m_previousCapability = qgetenv("ASTREA_SHELL_CAPABILITY_FILE");
        qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
        m_socketName = QStringLiteral("wayland-typhon-screen-capture");
        qputenv("WAYLAND_DISPLAY", m_socketName.toUtf8());
        m_capabilityPath = m_runtime.filePath(QStringLiteral("capability"));
        QFile capability(m_capabilityPath);
        QVERIFY(capability.open(QIODevice::WriteOnly));
        QCOMPARE(capability.write(QByteArray(64, 'a') + '\n'), qint64(65));
        capability.close();
        qputenv("ASTREA_SHELL_CAPABILITY_FILE", m_capabilityPath.toUtf8());

        m_display = wl_display_create();
        QVERIFY(m_display);
        m_loop = wl_display_get_event_loop(m_display);
        QVERIFY(m_loop);
        QCOMPARE(wl_display_add_socket(m_display, m_socketName.toUtf8().constData()), 0);
        m_serverNotifier = new QSocketNotifier(wl_event_loop_get_fd(m_loop),
                                               QSocketNotifier::Read, this);
        connect(m_serverNotifier, &QSocketNotifier::activated, this,
                [this](QSocketDescriptor, QSocketNotifier::Type) { dispatchServerEvents(); });

        m_authGlobal = wl_global_create(m_display, &astrea_shell_auth_manager_v1_interface, 1,
                                        this, &bindAuthManager);
        QVERIFY(m_authGlobal);
        m_captureGlobal = wl_global_create(m_display,
                                           &astrea_screen_capture_manager_v1_interface, 1, this,
                                           &bindCaptureManager);
        QVERIFY(m_captureGlobal);
        m_outputGlobal = wl_global_create(m_display, &wl_output_interface, 4, this, &bindOutput);
        QVERIFY(m_outputGlobal);
    }

    ~FakeScreenCaptureCompositor() override
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

    void disconnectClients()
    {
        if (m_display)
            wl_display_destroy_clients(m_display);
    }

    int captureRequestCount() const { return m_captureRequestCount; }
    bool managerDestroyed() const { return m_managerDestroyed; }
    wl_resource *lastOutput() const { return m_lastOutput; }
    wl_resource *outputResource() const { return m_output ? m_output->resource : nullptr; }

    void sendReady(const QByteArray &pixels, std::uint32_t width, std::uint32_t height,
                   std::uint32_t stride)
    {
        QVERIFY(m_lastCapture && m_lastCapture->resource);
        m_readyFile = std::make_unique<QTemporaryFile>(&m_readyFileParent);
        QVERIFY(m_readyFile->open());
        QCOMPARE(m_readyFile->write(pixels), pixels.size());
        QVERIFY(m_readyFile->flush());
        astrea_screen_capture_v1_send_ready(m_lastCapture->resource, m_readyFile->handle(),
                                             width, height, stride, kRgba8888);
        flushClients();
    }

    void sendFailed(const QString &reason)
    {
        QVERIFY(m_lastCapture && m_lastCapture->resource);
        astrea_screen_capture_v1_send_failed(m_lastCapture->resource,
                                             reason.toUtf8().constData());
        flushClients();
    }

    void clearReadyFile() { m_readyFile.reset(); }

private:
    static void bindAuthManager(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *self = static_cast<FakeScreenCaptureCompositor *>(data);
        auto *manager = new AuthManager;
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
        auto *manager = static_cast<AuthManager *>(wl_resource_get_user_data(resource));
        if (manager && manager->owner && manager->owner->m_authManager == manager)
            manager->owner->m_authManager = nullptr;
        delete manager;
    }

    static void authenticateRequest(wl_client *client, wl_resource *resource,
                                    const char *capability)
    {
        auto *manager = static_cast<AuthManager *>(wl_resource_get_user_data(resource));
        auto *self = manager ? manager->owner : nullptr;
        const QByteArray value = QByteArray(capability ? capability : "");
        bool valid = value.size() == 64;
        for (const char byte : value)
            valid = valid && ((byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f'));
        if (valid && self) {
            self->m_authenticatedClient = client;
            astrea_shell_auth_manager_v1_send_authenticated(resource);
        } else {
            astrea_shell_auth_manager_v1_send_rejected(resource);
        }
        if (self)
            self->flushClients();
    }

    static void destroyAuthManagerRequest(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    static void bindCaptureManager(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *self = static_cast<FakeScreenCaptureCompositor *>(data);
        auto *manager = new CaptureManager;
        manager->owner = self;
        manager->resource = wl_resource_create(
            client, &astrea_screen_capture_manager_v1_interface, std::min(version, 1u), id);
        if (!manager->resource) {
            delete manager;
            return;
        }
        wl_resource_set_implementation(manager->resource, &kCaptureManagerImplementation,
                                       manager, &destroyCaptureManager);
        self->m_captureManager = manager;
    }

    static void destroyCaptureManager(wl_resource *resource)
    {
        auto *manager = static_cast<CaptureManager *>(wl_resource_get_user_data(resource));
        if (manager && manager->owner && manager->owner->m_captureManager == manager) {
            manager->owner->m_captureManager = nullptr;
            manager->owner->m_managerDestroyed = true;
        }
        delete manager;
    }

    static void captureOutputRequest(wl_client *client, wl_resource *managerResource,
                                     uint32_t id, wl_resource *output)
    {
        auto *manager = static_cast<CaptureManager *>(wl_resource_get_user_data(managerResource));
        auto *self = manager ? manager->owner : nullptr;
        if (!self)
            return;
        auto *capture = new Capture;
        capture->owner = self;
        capture->output = output;
        capture->resource = wl_resource_create(client, &astrea_screen_capture_v1_interface, 1, id);
        if (!capture->resource) {
            delete capture;
            return;
        }
        wl_resource_set_implementation(capture->resource, &kCaptureImplementation, capture,
                                       &destroyCapture);
        self->m_lastCapture = capture;
        self->m_lastOutput = output;
        ++self->m_captureRequestCount;
    }

    static void destroyCaptureOutputRequest(wl_client *, wl_resource *resource)
    {
        wl_resource_destroy(resource);
    }

    static void destroyCapture(wl_resource *resource)
    {
        auto *capture = static_cast<Capture *>(wl_resource_get_user_data(resource));
        if (capture && capture->owner && capture->owner->m_lastCapture == capture)
            capture->owner->m_lastCapture = nullptr;
        delete capture;
    }

    static void bindOutput(wl_client *client, void *data, uint32_t version, uint32_t id)
    {
        auto *self = static_cast<FakeScreenCaptureCompositor *>(data);
        auto *output = new Output;
        output->owner = self;
        output->resource = wl_resource_create(client, &wl_output_interface, std::min(version, 4u), id);
        if (!output->resource) {
            delete output;
            return;
        }
        wl_resource_set_implementation(output->resource, &kOutputImplementation, output,
                                       &destroyOutput);
        self->m_output = output;
    }

    static void destroyOutput(wl_resource *resource)
    {
        auto *output = static_cast<Output *>(wl_resource_get_user_data(resource));
        if (output && output->owner && output->owner->m_output == output)
            output->owner->m_output = nullptr;
        delete output;
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

    static const struct astrea_shell_auth_manager_v1_interface kAuthManagerImplementation;
    static const struct astrea_screen_capture_manager_v1_interface kCaptureManagerImplementation;
    static const struct astrea_screen_capture_v1_interface kCaptureImplementation;
    static const struct wl_output_interface kOutputImplementation;

    QTemporaryDir m_runtime;
    QByteArray m_previousRuntime;
    QByteArray m_previousDisplay;
    QByteArray m_previousCapability;
    QString m_socketName;
    QString m_capabilityPath;
    wl_display *m_display = nullptr;
    wl_event_loop *m_loop = nullptr;
    wl_global *m_authGlobal = nullptr;
    wl_global *m_captureGlobal = nullptr;
    wl_global *m_outputGlobal = nullptr;
    QSocketNotifier *m_serverNotifier = nullptr;
    AuthManager *m_authManager = nullptr;
    CaptureManager *m_captureManager = nullptr;
    Capture *m_lastCapture = nullptr;
    Output *m_output = nullptr;
    wl_client *m_authenticatedClient = nullptr;
    wl_resource *m_lastOutput = nullptr;
    int m_captureRequestCount = 0;
    bool m_managerDestroyed = false;
    QObject m_readyFileParent;
    std::unique_ptr<QTemporaryFile> m_readyFile;
};

const struct astrea_shell_auth_manager_v1_interface
    FakeScreenCaptureCompositor::kAuthManagerImplementation = {
        &FakeScreenCaptureCompositor::authenticateRequest,
        &FakeScreenCaptureCompositor::destroyAuthManagerRequest,
    };

const struct astrea_screen_capture_manager_v1_interface
    FakeScreenCaptureCompositor::kCaptureManagerImplementation = {
        &FakeScreenCaptureCompositor::captureOutputRequest,
        &FakeScreenCaptureCompositor::destroyCaptureOutputRequest,
    };

const struct astrea_screen_capture_v1_interface
    FakeScreenCaptureCompositor::kCaptureImplementation = {
    &FakeScreenCaptureCompositor::destroyCaptureOutputRequest,
};

const struct wl_output_interface FakeScreenCaptureCompositor::kOutputImplementation = {};

} // namespace

class TyphonScreenCaptureProtocolIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void captureUsesGeneratedRequestOpcodeAndImportsReadyImage();
    void failedEventTerminatesRequestAndAllowsAnotherCapture();
    void malformedReadyPreservesReadinessForNextCapture();
    void disconnectTerminatesOldGenerationAndReconnects();
    void stoppingClientSendsManagerDestroyRequest();
};

void TyphonScreenCaptureProtocolIntegrationTest::captureUsesGeneratedRequestOpcodeAndImportsReadyImage()
{
    FakeScreenCaptureCompositor compositor;
    TyphonSharedConnection connection;
    TyphonScreenCaptureClient client(&connection);
    QSignalSpy readySpy(&client, &TyphonScreenCaptureClient::ready);

    connection.start();
    client.start();
    QVERIFY(compositor.pumpUntil([&client] { return client.isReady(); }));
    QVERIFY(client.requestCapture());
    QVERIFY(compositor.pumpUntil([&compositor] { return compositor.captureRequestCount() == 1; }));
    QCOMPARE(compositor.lastOutput(), compositor.outputResource());

    const char pixelData[] = {1, 2, 3, char(255), 4, 5, 6, char(255)};
    const QByteArray pixels(pixelData, sizeof(pixelData));
    compositor.sendReady(pixels, 2, 1, 8);
    QVERIFY(compositor.pumpUntil([&readySpy] { return readySpy.count() == 1; }));
    const QImage image = qvariant_cast<QImage>(readySpy.at(0).at(0));
    QCOMPARE(image.format(), QImage::Format_RGBA8888);
    QCOMPARE(image.pixelColor(1, 0), QColor(4, 5, 6, 255));
    compositor.clearReadyFile();
    connection.stop();
}

void TyphonScreenCaptureProtocolIntegrationTest::failedEventTerminatesRequestAndAllowsAnotherCapture()
{
    FakeScreenCaptureCompositor compositor;
    TyphonSharedConnection connection;
    TyphonScreenCaptureClient client(&connection);
    QSignalSpy failedSpy(&client, &TyphonScreenCaptureClient::failed);

    connection.start();
    client.start();
    QVERIFY(compositor.pumpUntil([&client] { return client.isReady(); }));
    QVERIFY(client.requestCapture());
    QVERIFY(compositor.pumpUntil([&compositor] { return compositor.captureRequestCount() == 1; }));
    compositor.sendFailed(QStringLiteral("render_failed"));
    QVERIFY(compositor.pumpUntil([&failedSpy] { return failedSpy.count() == 1; }));
    QCOMPARE(failedSpy.at(0).at(0).toString(), QStringLiteral("render_failed"));
    QVERIFY(client.requestCapture());
    QVERIFY(compositor.pumpUntil([&compositor] { return compositor.captureRequestCount() == 2; }));
    connection.stop();
}

void TyphonScreenCaptureProtocolIntegrationTest::malformedReadyPreservesReadinessForNextCapture()
{
    FakeScreenCaptureCompositor compositor;
    TyphonSharedConnection connection;
    TyphonScreenCaptureClient client(&connection);
    QSignalSpy failedSpy(&client, &TyphonScreenCaptureClient::failed);

    connection.start();
    client.start();
    QVERIFY(compositor.pumpUntil([&client] { return client.isReady(); }));
    QVERIFY(client.requestCapture());
    QVERIFY(compositor.pumpUntil([&compositor] { return compositor.captureRequestCount() == 1; }));

    compositor.sendReady(QByteArray(4, '\xff'), 2, 1, 8);
    QVERIFY(compositor.pumpUntil([&failedSpy] { return failedSpy.count() == 1; }));
    QCOMPARE(failedSpy.at(0).at(0).toString(), QStringLiteral("malformed screenshot payload"));
    QVERIFY(client.isReady());

    QVERIFY(client.requestCapture());
    QVERIFY(compositor.pumpUntil([&compositor] { return compositor.captureRequestCount() == 2; }));
    compositor.sendFailed(QStringLiteral("render_failed"));
    QVERIFY(compositor.pumpUntil([&failedSpy] { return failedSpy.count() == 2; }));
    QCOMPARE(failedSpy.at(1).at(0).toString(), QStringLiteral("render_failed"));
    QVERIFY(client.isReady());
    connection.stop();
}

void TyphonScreenCaptureProtocolIntegrationTest::disconnectTerminatesOldGenerationAndReconnects()
{
    FakeScreenCaptureCompositor compositor;
    TyphonSharedConnection connection;
    TyphonScreenCaptureClient client(&connection);
    QSignalSpy failedSpy(&client, &TyphonScreenCaptureClient::failed);

    connection.start();
    client.start();
    QVERIFY(compositor.pumpUntil([&client] { return client.isReady(); }));
    const auto oldGeneration = client.connectionGeneration();
    QVERIFY(client.requestCapture());
    QVERIFY(compositor.pumpUntil([&compositor] { return compositor.captureRequestCount() == 1; }));
    compositor.disconnectClients();
    QVERIFY(compositor.pumpUntil([&failedSpy] { return failedSpy.count() == 1; }));
    QCOMPARE(failedSpy.at(0).at(0).toString(), QStringLiteral("disconnected"));
    QCOMPARE(failedSpy.at(0).at(1).toULongLong(), oldGeneration);

    connection.reconnectNowForTest();
    QVERIFY(compositor.pumpUntil([&client] {
        return client.isReady() && client.connectionGeneration() > 1;
    }));
    QVERIFY(client.requestCapture());
    QVERIFY(compositor.pumpUntil([&compositor] { return compositor.captureRequestCount() == 2; }));
    compositor.sendFailed(QStringLiteral("render_failed"));
    QVERIFY(compositor.pumpUntil([&failedSpy] { return failedSpy.count() == 2; }));
    connection.stop();
}

void TyphonScreenCaptureProtocolIntegrationTest::stoppingClientSendsManagerDestroyRequest()
{
    FakeScreenCaptureCompositor compositor;
    TyphonSharedConnection connection;
    TyphonScreenCaptureClient client(&connection);

    connection.start();
    client.start();
    QVERIFY(compositor.pumpUntil([&client] { return client.isReady(); }));
    client.stop();
    QVERIFY(connection.flush());
    QVERIFY(compositor.pumpUntil([&compositor] { return compositor.managerDestroyed(); }));
    connection.stop();
}

QTEST_MAIN(TyphonScreenCaptureProtocolIntegrationTest)
#include "TyphonScreenCaptureProtocolIntegrationTest.moc"
