#include "screenshot/ScreenshotUiBarrier.hpp"

#include "platform/wayland/effects/QtWaylandWindow.hpp"

#include <QQuickWindow>

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

ScreenshotWaylandUiBarrier::ScreenshotWaylandUiBarrier(QObject *parent)
    : QObject(parent)
{
}

ScreenshotWaylandUiBarrier::~ScreenshotWaylandUiBarrier()
{
#if defined(ASTREA_HAVE_WAYLAND_EFFECTS) && ASTREA_HAVE_WAYLAND_EFFECTS
    if (m_callback)
        wl_callback_destroy(m_callback);
#endif
    m_callback = nullptr;
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
    if (m_completion) {
        completion(false, QStringLiteral("screenshot UI synchronization already pending"));
        return;
    }
    m_completion = std::move(completion);
    m_display = astreaQtWaylandDisplay();
    checkVisibility();
}

void ScreenshotWaylandUiBarrier::checkVisibility()
{
    if (!m_completion || m_callback || m_syncScheduled)
        return;
    if (m_selectionWindow && m_selectionWindow->isVisible())
        return;
    if (m_thumbnailWindow && m_thumbnailWindow->isVisible())
        return;
    if (m_previewWindow && m_previewWindow->isVisible())
        return;

    m_syncScheduled = true;
    QMetaObject::invokeMethod(this, [this] {
        m_syncScheduled = false;
        issueWaylandSyncIfStillHidden();
    }, Qt::QueuedConnection);
}

void ScreenshotWaylandUiBarrier::issueWaylandSyncIfStillHidden()
{
    if (!m_completion || m_callback)
        return;
    if (m_selectionWindow && m_selectionWindow->isVisible())
        return;
    if (m_thumbnailWindow && m_thumbnailWindow->isVisible())
        return;
    if (m_previewWindow && m_previewWindow->isVisible())
        return;

#if defined(ASTREA_HAVE_WAYLAND_EFFECTS) && ASTREA_HAVE_WAYLAND_EFFECTS
    if (!m_display) {
        complete(false, QStringLiteral("Qt Wayland display unavailable"));
        return;
    }
    m_callback = wl_display_sync(m_display);
    if (!m_callback || wl_callback_add_listener(m_callback, &kCallbackListener, this) != 0) {
        if (m_callback)
            wl_callback_destroy(m_callback);
        m_callback = nullptr;
        complete(false, QStringLiteral("Qt Wayland UI synchronization failed"));
        return;
    }
    errno = 0;
    if (wl_display_flush(m_display) < 0 && errno != EAGAIN) {
        wl_callback_destroy(m_callback);
        m_callback = nullptr;
        complete(false, QStringLiteral("Qt Wayland UI synchronization flush failed"));
    }
#else
    complete(false, QStringLiteral("Qt Wayland display support unavailable"));
#endif
}

void ScreenshotWaylandUiBarrier::complete(bool success, const QString &reason)
{
    if (!m_completion)
        return;
    Completion completion = std::move(m_completion);
    m_syncScheduled = false;
    m_display = nullptr;
    completion(success, reason);
}

void ScreenshotWaylandUiBarrier::callbackDone(void *data, wl_callback *callback,
                                               std::uint32_t)
{
    auto *self = static_cast<ScreenshotWaylandUiBarrier *>(data);
    if (!self || self->m_callback != callback)
        return;
#if defined(ASTREA_HAVE_WAYLAND_EFFECTS) && ASTREA_HAVE_WAYLAND_EFFECTS
    self->m_callback = nullptr;
    wl_callback_destroy(callback);
#else
    Q_UNUSED(callback);
#endif
    self->complete(true);
}
