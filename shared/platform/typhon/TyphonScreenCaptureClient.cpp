#include "platform/typhon/TyphonScreenCaptureClient.hpp"

#include "platform/typhon/TyphonSharedConnection.hpp"
#include "platform/typhon/TyphonWaylandDisplay.hpp"

#include <QDebug>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <limits>
#include <utility>

#if ASTREA_HAVE_TYPHON_PROTOCOL
#include "astrea-screen-capture-v1-client-protocol.h"

#include <wayland-client.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

constexpr std::uint32_t kRgba8888 = 0x34324152U;
constexpr std::uint64_t kMaxPayload = 256ULL * 1024ULL * 1024ULL;

} // namespace

struct TyphonScreenCaptureClient::Private {
    explicit Private(TyphonScreenCaptureClient *owner, TyphonSharedConnection *sharedConnection)
        : owner(owner), sharedConnection(sharedConnection)
    {
        display = sharedConnection ? sharedConnection->waylandDisplay() : nullptr;
    }

    TyphonScreenCaptureClient *owner = nullptr;
    TyphonSharedConnection *sharedConnection = nullptr;
    TyphonWaylandDisplay *display = nullptr;
    bool started = false;
    bool ready = false;
    std::uint64_t generation = 0;

#if ASTREA_HAVE_TYPHON_PROTOCOL
    wl_registry *registry = nullptr;
    wl_callback *registrySync = nullptr;
    wl_output *output = nullptr;
    std::uint32_t outputName = 0;
    bool outputNameValid = false;
    astrea_screen_capture_manager_v1 *manager = nullptr;
    std::uint32_t managerName = 0;
    bool managerNameValid = false;
    astrea_screen_capture_v1 *capture = nullptr;

    void destroyObjects(bool sendDestroy);
    void bindGeneration(std::uint64_t nextGeneration);
    void fail(const QString &reason);
    void failRequest(const QString &reason);
    void handleReady(int fd, std::uint32_t width, std::uint32_t height,
                     std::uint32_t stride, std::uint32_t format);

    static void global(void *data, wl_registry *registry, std::uint32_t name,
                       const char *interfaceName, std::uint32_t version);
    static void globalRemove(void *, wl_registry *, std::uint32_t);
    static void synchronized(void *data, wl_callback *callback, std::uint32_t);
    static void readyEvent(void *data, astrea_screen_capture_v1 *, int fd,
                           std::uint32_t width, std::uint32_t height,
                           std::uint32_t stride, std::uint32_t format);
    static void failedEvent(void *data, astrea_screen_capture_v1 *, const char *reason);

    static const wl_registry_listener registryListener;
    static const wl_callback_listener registrySyncListener;
    static const astrea_screen_capture_v1_listener captureListener;
#endif
};

#if ASTREA_HAVE_TYPHON_PROTOCOL
void TyphonScreenCaptureClient::Private::global(void *data, wl_registry *registry,
                                                std::uint32_t name, const char *interfaceName,
                                                std::uint32_t version)
{
    auto *self = static_cast<Private *>(data);
    if (!self || !self->started || self->registry != registry)
        return;
    if (!self->manager && qstrcmp(interfaceName, "astrea_screen_capture_manager_v1") == 0
        && version >= 1) {
        self->manager = static_cast<astrea_screen_capture_manager_v1 *>(wl_registry_bind(
            registry, name, &astrea_screen_capture_manager_v1_interface, 1));
        self->managerName = name;
        self->managerNameValid = self->manager != nullptr;
    } else if (!self->output && qstrcmp(interfaceName, "wl_output") == 0 && version >= 1) {
        self->output = static_cast<wl_output *>(
            wl_registry_bind(registry, name, &wl_output_interface, std::min(version, 4U)));
        self->outputName = name;
        self->outputNameValid = self->output != nullptr;
    }
}

void TyphonScreenCaptureClient::Private::globalRemove(void *data, wl_registry *, std::uint32_t name)
{
    auto *self = static_cast<Private *>(data);
    if (!self || !self->started)
        return;
    if (self->outputNameValid && self->outputName == name) {
        if (self->capture)
            self->fail(QStringLiteral("output_gone"));
        else
            self->ready = false;
        if (self->output)
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(self->output));
        self->output = nullptr;
        self->outputNameValid = false;
        self->owner->stateChanged(QStringLiteral("output_gone"));
    } else if (self->managerNameValid && self->managerName == name) {
        if (self->capture)
            self->fail(QStringLiteral("unsupported"));
        else
            self->ready = false;
        if (self->manager)
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(self->manager));
        self->manager = nullptr;
        self->managerNameValid = false;
        self->owner->stateChanged(QStringLiteral("unsupported"));
    }
}

void TyphonScreenCaptureClient::Private::synchronized(void *data, wl_callback *callback,
                                                      std::uint32_t)
{
    auto *self = static_cast<Private *>(data);
    if (!self || self->registrySync != callback)
        return;
    self->registrySync = nullptr;
    wl_callback_destroy(callback);
    if (!self->started || !self->manager || !self->output) {
        if (self->started)
            self->owner->diagnostic(QStringLiteral("Typhon screen capture output is unavailable"));
        return;
    }
    self->ready = true;
    self->owner->stateChanged(QStringLiteral("ready"));
}

