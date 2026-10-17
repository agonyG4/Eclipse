#include <QFile>
#include <QClipboard>
#include <QEventLoop>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QUrl>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>
#include <QTimer>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>

#include "platform/typhon/TyphonScreenCaptureClient.hpp"
#include "platform/typhon/TyphonSharedConnection.hpp"
#include "screenshot/ScreenshotConfiguration.hpp"
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

    void cancel() override
    {
        ++cancelCount;
        if (pending)
            cancelled.append(std::move(pending));
    }

    void completeCancelled(bool success, const QString &reason = {})
    {
        QVERIFY(!cancelled.isEmpty());
        Completion completion = std::move(cancelled.last());
        cancelled.removeLast();
        completion(success, reason);
    }

    int requestCount = 0;
    int cancelCount = 0;
    Completion pending;
    QList<Completion> cancelled;
};

class ScreenshotControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void overlayStartsHiddenAndAcceptsCompleteImage();
    void quickCapturePersistsResultWithoutShowingSelection();
    void quickCaptureCopiesPngToClipboard();
    void quickCaptureHonorsAutoCopyAndThumbnailVisibilitySettings();
    void nativeWaylandClipboardPublisherForTyphonIntegration();
    void frozenCaptureSelectsAndCropsCapturedImage();
    void frozenRegionCaptureCopiesCroppedPngToClipboard();
    void failedCapturePreservesPreviousClipboardContents();
    void liveCaptureWaitsForUiBarrierBeforeRequestingImage();
    void conflictingModeIsRejectedWhileCaptureIsActive();
    void barrierFailureReturnsToIdle();
    void cancelInvalidatesPendingBarrier();
    void cancelClearsBarrierAndOldCompletionCannotAffectNextCapture();
    void staleCaptureGenerationIsIgnored();
    void captureWatchdogTimesOutAllowsRetryAndIgnoresLateResponse();
    void uiSynchronizationWatchdogCoversPreparingAndLiveCapture();
    void staleUiWatchdogCannotExpireCaptureDeadline();
    void synchronousCaptureFailureDisarmsWatchdog();
    void interactiveRegionSelectionHasNoCaptureDeadline();
    void newCaptureIsAllowedWhileThumbnailIsVisible();
    void selectionMapsToPixelsAndClamps();
    void tinySelectionFallsBackToFullImage();
    void outputPathUsesDeterministicCollisionSuffixes();
    void staleOrInactiveFailureDoesNotCancelScreenshotRequest();
    void resultUriCanonicalizesEncodedLocalPath();

private:
    void configureFakeCapture(ScreenshotController &controller, int &requestCount,
                              std::uint64_t generation = 1);
    QTemporaryDir m_configHome;
    QByteArray m_previousConfigHome;
};

namespace {

void removeSavedFile(const QSignalSpy &savedSpy)
{
    if (!savedSpy.isEmpty())
        QFile::remove(savedSpy.constLast().at(0).toString());
}

} // namespace

void ScreenshotControllerTest::initTestCase()
{
    QVERIFY(m_configHome.isValid());
    m_previousConfigHome = qgetenv("XDG_CONFIG_HOME");
    qputenv("XDG_CONFIG_HOME", m_configHome.path().toUtf8());
}

void ScreenshotControllerTest::cleanupTestCase()
{
    if (m_previousConfigHome.isNull())
        qunsetenv("XDG_CONFIG_HOME");
    else
        qputenv("XDG_CONFIG_HOME", m_previousConfigHome);
}

void ScreenshotControllerTest::configureFakeCapture(ScreenshotController &controller,
                                                     int &requestCount,
                                                     std::uint64_t generation)
{
    controller.m_captureGeneration = generation;
    controller.m_captureRequest = [&requestCount](std::uint64_t) {
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
                                  client.connectionGeneration(), controller.m_operationToken);
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

    controller.handleCaptureReady(QImage(12, 8, QImage::Format_RGBA8888), 1,
                                  controller.m_requestOperationToken);
    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);
    QVERIFY(!controller.isVisible());
    QVERIFY(controller.thumbnailVisible());
    QCOMPARE(controller.image().size(), QSize(12, 8));
    QCOMPARE(savedSpy.count(), 1);
    removeSavedFile(savedSpy);
}

