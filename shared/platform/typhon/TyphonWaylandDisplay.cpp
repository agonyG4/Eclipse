#include "platform/typhon/TyphonWaylandDisplay.hpp"

#include <QSocketNotifier>

#include <cerrno>
#include <memory>

#if ASTREA_HAVE_TYPHON_WAYLAND
#include <wayland-client-core.h>
#endif

struct TyphonWaylandDisplay::Private {
#if ASTREA_HAVE_TYPHON_WAYLAND
    wl_display *display = nullptr;
    QSocketNotifier *readNotifier = nullptr;
    QSocketNotifier *writeNotifier = nullptr;
    bool readPrepared = false;
#endif
};

TyphonWaylandDisplay::TyphonWaylandDisplay(QObject *parent)
    : QObject(parent), m_private(std::make_unique<Private>())
{
}

TyphonWaylandDisplay::~TyphonWaylandDisplay()
{
    disconnectFromDisplay();
}

bool TyphonWaylandDisplay::connectToDisplay(const QString &displayName)
{
#if !ASTREA_HAVE_TYPHON_WAYLAND
    Q_UNUSED(displayName);
    return false;
#else
    if (m_private->display)
        return true;

    const QByteArray encodedName = displayName.toUtf8();
    m_private->display = wl_display_connect(displayName.isEmpty() ? nullptr : encodedName.constData());
    if (!m_private->display)
        return false;

    const int fd = wl_display_get_fd(m_private->display);
    if (fd < 0) {
        wl_display_disconnect(m_private->display);
        m_private->display = nullptr;
        return false;
    }

    m_private->readNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    m_private->writeNotifier = new QSocketNotifier(fd, QSocketNotifier::Write, this);
    m_private->writeNotifier->setEnabled(false);
    connect(m_private->readNotifier, &QSocketNotifier::activated,
            this, [this](QSocketDescriptor, QSocketNotifier::Type) { onReadable(); });
    connect(m_private->writeNotifier, &QSocketNotifier::activated,
            this, [this](QSocketDescriptor, QSocketNotifier::Type) { onWritable(); });
    emit connected();
    return true;
#endif
}

void TyphonWaylandDisplay::disconnectFromDisplay()
{
#if ASTREA_HAVE_TYPHON_WAYLAND
    if (!m_private->display)
        return;
    if (m_private->readPrepared) {
        wl_display_cancel_read(m_private->display);
        m_private->readPrepared = false;
    }
    delete m_private->readNotifier;
    delete m_private->writeNotifier;
    m_private->readNotifier = nullptr;
    m_private->writeNotifier = nullptr;
    wl_display_disconnect(m_private->display);
    m_private->display = nullptr;
    emit disconnected();
#endif
}

int TyphonWaylandDisplay::fileDescriptor() const
{
#if ASTREA_HAVE_TYPHON_WAYLAND
    return m_private->display ? wl_display_get_fd(m_private->display) : -1;
#else
    return -1;
#endif
}

bool TyphonWaylandDisplay::dispatchPending()
{
#if ASTREA_HAVE_TYPHON_WAYLAND
    if (!m_private->display)
        return false;
    if (wl_display_dispatch_pending(m_private->display) < 0) {
        fail(QStringLiteral("Wayland pending dispatch failed"));
        return false;
    }
    armRead();
    return true;
#else
    return false;
#endif
}

bool TyphonWaylandDisplay::flush()
{
#if ASTREA_HAVE_TYPHON_WAYLAND
    if (!m_private->display)
        return false;
    for (;;) {
        errno = 0;
        if (wl_display_flush(m_private->display) >= 0) {
            if (m_private->writeNotifier)
                m_private->writeNotifier->setEnabled(false);
            armRead();
            return true;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN) {
            if (m_private->writeNotifier)
                m_private->writeNotifier->setEnabled(true);
            armRead();
            return true;
        }
        fail(QStringLiteral("Wayland request flush failed (errno %1, display error %2, fd %3)")
                 .arg(errno)
                 .arg(wl_display_get_error(m_private->display))
                 .arg(wl_display_get_fd(m_private->display)));
        return false;
    }
#else
    return false;
#endif
}

bool TyphonWaylandDisplay::isConnected() const
{
#if ASTREA_HAVE_TYPHON_WAYLAND
    return m_private->display != nullptr;
#else
    return false;
#endif
}

wl_display *TyphonWaylandDisplay::nativeDisplay() const
{
#if ASTREA_HAVE_TYPHON_WAYLAND
    return m_private->display;
#else
    return nullptr;
#endif
}

void TyphonWaylandDisplay::armRead()
{
#if ASTREA_HAVE_TYPHON_WAYLAND
    if (!m_private->display || m_private->readPrepared)
        return;
    while (wl_display_prepare_read(m_private->display) != 0) {
        if (wl_display_dispatch_pending(m_private->display) < 0) {
            fail(QStringLiteral("Wayland pending dispatch failed while preparing read"));
            return;
        }
    }
    m_private->readPrepared = true;
#endif
}

void TyphonWaylandDisplay::onReadable()
{
#if ASTREA_HAVE_TYPHON_WAYLAND
    if (!m_private->display)
        return;
    if (!m_private->readPrepared) {
        dispatchPending();
        return;
    }
    if (wl_display_read_events(m_private->display) < 0) {
        fail(QStringLiteral("Wayland event read failed"));
        return;
    }
    m_private->readPrepared = false;
    if (wl_display_dispatch_pending(m_private->display) < 0) {
        fail(QStringLiteral("Wayland pending dispatch failed after read"));
        return;
    }
    emit readable();
    flush();
    armRead();
#endif
}

void TyphonWaylandDisplay::onWritable()
{
    flush();
}

void TyphonWaylandDisplay::fail(const QString &diagnostic)
{
    emit protocolError(diagnostic);
    disconnectFromDisplay();
}
