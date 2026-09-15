#include <QFile>
#include <QImage>
#include <QUrl>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTest>

#include "platform/typhon/TyphonScreenCaptureClient.hpp"
#include "platform/typhon/TyphonSharedConnection.hpp"
#include "screenshot/ScreenshotController.hpp"

class FakeScreenshotUiBarrier final : public ScreenshotUiBarrier {
public:
    void synchronize(Completion completion) override
    {
        ++requestCount;
        pending = std::move(completion);
    }

    void complete(bool success, const QString &reason = {})
    {
        QVERIFY(pending);
        Completion completion = std::move(pending);
        completion(success, reason);
    }

    int requestCount = 0;
    Completion pending;
};

class ScreenshotControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void overlayStartsHiddenAndAcceptsCompleteImage();
    void quickCapturePersistsResultWithoutShowingSelection();
    void frozenCaptureSelectsAndCropsCapturedImage();
    void liveCaptureWaitsForUiBarrierBeforeRequestingImage();
    void conflictingModeIsRejectedWhileCaptureIsActive();
    void barrierFailureReturnsToIdle();
    void cancelInvalidatesPendingBarrier();
    void staleCaptureGenerationIsIgnored();
    void newCaptureIsAllowedWhileThumbnailIsVisible();
    void selectionMapsToPixelsAndClamps();
    void tinySelectionFallsBackToFullImage();
    void outputPathUsesDeterministicCollisionSuffixes();
    void staleOrInactiveFailureDoesNotCancelScreenshotRequest();
    void resultUriCanonicalizesEncodedLocalPath();

private:
    void configureFakeCapture(ScreenshotController &controller, int &requestCount,
                              std::uint64_t generation = 1);
};

namespace {

void removeSavedFile(const QSignalSpy &savedSpy)
{
    if (!savedSpy.isEmpty())
        QFile::remove(savedSpy.constLast().at(0).toString());
}

} // namespace

void ScreenshotControllerTest::configureFakeCapture(ScreenshotController &controller,
                                                     int &requestCount,
                                                     std::uint64_t generation)
{
    controller.m_captureGeneration = generation;
    controller.m_captureRequest = [&requestCount] {
        ++requestCount;
        return true;
    };
}

void ScreenshotControllerTest::overlayStartsHiddenAndAcceptsCompleteImage()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    QVERIFY(!controller.isVisible());
    controller.m_intent = ScreenshotIntent::FrozenRegion;
    controller.m_phase = ScreenshotPhase::WaitingCapture;
    controller.m_requestGeneration = client.connectionGeneration();
    controller.handleCaptureReady(QImage(12, 8, QImage::Format_RGBA8888),
                                  client.connectionGeneration());
    QVERIFY(controller.isVisible());
    QCOMPARE(controller.image().size(), QSize(12, 8));
    QVERIFY(controller.imageSource().contains(QStringLiteral("generation=1")));
}

void ScreenshotControllerTest::quickCapturePersistsResultWithoutShowingSelection()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    int requestCount = 0;
    configureFakeCapture(controller, requestCount);
    QSignalSpy savedSpy(&controller, &ScreenshotController::saved);

    QVERIFY(controller.startQuickCapture());
    QCOMPARE(requestCount, 1);
    QCOMPARE(controller.phase(), ScreenshotPhase::WaitingCapture);

    controller.handleCaptureReady(QImage(12, 8, QImage::Format_RGBA8888), 1);
    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);
    QVERIFY(!controller.isVisible());
    QVERIFY(controller.thumbnailVisible());
    QCOMPARE(controller.image().size(), QSize(12, 8));
    QCOMPARE(savedSpy.count(), 1);
    removeSavedFile(savedSpy);
}

void ScreenshotControllerTest::frozenCaptureSelectsAndCropsCapturedImage()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    int requestCount = 0;
    configureFakeCapture(controller, requestCount);
    QSignalSpy savedSpy(&controller, &ScreenshotController::saved);

    QVERIFY(controller.startFrozenRegionCapture());
    QCOMPARE(requestCount, 1);
    controller.handleCaptureReady(QImage(400, 200, QImage::Format_RGBA8888), 1);
    QCOMPARE(controller.phase(), ScreenshotPhase::FrozenSelecting);
    QVERIFY(controller.isVisible());

    controller.setOverlaySize(200, 100);
    controller.setSelection(QRectF(20, 10, 100, 50));
    QVERIFY(controller.saveSelection());
    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);
    QVERIFY(!controller.isVisible());
    QCOMPARE(controller.image().size(), QSize(200, 100));
    QVERIFY(controller.thumbnailVisible());
    QCOMPARE(savedSpy.count(), 1);
    removeSavedFile(savedSpy);
}