void ScreenshotControllerTest::quickCaptureCopiesPngToClipboard()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    int requestCount = 0;
    configureFakeCapture(controller, requestCount);
    QSignalSpy savedSpy(&controller, &ScreenshotController::saved);
    QImage image(17, 9, QImage::Format_RGBA8888);
    image.fill(QColor(31, 97, 173, 255));

    QVERIFY(controller.startQuickCapture());
    QCOMPARE(requestCount, 1);
    controller.handleCaptureReady(image, 1, controller.m_requestOperationToken);

    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    QVERIFY(mimeData);
    QVERIFY(mimeData->hasFormat(QStringLiteral("image/png")));
    const QByteArray png = mimeData->data(QStringLiteral("image/png"));
    QVERIFY(png.startsWith(QByteArray::fromHex("89504e470d0a1a0a")));
    const QImage decoded = QImage::fromData(png, "PNG");
    QCOMPARE(decoded.size(), image.size());
    QCOMPARE(decoded.pixelColor(0, 0), image.pixelColor(0, 0));
    QCOMPARE(QGuiApplication::clipboard()->image().size(), image.size());

    controller.dismissThumbnail();
    QCOMPARE(QImage::fromData(QGuiApplication::clipboard()->mimeData()->data(
                                 QStringLiteral("image/png")), "PNG").size(), image.size());
    removeSavedFile(savedSpy);
}

void ScreenshotControllerTest::quickCaptureHonorsAutoCopyAndThumbnailVisibilitySettings()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    const QString configPath = m_configHome.filePath(QStringLiteral("no-auto-copy.json"));
    ScreenshotController controller(&client, nullptr, configPath);
    controller.configuration()->setAutoCopy(false);
    controller.configuration()->setShowThumbnail(false);

    QGuiApplication::clipboard()->setText(QStringLiteral("leave clipboard alone"));
    int requestCount = 0;
    configureFakeCapture(controller, requestCount);
    QSignalSpy savedSpy(&controller, &ScreenshotController::saved);
    QVERIFY(controller.startQuickCapture());
    QCOMPARE(requestCount, 1);
    const QImage capture(14, 9, QImage::Format_RGBA8888);
    controller.handleCaptureReady(capture, controller.m_requestGeneration,
                                  controller.m_requestOperationToken);

    QCOMPARE(savedSpy.count(), 1);
    QVERIFY(!controller.thumbnailVisible());
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("leave clipboard alone"));
    QVERIFY(controller.copyScreenshot());
    QVERIFY(QGuiApplication::clipboard()->mimeData()->hasImage());
    const QByteArray png = QGuiApplication::clipboard()->mimeData()->data(QStringLiteral("image/png"));
    QVERIFY(png.startsWith(QByteArray::fromHex("89504e470d0a1a0a")));
    removeSavedFile(savedSpy);
}

