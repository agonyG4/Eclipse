#include <QCoreApplication>
#include <QQuickWindow>
#include <QtTest/QtTest>

#include <array>
#include <sys/socket.h>
#include <unistd.h>

#if defined(ASTREA_HAVE_WAYLAND_EFFECTS) && ASTREA_HAVE_WAYLAND_EFFECTS
#include <wayland-client.h>
#endif

#include "screenshot/ScreenshotUiBarrier.hpp"

class ScreenshotUiBarrierTest final : public QObject {
    Q_OBJECT

private slots:
    void hiddenWindowSynchronizationIsDeferredPastVisibleChanged();
    void nullAndDestroyedTrackedWindowsAreSafe();
    void cancelInvalidatesQueuedSynchronizationAndAllowsAnotherRequest();
    void cancelDestroysOutstandingWaylandCallback();
    void waylandCallbackRechecksVisibilityBeforeCompleting();
};

void ScreenshotUiBarrierTest::hiddenWindowSynchronizationIsDeferredPastVisibleChanged()
{
    QQuickWindow window;
    window.show();
    QVERIFY(window.isVisible());

    ScreenshotWaylandUiBarrier barrier;
    barrier.setTrackedWindows({&window});

    bool completed = false;
    barrier.synchronize([&completed](bool, QString) { completed = true; });
    QVERIFY(!completed);

    window.hide();
    QVERIFY(!completed);

    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QVERIFY(completed);
}

void ScreenshotUiBarrierTest::nullAndDestroyedTrackedWindowsAreSafe()
{
    ScreenshotWaylandUiBarrier barrier;
    barrier.setTrackedWindows({nullptr, nullptr, nullptr});
    QVERIFY(barrier.trackedWindowsAreHidden());

    auto *window = new QQuickWindow;
    barrier.setTrackedWindows({window, nullptr, nullptr});
    QVERIFY(barrier.trackedWindowsAreHidden());
    delete window;
    QVERIFY(barrier.trackedWindowsAreHidden());

    barrier.setTrackedWindows({nullptr, nullptr, nullptr});
    QVERIFY(barrier.trackedWindowsAreHidden());
}

void ScreenshotUiBarrierTest::cancelInvalidatesQueuedSynchronizationAndAllowsAnotherRequest()
{
    QQuickWindow window;
    ScreenshotWaylandUiBarrier barrier;
    barrier.setTrackedWindows({&window});
    int cancelledCompletionCount = 0;
    int nextCompletionCount = 0;

    barrier.synchronize([&cancelledCompletionCount](bool, QString) {
        ++cancelledCompletionCount;
    });
    barrier.cancel();
    barrier.cancel();

    window.show();
    barrier.synchronize([&nextCompletionCount](bool, QString) {
        ++nextCompletionCount;
    });
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCOMPARE(cancelledCompletionCount, 0);
    QCOMPARE(nextCompletionCount, 0);

    window.hide();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QCOMPARE(cancelledCompletionCount, 0);
    QCOMPARE(nextCompletionCount, 1);
}

void ScreenshotUiBarrierTest::cancelDestroysOutstandingWaylandCallback()
{
#if defined(ASTREA_HAVE_WAYLAND_EFFECTS) && ASTREA_HAVE_WAYLAND_EFFECTS
    struct DisplayPair {
        wl_display *client = nullptr;
        int peerFd = -1;
        ~DisplayPair()
        {
            if (client)
                wl_display_disconnect(client);
            if (peerFd >= 0)
                ::close(peerFd);
        }
    } displayPair;
    int sockets[2] = {-1, -1};
    QVERIFY(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) == 0);
    displayPair.client = wl_display_connect_to_fd(sockets[0]);
    displayPair.peerFd = sockets[1];
    QVERIFY(displayPair.client);

    ScreenshotWaylandUiBarrier barrier;
    barrier.m_display = displayPair.client;
    int completionCount = 0;
    barrier.synchronize([&completionCount](bool, QString) { ++completionCount; });
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QVERIFY(barrier.m_callback);
    QVERIFY(barrier.m_callbackContext);

    barrier.cancel();
    barrier.cancel();
    QVERIFY(!barrier.m_callback);
    QVERIFY(!barrier.m_callbackContext);
    QCOMPARE(completionCount, 0);
#else
    QSKIP("Wayland display support is unavailable in this build");
#endif
}

void ScreenshotUiBarrierTest::waylandCallbackRechecksVisibilityBeforeCompleting()
{
#if defined(ASTREA_HAVE_WAYLAND_EFFECTS) && ASTREA_HAVE_WAYLAND_EFFECTS
    struct DisplayPair {
        wl_display *client = nullptr;
        int peerFd = -1;
        ~DisplayPair()
        {
            if (client)
                wl_display_disconnect(client);
            if (peerFd >= 0)
                ::close(peerFd);
        }
    } displayPair;
    int sockets[2] = {-1, -1};
    QVERIFY(::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) == 0);
    displayPair.client = wl_display_connect_to_fd(sockets[0]);
    displayPair.peerFd = sockets[1];
    QVERIFY(displayPair.client);

    QQuickWindow window;
    ScreenshotWaylandUiBarrier barrier;
    barrier.setTrackedWindows({&window});
    barrier.m_display = displayPair.client;
    int completionCount = 0;
    barrier.synchronize([&completionCount](bool success, QString) {
        QVERIFY(success);
        ++completionCount;
    });
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QVERIFY(barrier.m_callback);

    const auto sendDone = [&displayPair](wl_callback *callback) {
        const std::array<std::uint32_t, 3> event{
            wl_proxy_get_id(reinterpret_cast<wl_proxy *>(callback)), 12U << 16, 1};
        return ::send(displayPair.peerFd, event.data(), sizeof(event), MSG_NOSIGNAL);
    };
    window.show();
    QCOMPARE(sendDone(barrier.m_callback), static_cast<ssize_t>(3 * sizeof(std::uint32_t)));
    QCOMPARE(wl_display_dispatch(displayPair.client), 1);
    QCOMPARE(completionCount, 0);
    QVERIFY(barrier.m_completion);

    window.hide();
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    QVERIFY(barrier.m_callback);
    QCOMPARE(sendDone(barrier.m_callback), static_cast<ssize_t>(3 * sizeof(std::uint32_t)));
    QCOMPARE(wl_display_dispatch(displayPair.client), 1);
    QCOMPARE(completionCount, 1);
#else
    QSKIP("Wayland display support is unavailable in this build");
#endif
}

QTEST_MAIN(ScreenshotUiBarrierTest)
#include "ScreenshotUiBarrierTest.moc"