void ScreenshotControllerTest::liveCaptureWaitsForUiBarrierBeforeRequestingImage()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    FakeScreenshotUiBarrier barrier;
    controller.setUiBarrier(&barrier);
    int requestCount = 0;
    configureFakeCapture(controller, requestCount);
    QSignalSpy savedSpy(&controller, &ScreenshotController::saved);

    QVERIFY(controller.startLiveRegionCapture());
    QCOMPARE(controller.phase(), ScreenshotPhase::LiveSelecting);
    QCOMPARE(requestCount, 0);
    QVERIFY(controller.isVisible());
    QVERIFY(!controller.selectionImageVisible());

    controller.setOverlaySize(200, 100);
    controller.setSelection(QRectF(20, 10, 100, 50));
    QVERIFY(controller.finishSelection());
    QCOMPARE(controller.phase(), ScreenshotPhase::WaitingLiveCapture);
    QCOMPARE(barrier.requestCount, 1);
    QCOMPARE(requestCount, 0);
    QVERIFY(!controller.isVisible());

    barrier.complete(true);
    QCOMPARE(requestCount, 1);
    QCOMPARE(controller.phase(), ScreenshotPhase::WaitingCapture);
    controller.handleCaptureReady(QImage(400, 200, QImage::Format_RGBA8888), 1);
    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);
    QCOMPARE(controller.image().size(), QSize(200, 100));
    QVERIFY(controller.thumbnailVisible());
    QCOMPARE(savedSpy.count(), 1);
    removeSavedFile(savedSpy);
}

void ScreenshotControllerTest::conflictingModeIsRejectedWhileCaptureIsActive()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    int requestCount = 0;
    configureFakeCapture(controller, requestCount);

    QVERIFY(controller.startFrozenRegionCapture());
    QVERIFY(!controller.startQuickCapture());
    QCOMPARE(requestCount, 1);
    controller.cancel();
    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);
}

void ScreenshotControllerTest::barrierFailureReturnsToIdle()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    FakeScreenshotUiBarrier barrier;
    controller.setUiBarrier(&barrier);
    QSignalSpy failureSpy(&controller, &ScreenshotController::captureFailed);

    QVERIFY(controller.startLiveRegionCapture());
    controller.setOverlaySize(200, 100);
    controller.setSelection(QRectF(10, 10, 50, 50));
    QVERIFY(controller.finishSelection());
    barrier.complete(false, QStringLiteral("display gone"));

    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);
    QVERIFY(!controller.isVisible());
    QCOMPARE(failureSpy.count(), 1);
    QCOMPARE(failureSpy.at(0).at(0).toString(), QStringLiteral("display gone"));
}

void ScreenshotControllerTest::cancelInvalidatesPendingBarrier()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    FakeScreenshotUiBarrier barrier;
    controller.setUiBarrier(&barrier);
    int requestCount = 0;
    configureFakeCapture(controller, requestCount);

    QVERIFY(controller.startLiveRegionCapture());
    controller.setOverlaySize(200, 100);
    controller.setSelection(QRectF(10, 10, 50, 50));
    QVERIFY(controller.finishSelection());
    controller.cancel();
    barrier.complete(true);

    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);
    QCOMPARE(requestCount, 0);
    QVERIFY(!controller.isVisible());
}

void ScreenshotControllerTest::staleCaptureGenerationIsIgnored()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    int requestCount = 0;
    configureFakeCapture(controller, requestCount, 9);

    QVERIFY(controller.startQuickCapture());
    controller.handleCaptureReady(QImage(4, 4, QImage::Format_RGBA8888), 8);
    QCOMPARE(controller.phase(), ScreenshotPhase::WaitingCapture);
    QVERIFY(controller.image().isNull());
}