void ScreenshotControllerTest::nativeWaylandClipboardPublisherForTyphonIntegration()
{
    const QByteArray controlPath = qEnvironmentVariable("ASTREA_TEST_CLIPBOARD_CONTROL_SOCKET").toUtf8();
    if (controlPath.isEmpty())
        QSKIP("requires the Typhon Wayland clipboard integration harness");
    QVERIFY(controlPath.size() < static_cast<qsizetype>(sizeof(sockaddr_un::sun_path)));

    const int controlFd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    QVERIFY(controlFd >= 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, controlPath.constData(), static_cast<std::size_t>(controlPath.size()));
    QVERIFY(::connect(controlFd, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == 0);

    const auto writeExact = [controlFd](const char *data, std::size_t size) {
        std::size_t written = 0;
        while (written < size) {
            const ssize_t result = ::send(controlFd, data + written, size - written, MSG_NOSIGNAL);
            if (result < 0 && errno == EINTR)
                continue;
            if (result <= 0)
                return false;
            written += static_cast<std::size_t>(result);
        }
        return true;
    };
    const auto readExact = [controlFd](char *data, std::size_t size) {
        std::size_t received = 0;
        while (received < size) {
            const ssize_t result = ::recv(controlFd, data + received, size - received, 0);
            if (result < 0 && errno == EINTR)
                continue;
            if (result <= 0)
                return false;
            received += static_cast<std::size_t>(result);
        }
        return true;
    };

    QVERIFY(writeExact("R", 1));
    char command = 0;
    QVERIFY(readExact(&command, 1));
    QCOMPARE(command, 'P');

    ScreenshotController controller(nullptr);
    int requestCount = 0;
    configureFakeCapture(controller, requestCount);
    QSignalSpy savedSpy(&controller, &ScreenshotController::saved);
    QImage image(3, 2, QImage::Format_RGBA8888);
    image.fill(QColor(20, 80, 140, 255));
    image.setPixelColor(2, 1, QColor(230, 40, 10, 255));

    QVERIFY(controller.startQuickCapture());
    QCOMPARE(requestCount, 1);
    controller.handleCaptureReady(image, 1, controller.m_requestOperationToken);
    const QString expectedPicturesDirectory = qEnvironmentVariable("ASTREA_TEST_PICTURES_DIR");
    QVERIFY(!expectedPicturesDirectory.isEmpty());
    QVERIFY(controller.resultPath().startsWith(expectedPicturesDirectory));
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    QVERIFY(mimeData->hasFormat(QStringLiteral("image/png")));
    const QByteArray png = mimeData->data(QStringLiteral("image/png"));
    const QImage decoded = QImage::fromData(png, "PNG");
    QCOMPARE(decoded.size(), image.size());
    QCOMPARE(decoded.pixelColor(0, 0), QColor(20, 80, 140, 255));
    QCOMPARE(decoded.pixelColor(2, 1), QColor(230, 40, 10, 255));

    QImage replacement(4, 3, QImage::Format_RGBA8888);
    replacement.fill(QColor(60, 150, 35, 255));
    replacement.setPixelColor(3, 2, QColor(240, 25, 90, 255));
    QVERIFY(controller.startQuickCapture());
    QCOMPARE(requestCount, 2);
    controller.handleCaptureReady(replacement, 1, controller.m_requestOperationToken);
    QCOMPARE(savedSpy.count(), 2);
    QVERIFY(controller.resultPath().startsWith(expectedPicturesDirectory));
    const QImage replacementClipboard = QImage::fromData(
        QGuiApplication::clipboard()->mimeData()->data(QStringLiteral("image/png")), "PNG");
    QCOMPARE(replacementClipboard.size(), replacement.size());
    QCOMPARE(replacementClipboard.pixelColor(0, 0), QColor(60, 150, 35, 255));
    QCOMPARE(replacementClipboard.pixelColor(3, 2), QColor(240, 25, 90, 255));
    controller.dismissThumbnail();
    QVERIFY(!controller.thumbnailVisible());
    QVERIFY(controller.startQuickCapture());
    QCOMPARE(requestCount, 3);
    controller.handleCaptureFailed(QStringLiteral("integration failure"), 1,
                                   controller.m_requestOperationToken);
    const QImage preservedClipboard = QImage::fromData(
        QGuiApplication::clipboard()->mimeData()->data(QStringLiteral("image/png")), "PNG");
    QCOMPARE(preservedClipboard.size(), replacement.size());
    QCOMPARE(preservedClipboard.pixelColor(3, 2), QColor(240, 25, 90, 255));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    QVERIFY(writeExact("D", 1));

    bool destinationDecodedTransferredPng = false;
    QEventLoop transferLoop;
    QSocketNotifier notifier(controlFd, QSocketNotifier::Read);
    connect(&notifier, &QSocketNotifier::activated, &transferLoop,
            [&](QSocketDescriptor, QSocketNotifier::Type) {
                char response = 0;
                if (!readExact(&response, 1) || response != 'V') {
                    transferLoop.quit();
                    return;
                }
                std::uint32_t length = 0;
                if (!readExact(reinterpret_cast<char *>(&length), sizeof(length))
                    || length == 0 || length > 1024 * 1024) {
                    transferLoop.quit();
                    return;
                }
                QByteArray transferred(static_cast<qsizetype>(length), Qt::Uninitialized);
                if (!readExact(transferred.data(), length)) {
                    transferLoop.quit();
                    return;
                }
                const QImage receivedImage = QImage::fromData(transferred, "PNG");
                destinationDecodedTransferredPng = receivedImage.size() == replacement.size()
                    && receivedImage.pixelColor(0, 0) == QColor(60, 150, 35, 255)
                    && receivedImage.pixelColor(3, 2) == QColor(240, 25, 90, 255);
                if (destinationDecodedTransferredPng)
                    writeExact("K", 1);
                transferLoop.quit();
            });
    QTimer::singleShot(10000, &transferLoop, &QEventLoop::quit);
    transferLoop.exec();
    QVERIFY(destinationDecodedTransferredPng);
    for (const auto &entry : savedSpy)
        QFile::remove(entry.at(0).toString());
    ::close(controlFd);
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
    controller.handleCaptureReady(QImage(400, 200, QImage::Format_RGBA8888), 1,
                                  controller.m_requestOperationToken);
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

void ScreenshotControllerTest::frozenRegionCaptureCopiesCroppedPngToClipboard()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    int requestCount = 0;
    configureFakeCapture(controller, requestCount);
    QSignalSpy savedSpy(&controller, &ScreenshotController::saved);
    QImage source(400, 200, QImage::Format_RGBA8888);
    source.fill(QColor(220, 30, 10, 255));
    source.setPixelColor(100, 50, QColor(10, 30, 220, 255));

    QVERIFY(controller.startFrozenRegionCapture());
    controller.handleCaptureReady(source, 1, controller.m_requestOperationToken);
    controller.setOverlaySize(200, 100);
    controller.setSelection(QRectF(20, 10, 100, 50));
    QVERIFY(controller.saveSelection());

    const QByteArray png = QGuiApplication::clipboard()->mimeData()->data(
        QStringLiteral("image/png"));
    const QImage decoded = QImage::fromData(png, "PNG");
    QCOMPARE(decoded.size(), QSize(200, 100));
    QCOMPARE(decoded.pixelColor(0, 0), QColor(220, 30, 10, 255));
    QCOMPARE(decoded.pixelColor(60, 30), QColor(10, 30, 220, 255));
    removeSavedFile(savedSpy);
}

void ScreenshotControllerTest::failedCapturePreservesPreviousClipboardContents()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    int requestCount = 0;
    configureFakeCapture(controller, requestCount);
    QGuiApplication::clipboard()->setText(QStringLiteral("keep this clipboard text"));

    QVERIFY(controller.startQuickCapture());
    controller.handleCaptureFailed(QStringLiteral("capture failed"), 1,
                                   controller.m_requestOperationToken);

    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("keep this clipboard text"));
    QVERIFY(!QGuiApplication::clipboard()->mimeData()->hasFormat(QStringLiteral("image/png")));
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
    QImage capturedImage(400, 200, QImage::Format_RGBA8888);
    capturedImage.fill(QColor(8, 32, 64, 255));
    capturedImage.setPixelColor(120, 70, QColor(220, 40, 10, 255));
    controller.handleCaptureReady(capturedImage, 1, controller.m_requestOperationToken);
    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);
    QCOMPARE(controller.image().size(), QSize(200, 100));
    const QMimeData *mimeData = QGuiApplication::clipboard()->mimeData();
    QVERIFY(mimeData->hasFormat(QStringLiteral("image/png")));
    const QByteArray png = mimeData->data(QStringLiteral("image/png"));
    QVERIFY(png.startsWith(QByteArray::fromHex("89504e470d0a1a0a")));
    const QImage decoded = QImage::fromData(png, "PNG");
    QCOMPARE(decoded.size(), QSize(200, 100));
    QCOMPARE(decoded.pixelColor(0, 0), QColor(8, 32, 64, 255));
    QCOMPARE(decoded.pixelColor(80, 50), QColor(220, 40, 10, 255));
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
    barrier.completeCancelled(true);

    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);
    QCOMPARE(requestCount, 0);
    QVERIFY(!controller.isVisible());
}