void TyphonScreenCaptureClient::Private::readyEvent(void *data, astrea_screen_capture_v1 *, int fd,
                                                    std::uint32_t width, std::uint32_t height,
                                                    std::uint32_t stride, std::uint32_t format)
{
    auto *self = static_cast<Private *>(data);
    if (self)
        self->handleReady(fd, width, height, stride, format);
    else if (fd >= 0)
        ::close(fd);
}

void TyphonScreenCaptureClient::Private::failedEvent(void *data, astrea_screen_capture_v1 *,
                                                     const char *reason)
{
    auto *self = static_cast<Private *>(data);
    if (self && self->started)
        self->failRequest(QString::fromUtf8(reason ? reason : "render_failed"));
}

void TyphonScreenCaptureClient::Private::handleReady(int fd, std::uint32_t width,
                                                     std::uint32_t height, std::uint32_t stride,
                                                     std::uint32_t format)
{
    auto closeFd = [fd] {
        if (fd >= 0)
            ::close(fd);
    };
    if (!started || !capture || fd < 0 || format != kRgba8888 || width == 0 || height == 0) {
        closeFd();
        if (started)
            failRequest(QStringLiteral("malformed screenshot metadata"));
        return;
    }
    const std::uint64_t minimumStride = static_cast<std::uint64_t>(width) * 4ULL;
    const std::uint64_t payload = static_cast<std::uint64_t>(stride) * height;
    if (stride < minimumStride || stride > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
        || payload > kMaxPayload
        || payload > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        closeFd();
        failRequest(QStringLiteral("malformed screenshot metadata"));
        return;
    }
    struct stat metadata {};
    if (::fstat(fd, &metadata) != 0 || metadata.st_size < 0
        || static_cast<std::uint64_t>(metadata.st_size) != payload) {
        closeFd();
        failRequest(QStringLiteral("malformed screenshot payload"));
        return;
    }
    void *mapped = ::mmap(nullptr, static_cast<std::size_t>(payload), PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        closeFd();
        failRequest(QStringLiteral("screenshot image mapping failed"));
        return;
    }
    const QImage view(static_cast<const uchar *>(mapped), static_cast<int>(width),
                      static_cast<int>(height), static_cast<int>(stride), QImage::Format_RGBA8888);
    const QImage image = view.copy();
    ::munmap(mapped, static_cast<std::size_t>(payload));
    closeFd();
    if (image.isNull()) {
        failRequest(QStringLiteral("screenshot image import failed"));
        return;
    }
    astrea_screen_capture_v1_destroy(capture);
    capture = nullptr;
    emit owner->ready(image, generation);
}

void TyphonScreenCaptureClient::Private::fail(const QString &reason)
{
    if (!started)
        return;
    if (capture) {
        astrea_screen_capture_v1_destroy(capture);
        capture = nullptr;
    }
    ready = false;
    emit owner->failed(reason, generation);
}

void TyphonScreenCaptureClient::Private::failRequest(const QString &reason)
{
    if (!started)
        return;
    if (capture) {
        astrea_screen_capture_v1_destroy(capture);
        capture = nullptr;
    }
    ready = manager != nullptr && output != nullptr;
    emit owner->failed(reason, generation);
}

void TyphonScreenCaptureClient::Private::destroyObjects(bool sendDestroy)
{
    const bool transportAlive = sharedConnection && sharedConnection->nativeDisplay();
    ready = false;
    if (capture) {
        if (sendDestroy)
            astrea_screen_capture_v1_destroy(capture);
        else if (transportAlive)
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(capture));
        capture = nullptr;
    }
    if (registrySync) {
        if (sendDestroy)
            wl_callback_destroy(registrySync);
        else if (transportAlive)
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(registrySync));
        registrySync = nullptr;
    }
    if (manager) {
        if (sendDestroy)
            astrea_screen_capture_manager_v1_destroy(manager);
        else if (transportAlive)
            wl_proxy_destroy(reinterpret_cast<wl_proxy *>(manager));
        manager = nullptr;
    }
    if (output && transportAlive) {
        wl_proxy_destroy(reinterpret_cast<wl_proxy *>(output));
        output = nullptr;
    }
    output = nullptr;
    outputNameValid = false;
    if (registry && transportAlive) {
        wl_registry_destroy(registry);
        registry = nullptr;
    }
    registry = nullptr;
    managerNameValid = false;
}

