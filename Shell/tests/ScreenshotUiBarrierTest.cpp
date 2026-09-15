#include <QCoreApplication>
#include <QQuickWindow>
#include <QtTest/QtTest>

#include "screenshot/ScreenshotUiBarrier.hpp"

class ScreenshotUiBarrierTest final : public QObject {
    Q_OBJECT

private slots:
    void hiddenWindowSynchronizationIsDeferredPastVisibleChanged();
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

QTEST_MAIN(ScreenshotUiBarrierTest)
#include "ScreenshotUiBarrierTest.moc"