void ScreenshotControllerTest::cancelClearsBarrierAndOldCompletionCannotAffectNextCapture()
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
    QCOMPARE(barrier.requestCount, 1);
    QCOMPARE(barrier.cancelCount, 0);

    controller.cancel();
    QCOMPARE(barrier.cancelCount, 1);
    QVERIFY(!barrier.pending);
    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);

    QVERIFY(controller.startQuickCapture());
    QCOMPARE(controller.phase(), ScreenshotPhase::WaitingCapture);
    QCOMPARE(requestCount, 1);
    barrier.completeCancelled(true);
    QCOMPARE(controller.phase(), ScreenshotPhase::WaitingCapture);
    QCOMPARE(requestCount, 1);
}

void ScreenshotControllerTest::staleCaptureGenerationIsIgnored()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    int requestCount = 0;
    configureFakeCapture(controller, requestCount, 9);

    QVERIFY(controller.startQuickCapture());
    controller.handleCaptureReady(QImage(4, 4, QImage::Format_RGBA8888), 8,
                                  controller.m_operationToken);
    QCOMPARE(controller.phase(), ScreenshotPhase::WaitingCapture);
    QVERIFY(controller.image().isNull());
}

void ScreenshotControllerTest::captureWatchdogTimesOutAllowsRetryAndIgnoresLateResponse()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    controller.m_captureTimeoutMs = 25;
    int requestCount = 0;
    configureFakeCapture(controller, requestCount);
    QSignalSpy failureSpy(&controller, &ScreenshotController::captureFailed);
    QSignalSpy savedSpy(&controller, &ScreenshotController::saved);

    QVERIFY(controller.startQuickCapture());
    QCOMPARE(controller.phase(), ScreenshotPhase::WaitingCapture);
    const std::uint64_t timedOutOperation = controller.m_requestOperationToken;
    QTRY_VERIFY_WITH_TIMEOUT(failureSpy.count() == 1, 1000);
    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);
    QCOMPARE(controller.lastFailureStage(), QStringLiteral("compositor_capture_timeout"));
    QVERIFY(controller.lastFailureReason().contains(QStringLiteral("25")));
    QCOMPARE(controller.operationDeadlineRemainingMs(), 0);
    QCOMPARE(controller.m_watchdog.isActive(), false);

    QVERIFY(controller.startQuickCapture());
    QCOMPARE(requestCount, 2);
    QCOMPARE(controller.phase(), ScreenshotPhase::WaitingCapture);
    const std::uint64_t currentOperation = controller.m_requestOperationToken;
    QVERIFY(currentOperation != timedOutOperation);
    controller.handleCaptureReady(QImage(3, 2, QImage::Format_RGBA8888), 1,
                                  timedOutOperation);
    QCOMPARE(controller.phase(), ScreenshotPhase::WaitingCapture);
    QVERIFY(controller.image().isNull());

    controller.handleCaptureReady(QImage(3, 2, QImage::Format_RGBA8888), 1,
                                  currentOperation);
    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);
    QCOMPARE(savedSpy.count(), 1);
    removeSavedFile(savedSpy);
}