void ScreenshotControllerTest::newCaptureIsAllowedWhileThumbnailIsVisible()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    FakeScreenshotUiBarrier barrier;
    controller.setUiBarrier(&barrier);
    int requestCount = 0;
    configureFakeCapture(controller, requestCount);
    QSignalSpy savedSpy(&controller, &ScreenshotController::saved);

    QVERIFY(controller.startQuickCapture());
    controller.handleCaptureReady(QImage(4, 4, QImage::Format_RGBA8888), 1);
    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);
    QVERIFY(controller.thumbnailVisible());
    QVERIFY(controller.startQuickCapture());
    QVERIFY(!controller.thumbnailVisible());
    QCOMPARE(controller.phase(), ScreenshotPhase::PreparingCapture);
    QCOMPARE(barrier.requestCount, 1);
    QCOMPARE(requestCount, 1);
    barrier.complete(true);
    QCOMPARE(requestCount, 2);
    controller.handleCaptureReady(QImage(5, 5, QImage::Format_RGBA8888), 1);
    removeSavedFile(savedSpy);
    removeSavedFile(savedSpy);
}

void ScreenshotControllerTest::selectionMapsToPixelsAndClamps()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    controller.m_image = QImage(400, 200, QImage::Format_RGBA8888);
    controller.setOverlaySize(200, 100);
    controller.setSelection(QRectF(-10, 20, 130, 100));
    QCOMPARE(controller.mappedSelection(), QRect(0, 40, 240, 160));
}

void ScreenshotControllerTest::tinySelectionFallsBackToFullImage()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    controller.m_image = QImage(20, 10, QImage::Format_RGBA8888);
    controller.m_phase = ScreenshotPhase::FrozenSelecting;
    controller.m_operationToken = 1;
    controller.setOverlaySize(20, 10);
    controller.setSelection(QRectF(0, 0, 5, 9));
    QSignalSpy savedSpy(&controller, &ScreenshotController::saved);
    QVERIFY(controller.saveSelection());
    QCOMPARE(savedSpy.count(), 1);
    QFile::remove(savedSpy.at(0).at(0).toString());
    controller.cancel();
}

void ScreenshotControllerTest::outputPathUsesDeterministicCollisionSuffixes()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    const QString first = controller.nextOutputPath();
    QVERIFY(first.endsWith(QStringLiteral(".png")));
    QFile marker(first);
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.close();
    const QString second = controller.nextOutputPath();
    QVERIFY(second.contains(QRegularExpression(QStringLiteral(" \\([0-9]+\\)\\.png$"))));
    QFile::remove(first);
}

void ScreenshotControllerTest::staleOrInactiveFailureDoesNotCancelScreenshotRequest()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    QSignalSpy failureSpy(&controller, &ScreenshotController::captureFailed);

    controller.m_phase = ScreenshotPhase::WaitingCapture;
    controller.m_requestGeneration = 7;
    controller.handleCaptureFailed(QStringLiteral("stale"), 6);
    QVERIFY(controller.m_phase == ScreenshotPhase::WaitingCapture);
    QCOMPARE(failureSpy.count(), 0);

    controller.handleCaptureFailed(QStringLiteral("disconnected"), 7);
    QVERIFY(controller.m_phase == ScreenshotPhase::Idle);
    QCOMPARE(failureSpy.count(), 1);

    controller.handleCaptureFailed(QStringLiteral("late"), 7);
    QCOMPARE(failureSpy.count(), 1);

    controller.m_phase = ScreenshotPhase::WaitingCapture;
    controller.m_requestGeneration = 8;
    controller.handleCaptureFailed(QStringLiteral("old generation"), 7);
    QVERIFY(controller.m_phase == ScreenshotPhase::WaitingCapture);
    QCOMPARE(failureSpy.count(), 1);
}

void ScreenshotControllerTest::resultUriCanonicalizesEncodedLocalPath()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    const QString path = QString::fromUtf8("/tmp/Captura de tela/截图 final.png");
    controller.m_resultPath = path;

    const QString uriText = controller.resultUri();
    const QUrl uri(uriText);
    QVERIFY(uri.isLocalFile());
    QCOMPARE(uri.toLocalFile(), path);
    QVERIFY(uriText.contains(QStringLiteral("%20")));
    QVERIFY(uriText.contains(QStringLiteral("%E6%88%AA")));
}

QTEST_MAIN(ScreenshotControllerTest)
#include "ScreenshotControllerTest.moc"