void TyphonScreenCaptureClient::Private::bindGeneration(std::uint64_t nextGeneration)
{
    generation = nextGeneration;
    destroyObjects(false);
    display = sharedConnection->waylandDisplay();
    if (!started || !display || !sharedConnection->nativeDisplay()) {
        owner->stateChanged(QStringLiteral("disconnected"));
        return;
    }
    registry = wl_display_get_registry(sharedConnection->nativeDisplay());
    if (!registry || wl_registry_add_listener(registry, &registryListener, this) != 0) {
        fail(QStringLiteral("Typhon screen capture registry setup failed"));
        return;
    }
    registrySync = wl_display_sync(sharedConnection->nativeDisplay());
    if (!registrySync || wl_callback_add_listener(registrySync, &registrySyncListener, this) != 0) {
        fail(QStringLiteral("Typhon screen capture registry synchronization failed"));
        return;
    }
    owner->stateChanged(QStringLiteral("connecting"));
    if (!sharedConnection->flush())
        fail(QStringLiteral("Typhon screen capture registry flush failed"));
}

const wl_registry_listener TyphonScreenCaptureClient::Private::registryListener = {
    &TyphonScreenCaptureClient::Private::global,
    &TyphonScreenCaptureClient::Private::globalRemove,
};

const wl_callback_listener TyphonScreenCaptureClient::Private::registrySyncListener = {
    &TyphonScreenCaptureClient::Private::synchronized,
};

const astrea_screen_capture_v1_listener TyphonScreenCaptureClient::Private::captureListener = {
    &TyphonScreenCaptureClient::Private::readyEvent,
    &TyphonScreenCaptureClient::Private::failedEvent,
};
#endif

TyphonScreenCaptureClient::TyphonScreenCaptureClient(TyphonSharedConnection *sharedConnection,
                                                     QObject *parent)
    : QObject(parent), m_private(std::make_unique<Private>(this, sharedConnection))
{
    if (sharedConnection) {
        connect(sharedConnection, &TyphonSharedConnection::ready, this,
                [this](quint64 generation) {
#if ASTREA_HAVE_TYPHON_PROTOCOL
                    if (m_private->started)
                        m_private->bindGeneration(generation);
#else
                    Q_UNUSED(generation);
#endif
                });
        connect(sharedConnection, &TyphonSharedConnection::disconnected, this,
                [this](quint64 generation) {
                    if (!m_private->started || generation != m_private->generation)
                        return;
                    const std::uint64_t oldGeneration = m_private->generation;
                    bool hadCapture = false;
#if ASTREA_HAVE_TYPHON_PROTOCOL
                    hadCapture = m_private->capture != nullptr;
                    m_private->destroyObjects(false);
#endif
                    m_private->ready = false;
                    emit stateChanged(QStringLiteral("disconnected"));
                    if (hadCapture)
                        emit failed(QStringLiteral("disconnected"), oldGeneration);
                });
    }
}

TyphonScreenCaptureClient::~TyphonScreenCaptureClient()
{
    stop();
}

void TyphonScreenCaptureClient::start()
{
    if (m_private->started)
        return;
    m_private->started = true;
#if ASTREA_HAVE_TYPHON_PROTOCOL
    if (m_private->sharedConnection && m_private->sharedConnection->isReady())
        m_private->bindGeneration(m_private->sharedConnection->connectionGeneration());
    else
#endif
        emit stateChanged(QStringLiteral("waiting"));
}

void TyphonScreenCaptureClient::stop()
{
    if (!m_private->started)
        return;
    m_private->started = false;
#if ASTREA_HAVE_TYPHON_PROTOCOL
    const bool canSendDestroy = m_private->sharedConnection
        && m_private->sharedConnection->isReady() && m_private->sharedConnection->nativeDisplay();
    m_private->destroyObjects(canSendDestroy);
#endif
    ++m_private->generation;
    emit stateChanged(QStringLiteral("stopped"));
}

bool TyphonScreenCaptureClient::requestCapture()
{
#if !ASTREA_HAVE_TYPHON_PROTOCOL
    emit failed(QStringLiteral("unsupported"), m_private->generation);
    return false;
#else
    if (!m_private->started || !m_private->ready || !m_private->manager || !m_private->output
        || m_private->capture) {
        emit failed(m_private->capture ? QStringLiteral("busy") : QStringLiteral("unsupported"),
                    m_private->generation);
        return false;
    }
    m_private->capture = astrea_screen_capture_manager_v1_capture_output(m_private->manager,
                                                                           m_private->output);
    if (!m_private->capture
        || astrea_screen_capture_v1_add_listener(m_private->capture, &Private::captureListener,
                                                 m_private.get())
               != 0) {
        m_private->fail(QStringLiteral("render_failed"));
        return false;
    }
    if (!m_private->sharedConnection->flush()) {
        m_private->fail(QStringLiteral("render_failed"));
        return false;
    }
    return true;
#endif
}

void TyphonScreenCaptureClient::cancelCapture()
{
#if ASTREA_HAVE_TYPHON_PROTOCOL
    if (m_private->capture) {
        astrea_screen_capture_v1_destroy(m_private->capture);
        m_private->capture = nullptr;
    }
#endif
}

bool TyphonScreenCaptureClient::isReady() const
{
    return m_private->ready;
}

std::uint64_t TyphonScreenCaptureClient::connectionGeneration() const
{
    return m_private->generation;
}