void ScreenshotControllerTest::synchronousCaptureFailureDisarmsWatchdog()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    controller.m_captureGeneration = 3;
    QSignalSpy failureSpy(&controller, &ScreenshotController::captureFailed);
    controller.m_captureRequest = [&controller](std::uint64_t token) {
        controller.handleCaptureFailed(QStringLiteral("synchronous failure"), 3, token);
        return false;
    };

    QVERIFY(controller.startQuickCapture());
    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);
    QCOMPARE(failureSpy.count(), 1);
    QVERIFY(!controller.m_watchdog.isActive());
    QCOMPARE(controller.operationDeadlineRemainingMs(), 0);

    controller.m_captureRequest = [](std::uint64_t) { return true; };
    QVERIFY(controller.startQuickCapture());
    QCOMPARE(controller.phase(), ScreenshotPhase::WaitingCapture);
    QVERIFY(controller.m_watchdog.isActive());
    controller.cancel();
    QVERIFY(!controller.m_watchdog.isActive());
}

void ScreenshotControllerTest::uiSynchronizationWatchdogCoversPreparingAndLiveCapture()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    controller.m_uiTimeoutMs = 20;
    FakeScreenshotUiBarrier barrier;
    controller.setUiBarrier(&barrier);
    int requestCount = 0;
    configureFakeCapture(controller, requestCount);
    QSignalSpy failureSpy(&controller, &ScreenshotController::captureFailed);
    QSignalSpy savedSpy(&controller, &ScreenshotController::saved);

    QVERIFY(controller.startQuickCapture());
    controller.handleCaptureReady(QImage(4, 4, QImage::Format_RGBA8888), 1,
                                  controller.m_requestOperationToken);
    QCOMPARE(savedSpy.count(), 1);

    QVERIFY(controller.startQuickCapture());
    QCOMPARE(controller.phase(), ScreenshotPhase::PreparingCapture);
    QTRY_VERIFY_WITH_TIMEOUT(failureSpy.count() == 1, 1000);
    QCOMPARE(controller.lastFailureStage(), QStringLiteral("ui_synchronization_timeout"));
    QCOMPARE(barrier.cancelCount, 1);
    QCOMPARE(requestCount, 1);
    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);

    QVERIFY(controller.startLiveRegionCapture());
    QCOMPARE(controller.phase(), ScreenshotPhase::LiveSelecting);
    controller.setOverlaySize(100, 100);
    controller.setSelection(QRectF(10, 10, 50, 50));
    QVERIFY(controller.finishSelection());
    QCOMPARE(controller.phase(), ScreenshotPhase::WaitingLiveCapture);
    QTRY_VERIFY_WITH_TIMEOUT(failureSpy.count() == 2, 1000);
    QCOMPARE(controller.lastFailureStage(), QStringLiteral("ui_synchronization_timeout"));
    QCOMPARE(barrier.cancelCount, 2);
    QCOMPARE(requestCount, 1);
    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);
    removeSavedFile(savedSpy);
}

