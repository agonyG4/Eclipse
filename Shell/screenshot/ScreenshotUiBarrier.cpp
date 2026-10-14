#include "screenshot/ScreenshotUiBarrier.hpp"

#include "platform/wayland/effects/QtWaylandWindow.hpp"

#include <QQuickWindow>
#include <QThread>

#include <cerrno>

#if defined(ASTREA_HAVE_WAYLAND_EFFECTS) && ASTREA_HAVE_WAYLAND_EFFECTS
#include <wayland-client.h>
#endif

namespace {

#if defined(ASTREA_HAVE_WAYLAND_EFFECTS) && ASTREA_HAVE_WAYLAND_EFFECTS
const wl_callback_listener kCallbackListener = {
    &ScreenshotWaylandUiBarrier::callbackDone,
};
#endif

} // namespace

struct ScreenshotWaylandUiBarrier::CallbackContext {
    ScreenshotWaylandUiBarrier *owner = nullptr;
    std::uint64_t generation = 0;
};

ScreenshotWaylandUiBarrier::ScreenshotWaylandUiBarrier(QObject *parent)
    : QObject(parent)
{
}

ScreenshotWaylandUiBarrier::~ScreenshotWaylandUiBarrier()
{
    cancel();
}

void ScreenshotWaylandUiBarrier::setTrackedWindows(const QList<QQuickWindow *> &windows)
{
    for (const QMetaObject::Connection &connection : m_windowConnections)
        QObject::disconnect(connection);
    m_windowConnections.clear();

    m_selectionWindow = windows.value(0);
    m_thumbnailWindow = windows.value(1);
    m_previewWindow = windows.value(2);
    for (QQuickWindow *window : windows) {
        if (!window)
            continue;
        m_windowConnections.push_back(connect(window, &QWindow::visibleChanged, this,
                                              [this] { checkVisibility(); }));
    }
    if (m_completion)
        checkVisibility();
}

void ScreenshotWaylandUiBarrier::synchronize(Completion completion)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_completion) {
        completion(false, QStringLiteral("screenshot UI synchronization already pending"));
        return;
    }
    ++m_generation;
    m_completion = std::move(completion);
    if (!m_display)
        m_display = astreaQtWaylandDisplay();
    checkVisibility();
}

void ScreenshotWaylandUiBarrier::cancel()
{
    Q_ASSERT(QThread::currentThread() == thread());
    ++m_generation;
    m_syncScheduled = false;
    m_completion = {};
    m_display = nullptr;
#if defined(ASTREA_HAVE_WAYLAND_EFFECTS) && ASTREA_HAVE_WAYLAND_EFFECTS
    if (m_callback) {
        wl_callback *callback = m_callback;
        m_callback = nullptr;
        wl_callback_destroy(callback);
        delete m_callbackContext;
        m_callbackContext = nullptr;
    } else {
        delete m_callbackContext;
        m_callbackContext = nullptr;
    }
#else
    m_callback = nullptr;
    delete m_callbackContext;
    m_callbackContext = nullptr;
#endif
}

bool ScreenshotWaylandUiBarrier::trackedWindowsAreHidden() const
{
    return (!m_selectionWindow || !m_selectionWindow->isVisible())
        && (!m_thumbnailWindow || !m_thumbnailWindow->isVisible())
        && (!m_previewWindow || !m_previewWindow->isVisible());
}

void ScreenshotWaylandUiBarrier::checkVisibility()
{
    if (!m_completion || m_callback || m_syncScheduled)
        return;
    if (!trackedWindowsAreHidden())
        return;

    m_syncScheduled = true;
    const std::uint64_t generation = m_generation;
    QMetaObject::invokeMethod(this, [this, generation] {
        if (generation != m_generation)
            return;
        m_syncScheduled = false;
        issueWaylandSyncIfStillHidden(generation);
    }, Qt::QueuedConnection);
}

void ScreenshotWaylandUiBarrier::issueWaylandSyncIfStillHidden(std::uint64_t generation)
{
    if (generation != m_generation || !m_completion || m_callback)
        return;
    if (!trackedWindowsAreHidden())
        return;

#if defined(ASTREA_HAVE_WAYLAND_EFFECTS) && ASTREA_HAVE_WAYLAND_EFFECTS
    if (!m_display) {
        complete(generation, false, QStringLiteral("Qt Wayland display unavailable"));
        return;
    }
    m_callback = wl_display_sync(m_display);
    m_callbackContext = new CallbackContext{this, generation};
    if (!m_callback
        || wl_callback_add_listener(m_callback, &kCallbackListener, m_callbackContext) != 0) {
        if (m_callback)
            wl_callback_destroy(m_callback);
        m_callback = nullptr;
        delete m_callbackContext;
        m_callbackContext = nullptr;
        complete(generation, false, QStringLiteral("Qt Wayland UI synchronization failed"));
        return;
    }
    errno = 0;
    if (wl_display_flush(m_display) < 0 && errno != EAGAIN) {
        wl_callback_destroy(m_callback);
        m_callback = nullptr;
        delete m_callbackContext;
        m_callbackContext = nullptr;
        complete(generation, false,
                 QStringLiteral("Qt Wayland UI synchronization flush failed"));
    }
#else
    complete(generation, false, QStringLiteral("Qt Wayland display support unavailable"));
#endif
}

void ScreenshotWaylandUiBarrier::complete(std::uint64_t generation, bool success,
                                          const QString &reason)
{
    if (generation != m_generation || !m_completion)
        return;
    Completion completion = std::move(m_completion);
    ++m_generation;
    m_syncScheduled = false;
    m_display = nullptr;
    completion(success, reason);
}

void ScreenshotWaylandUiBarrier::callbackDone(void *data, wl_callback *callback,
                                               std::uint32_t)
{
    auto *context = static_cast<CallbackContext *>(data);
    auto *self = context ? context->owner : nullptr;
    if (!context) {
#if defined(ASTREA_HAVE_WAYLAND_EFFECTS) && ASTREA_HAVE_WAYLAND_EFFECTS
        wl_callback_destroy(callback);
#else
        Q_UNUSED(callback);
#endif
        return;
    }
    const std::uint64_t generation = context->generation;
    if (!self || self->m_callback != callback || self->m_callbackContext != context
        || generation != self->m_generation) {
#if defined(ASTREA_HAVE_WAYLAND_EFFECTS) && ASTREA_HAVE_WAYLAND_EFFECTS
        wl_callback_destroy(callback);
#else
        Q_UNUSED(callback);
#endif
        delete context;
        return;
    }
#if defined(ASTREA_HAVE_WAYLAND_EFFECTS) && ASTREA_HAVE_WAYLAND_EFFECTS
    self->m_callback = nullptr;
    wl_callback_destroy(callback);
#else
    Q_UNUSED(callback);
#endif
    self->m_callbackContext = nullptr;
    delete context;
    if (!self->trackedWindowsAreHidden()) {
        self->checkVisibility();
        return;
    }
    self->complete(generation, true);
}