void ScreenshotControllerTest::staleUiWatchdogCannotExpireCaptureDeadline()
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
    controller.handleCaptureReady(QImage(4, 4, QImage::Format_RGBA8888), 1,
                                  controller.m_requestOperationToken);
    QVERIFY(controller.startQuickCapture());
    const std::uint64_t operationToken = controller.m_operationToken;
    const std::uint64_t uiWatchdogGeneration = controller.m_watchdogGeneration;
    barrier.complete(true);
    QCOMPARE(controller.phase(), ScreenshotPhase::WaitingCapture);
    QVERIFY(controller.m_watchdogGeneration != uiWatchdogGeneration);

    controller.handleWatchdogExpired(operationToken, uiWatchdogGeneration);
    QCOMPARE(controller.phase(), ScreenshotPhase::WaitingCapture);
    QVERIFY(controller.m_watchdog.isActive());
    controller.cancel();
    removeSavedFile(savedSpy);
}

void ScreenshotControllerTest::interactiveRegionSelectionHasNoCaptureDeadline()
{
    TyphonSharedConnection connection({});
    TyphonScreenCaptureClient client(&connection);
    ScreenshotController controller(&client);
    controller.m_captureTimeoutMs = 20;
    int requestCount = 0;
    configureFakeCapture(controller, requestCount);

    QVERIFY(controller.startFrozenRegionCapture());
    controller.handleCaptureReady(QImage(8, 8, QImage::Format_RGBA8888), 1,
                                  controller.m_requestOperationToken);
    QCOMPARE(controller.phase(), ScreenshotPhase::FrozenSelecting);
    QVERIFY(!controller.m_watchdog.isActive());
    QTest::qWait(40);
    QCOMPARE(controller.phase(), ScreenshotPhase::FrozenSelecting);

    controller.cancel();
    QVERIFY(controller.startLiveRegionCapture());
    QCOMPARE(controller.phase(), ScreenshotPhase::LiveSelecting);
    QVERIFY(!controller.m_watchdog.isActive());
    QTest::qWait(40);
    QCOMPARE(controller.phase(), ScreenshotPhase::LiveSelecting);
    controller.cancel();
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

    QImage firstImage(4, 4, QImage::Format_RGBA8888);
    firstImage.fill(QColor(180, 20, 30, 255));
    QVERIFY(controller.startQuickCapture());
    controller.handleCaptureReady(firstImage, 1, controller.m_requestOperationToken);
    QCOMPARE(controller.phase(), ScreenshotPhase::Idle);
    QVERIFY(controller.thumbnailVisible());
    const QImage firstClipboard = QImage::fromData(
        QGuiApplication::clipboard()->mimeData()->data(QStringLiteral("image/png")), "PNG");
    QCOMPARE(firstClipboard.pixelColor(0, 0), QColor(180, 20, 30, 255));

    QVERIFY(controller.startQuickCapture());
    QVERIFY(!controller.thumbnailVisible());
    QCOMPARE(controller.phase(), ScreenshotPhase::PreparingCapture);
    QCOMPARE(barrier.requestCount, 1);
    QCOMPARE(requestCount, 1);
    barrier.complete(true);
    QCOMPARE(requestCount, 2);
    QImage secondImage(5, 5, QImage::Format_RGBA8888);
    secondImage.fill(QColor(15, 170, 60, 255));
    controller.handleCaptureReady(secondImage, 1, controller.m_requestOperationToken);
    const QImage secondClipboard = QImage::fromData(
        QGuiApplication::clipboard()->mimeData()->data(QStringLiteral("image/png")), "PNG");
    QCOMPARE(secondClipboard.size(), QSize(5, 5));
    QCOMPARE(secondClipboard.pixelColor(0, 0), QColor(15, 170, 60, 255));
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
    controller.handleCaptureFailed(QStringLiteral("stale"), 6, 0);
    QVERIFY(controller.m_phase == ScreenshotPhase::WaitingCapture);
    QCOMPARE(failureSpy.count(), 0);

    controller.handleCaptureFailed(QStringLiteral("disconnected"), 7, 0);
    QVERIFY(controller.m_phase == ScreenshotPhase::Idle);
    QCOMPARE(failureSpy.count(), 1);

    controller.handleCaptureFailed(QStringLiteral("late"), 7, 0);
    QCOMPARE(failureSpy.count(), 1);

    controller.m_phase = ScreenshotPhase::WaitingCapture;
    controller.m_requestGeneration = 8;
    controller.handleCaptureFailed(QStringLiteral("old generation"), 7, 0);
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
