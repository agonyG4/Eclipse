#include "screenshot/ScreenshotController.hpp"

#include "platform/typhon/TyphonScreenCaptureClient.hpp"

#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

#include <cmath>
#include <utility>

namespace {

QRectF normalizedRect(const QRectF &rect)
{
    return rect.normalized();
}

bool isCaptureWaiting(ScreenshotController::ScreenshotPhase phase)
{
    return phase == ScreenshotController::ScreenshotPhase::WaitingCapture
        || phase == ScreenshotController::ScreenshotPhase::WaitingLiveCapture;
}

} // namespace

ScreenshotController::ScreenshotController(TyphonScreenCaptureClient *client, QObject *parent)
    : QObject(parent), m_client(client)
{
    m_watchdog.setSingleShot(true);
    if (!m_client)
        return;
    connect(m_client, &TyphonScreenCaptureClient::ready, this,
            &ScreenshotController::handleCaptureReady);
    connect(m_client, &TyphonScreenCaptureClient::failed, this,
            &ScreenshotController::handleCaptureFailed);
}

QString ScreenshotController::imageSource() const
{
    return QStringLiteral("image://astrea-screenshot/current?generation=%1")
        .arg(m_imageGeneration);
}

QString ScreenshotController::resultUri() const
{
    if (m_resultPath.isEmpty())
        return {};
    return QUrl::fromLocalFile(m_resultPath).toString(QUrl::FullyEncoded);
}

QString ScreenshotController::phaseName() const
{
    switch (m_phase) {
    case ScreenshotPhase::Idle: return QStringLiteral("idle");
    case ScreenshotPhase::PreparingCapture: return QStringLiteral("preparing_capture");
    case ScreenshotPhase::WaitingCapture: return QStringLiteral("waiting_capture");
    case ScreenshotPhase::FrozenSelecting: return QStringLiteral("frozen_selecting");
    case ScreenshotPhase::LiveSelecting: return QStringLiteral("live_selecting");
    case ScreenshotPhase::WaitingLiveCapture: return QStringLiteral("waiting_live_capture");
    }
    return QStringLiteral("unknown");
}

QString ScreenshotController::intentName() const
{
    switch (m_intent) {
    case ScreenshotIntent::QuickFullscreen: return QStringLiteral("quick_fullscreen");
    case ScreenshotIntent::FrozenRegion: return QStringLiteral("frozen_region");
    case ScreenshotIntent::LiveRegion: return QStringLiteral("live_region");
    }
    return QStringLiteral("unknown");
}

bool ScreenshotController::captureClientReady() const
{
    return m_client && m_client->isReady();
}

int ScreenshotController::operationDeadlineRemainingMs() const
{
    if (!m_watchdog.isActive() || !m_watchdogClock.isValid())
        return 0;
    return qMax(0, m_deadlineMs - static_cast<int>(m_watchdogClock.elapsed()));
}

bool ScreenshotController::startQuickCapture()
{
    return begin(ScreenshotIntent::QuickFullscreen);
}

bool ScreenshotController::startFrozenRegionCapture()
{
    return begin(ScreenshotIntent::FrozenRegion);
}

bool ScreenshotController::startLiveRegionCapture()
{
    return begin(ScreenshotIntent::LiveRegion);
}

bool ScreenshotController::capture()
{
    return startFrozenRegionCapture();
}

bool ScreenshotController::begin(ScreenshotIntent intent)
{
    if (m_phase != ScreenshotPhase::Idle) {
        ++m_rejectedOperationCount;
        qInfo().noquote() << "Screenshot request rejected while operation is active; phase="
                          << phaseName();
        return false;
    }
    if (!m_client && !m_captureRequest) {
        qWarning() << "Screenshot request rejected: capture client unavailable";
        return false;
    }

    const bool oldUiVisible = m_thumbnailVisible || m_previewVisible || m_visible;
    disarmWatchdog();
    ++m_operationToken;
    const std::uint64_t token = m_operationToken;
    qInfo().noquote() << "Screenshot operation accepted; intent="
                      << (intent == ScreenshotIntent::QuickFullscreen ? "quick_fullscreen"
                          : intent == ScreenshotIntent::FrozenRegion ? "frozen_region"
                                                                     : "live_region");
    if (m_intent != intent) {
        m_intent = intent;
        emit intentChanged();
        if (token != m_operationToken)
            return true;
    }
    m_selection = {};
    m_pendingSelection = {};
    emit selectionChanged();
    if (token != m_operationToken)
        return true;
    setVisible(false);
    if (token != m_operationToken)
        return true;
    setSelectionImageVisible(false);
    if (token != m_operationToken)
        return true;
    setThumbnailVisible(false);
    if (token != m_operationToken)
        return true;
    setPreviewVisible(false);
    if (token != m_operationToken)
        return true;
    setPhase(ScreenshotPhase::PreparingCapture);
    if (token != m_operationToken || m_phase != ScreenshotPhase::PreparingCapture)
        return true;
    armWatchdog(m_uiTimeoutMs);

    if (oldUiVisible && m_uiBarrier) {
        m_uiBarrier->synchronize(
            [this, token](bool success, QString reason) {
                handleUiBarrierFinished(token, success, std::move(reason));
            });
    } else {
        handleUiBarrierFinished(token, true, {});
    }
    return true;
}

void ScreenshotController::handleUiBarrierFinished(std::uint64_t token, bool success,
                                                   QString reason)
{
    if (token != m_operationToken)
        return;

    if (m_phase == ScreenshotPhase::PreparingCapture) {
        disarmWatchdog();
        if (!success) {
            failOperation(reason.isEmpty() ? QStringLiteral("screenshot UI synchronization failed")
                                           : reason,
                          token, QStringLiteral("ui_synchronization"));
            return;
        }
        if (m_intent == ScreenshotIntent::LiveRegion) {
            setPhase(ScreenshotPhase::LiveSelecting);
            if (token != m_operationToken || m_phase != ScreenshotPhase::LiveSelecting)
                return;
            setSelectionImageVisible(false);
            if (token != m_operationToken || m_phase != ScreenshotPhase::LiveSelecting)
                return;
            setVisible(true);
            return;
        }
        requestCapture(token);
        return;
    }

    if (m_phase != ScreenshotPhase::WaitingLiveCapture)
        return;
    disarmWatchdog();
    if (!success) {
        failOperation(reason.isEmpty() ? QStringLiteral("screenshot UI synchronization failed")
                                       : reason,
                      token, QStringLiteral("ui_synchronization"));
        return;
    }
    requestCapture(token);
}

void ScreenshotController::requestCapture(std::uint64_t token)
{
    if (token != m_operationToken || (m_phase != ScreenshotPhase::PreparingCapture
                                      && m_phase != ScreenshotPhase::WaitingLiveCapture))
        return;

    disarmWatchdog();
    setPhase(ScreenshotPhase::WaitingCapture);
    if (token != m_operationToken || m_phase != ScreenshotPhase::WaitingCapture)
        return;
    m_requestGeneration = m_captureRequest ? m_captureGeneration
                                           : m_client->connectionGeneration();
    m_requestOperationToken = token;
    armWatchdog(m_captureTimeoutMs);
    const bool accepted = m_captureRequest ? m_captureRequest(token)
                                           : m_client->requestCapture(token);
    if (!accepted && token == m_operationToken && isCaptureWaiting(m_phase))
        failOperation(QStringLiteral("capture unavailable"), token,
                      QStringLiteral("capture_request"));
}

void ScreenshotController::handleCaptureReady(QImage image, std::uint64_t generation,
                                               std::uint64_t operationToken)
{
    if (m_phase != ScreenshotPhase::WaitingCapture || generation != m_requestGeneration
        || operationToken != m_requestOperationToken || operationToken != m_operationToken)
        return;
    const std::uint64_t token = operationToken;
    disarmWatchdog();
    if (image.isNull()) {
        failOperation(QStringLiteral("empty screenshot"), token,
                      QStringLiteral("capture_response"));
        return;
    }

    if (m_intent == ScreenshotIntent::QuickFullscreen) {
        persistResult(image, token);
        return;
    }

    m_image = image.copy();
    ++m_imageGeneration;
    emit imageChanged();
    if (token != m_operationToken || m_phase != ScreenshotPhase::WaitingCapture)
        return;

    if (m_intent == ScreenshotIntent::FrozenRegion) {
        setPhase(ScreenshotPhase::FrozenSelecting);
        if (token != m_operationToken || m_phase != ScreenshotPhase::FrozenSelecting)
            return;
        setSelectionImageVisible(true);
        if (token != m_operationToken || m_phase != ScreenshotPhase::FrozenSelecting)
            return;
        setVisible(true);
        return;
    }

    const QRect mapped = mappedSelection(m_image, m_pendingSelection, m_pendingOverlayWidth,
                                         m_pendingOverlayHeight);
    const bool tiny = mapped.width() < 6 || mapped.height() < 6;
    const QImage result = tiny ? m_image : m_image.copy(mapped);
    persistResult(result, token);
}

void ScreenshotController::handleCaptureFailed(QString reason, std::uint64_t generation,
                                                std::uint64_t operationToken)
{
    if (!isCaptureWaiting(m_phase) || generation != m_requestGeneration
        || operationToken != m_requestOperationToken || operationToken != m_operationToken)
        return;
    failOperation(reason, operationToken, QStringLiteral("capture_response"));
}

void ScreenshotController::setPhase(ScreenshotPhase phase)
{
    if (m_phase == phase)
        return;
    m_phase = phase;
    emit phaseChanged();
}

void ScreenshotController::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
    emit visibleChanged();
}

void ScreenshotController::setSelectionImageVisible(bool visible)
{
    if (m_selectionImageVisible == visible)
        return;
    m_selectionImageVisible = visible;
    emit selectionImageVisibleChanged();
}

void ScreenshotController::setThumbnailVisible(bool visible)
{
    if (m_thumbnailVisible == visible)
        return;
    m_thumbnailVisible = visible;
    emit thumbnailVisibleChanged();
}

void ScreenshotController::setPreviewVisible(bool visible)
{
    if (m_previewVisible == visible)
        return;
    m_previewVisible = visible;
    emit previewVisibleChanged();
}

void ScreenshotController::failOperation(const QString &reason, std::uint64_t token,
                                         const QString &stage)
{
    if (token != m_operationToken)
        return;
    const ScreenshotPhase failedPhase = m_phase;
    disarmWatchdog();
    if ((failedPhase == ScreenshotPhase::PreparingCapture
         || failedPhase == ScreenshotPhase::WaitingLiveCapture) && m_uiBarrier) {
        m_uiBarrier->cancel();
    }
    if (isCaptureWaiting(failedPhase) && m_client)
        m_client->cancelCapture();
    m_lastFailureReason = reason;
    m_lastFailureStage = stage;
    qWarning().noquote() << "Astrea screenshot failed at" << stage << ":" << reason;
    returnToIdle();
    emit captureFailed(reason);
}

void ScreenshotController::returnToIdle()
{
    disarmWatchdog();
    const bool phaseChangedValue = m_phase != ScreenshotPhase::Idle;
    const bool visibleChangedValue = m_visible;
    const bool selectionImageChangedValue = m_selectionImageVisible;
    const bool selectionChangedValue = !m_selection.isNull() || !m_pendingSelection.isNull();
    ++m_operationToken;
    const std::uint64_t idleToken = m_operationToken;
    m_phase = ScreenshotPhase::Idle;
    m_visible = false;
    m_selectionImageVisible = false;
    m_selection = {};
    m_pendingSelection = {};
    if (phaseChangedValue)
        emit phaseChanged();
    if (idleToken != m_operationToken)
        return;
    if (visibleChangedValue)
        emit visibleChanged();
    if (idleToken != m_operationToken)
        return;
    if (selectionImageChangedValue)
        emit selectionImageVisibleChanged();
    if (idleToken != m_operationToken)
        return;
    if (selectionChangedValue)
        emit selectionChanged();
}

void ScreenshotController::cancel()
{
    disarmWatchdog();
    if (m_uiBarrier)
        m_uiBarrier->cancel();
    if (isCaptureWaiting(m_phase) && m_client)
        m_client->cancelCapture();
    returnToIdle();
}

void ScreenshotController::armWatchdog(int timeoutMs)
{
    m_deadlineMs = qMax(1, timeoutMs);
    m_watchdogClock.restart();
    QObject::disconnect(m_watchdogConnection);
    const std::uint64_t token = m_operationToken;
    const std::uint64_t generation = ++m_watchdogGeneration;
    m_watchdogConnection = connect(&m_watchdog, &QTimer::timeout, this,
                                   [this, token, generation] {
                                       handleWatchdogExpired(token, generation);
                                   });
    m_watchdog.start(m_deadlineMs);
}

void ScreenshotController::disarmWatchdog()
{
    m_watchdog.stop();
    ++m_watchdogGeneration;
    QObject::disconnect(m_watchdogConnection);
    m_watchdogConnection = {};
    m_watchdogClock.invalidate();
    m_deadlineMs = 0;
}

void ScreenshotController::handleWatchdogExpired(std::uint64_t operationToken,
                                                  std::uint64_t watchdogGeneration)
{
    if (operationToken != m_operationToken || watchdogGeneration != m_watchdogGeneration)
        return;
    const ScreenshotPhase expiredPhase = m_phase;
    const int timeoutMs = m_deadlineMs;
    QString stage;
    QString label;
    if (expiredPhase == ScreenshotPhase::PreparingCapture
        || expiredPhase == ScreenshotPhase::WaitingLiveCapture) {
        stage = QStringLiteral("ui_synchronization_timeout");
        label = QStringLiteral("screenshot UI synchronization");
    } else if (expiredPhase == ScreenshotPhase::WaitingCapture) {
        stage = QStringLiteral("compositor_capture_timeout");
        label = QStringLiteral("compositor capture");
    } else {
        disarmWatchdog();
        return;
    }
    const QString reason = QStringLiteral("%1 timed out after %2 ms").arg(label).arg(timeoutMs);
    qWarning().noquote() << "Astrea screenshot watchdog expired at" << stage << ":" << reason;
    failOperation(reason, operationToken, stage);
}

void ScreenshotController::setOverlaySize(qreal width, qreal height)
{
    m_overlayWidth = qMax<qreal>(width, 1.0);
    m_overlayHeight = qMax<qreal>(height, 1.0);
}

void ScreenshotController::setSelection(const QRectF &selection)
{
    const QRectF next = normalizedRect(selection);
    if (m_selection == next)
        return;
    m_selection = next;
    emit selectionChanged();
}

void ScreenshotController::setThumbnailRect(const QRectF &rect)
{
    const QRectF next = normalizedRect(rect);
    if (m_thumbnailRect == next)
        return;
    m_thumbnailRect = next;
    emit thumbnailRectChanged();
}

QRect ScreenshotController::mappedSelection() const
{
    return mappedSelection(m_image, m_selection, m_overlayWidth, m_overlayHeight);
}

QRect ScreenshotController::mappedSelection(const QImage &image, const QRectF &selection,
                                            qreal width, qreal height) const
{
    if (image.isNull() || width <= 0.0 || height <= 0.0)
        return {};
    const QRectF normalized = normalizedRect(selection);
    const qreal scaleX = static_cast<qreal>(image.width()) / width;
    const qreal scaleY = static_cast<qreal>(image.height()) / height;
    const QRectF mapped(normalized.left() * scaleX, normalized.top() * scaleY,
                        normalized.width() * scaleX, normalized.height() * scaleY);
    const int left = qBound(0, static_cast<int>(std::floor(mapped.left())), image.width());
    const int top = qBound(0, static_cast<int>(std::floor(mapped.top())), image.height());
    const int right = qBound(left, static_cast<int>(std::ceil(mapped.right())), image.width());
    const int bottom = qBound(top, static_cast<int>(std::ceil(mapped.bottom())), image.height());
    return QRect(left, top, qMax(0, right - left), qMax(0, bottom - top));
}

QString ScreenshotController::nextOutputPath() const
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
        + QStringLiteral("/Capturas de tela/");
    QDir().mkpath(directory);
    const QString stem = QStringLiteral("Captura de tela %1")
                             .arg(QDateTime::currentDateTime().toString(
                                 QStringLiteral("yyyy-MM-dd_HH-mm-ss")));
    QString path = directory + stem + QStringLiteral(".png");
    for (int suffix = 1; QFile::exists(path); ++suffix)
        path = directory + stem + QStringLiteral(" (%1).png").arg(suffix);
    return path;
}

bool ScreenshotController::persistResult(const QImage &result, std::uint64_t token)
{
    if (token != m_operationToken || result.isNull()) {
        if (token == m_operationToken)
            failOperation(QStringLiteral("invalid screenshot image"), token,
                          QStringLiteral("persistence"));
        return false;
    }

    const QString path = nextOutputPath();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !result.save(&file, "PNG") || !file.commit()) {
        failOperation(QStringLiteral("screenshot save failed"), token,
                      QStringLiteral("persistence"));
        return false;
    }
    const bool resultChangedValue = m_resultPath != path;
    const bool phaseChangedValue = m_phase != ScreenshotPhase::Idle;
    const bool visibleChangedValue = m_visible;
    const bool selectionImageChangedValue = m_selectionImageVisible;
    const bool selectionChangedValue = !m_selection.isNull() || !m_pendingSelection.isNull();
    const bool thumbnailChangedValue = !m_thumbnailVisible;
    m_image = result.copy();
    ++m_imageGeneration;
    m_resultPath = path;
    disarmWatchdog();
    ++m_operationToken;
    const std::uint64_t completedOperationToken = m_operationToken;
    m_phase = ScreenshotPhase::Idle;
    m_visible = false;
    m_selectionImageVisible = false;
    m_selection = {};
    m_pendingSelection = {};
    m_thumbnailVisible = true;

    if (auto *clipboard = QGuiApplication::clipboard())
        clipboard->setImage(result);

    emit imageChanged();
    if (resultChangedValue)
        emit resultChanged();
    emit saved(path);
    if (completedOperationToken != m_operationToken)
        return true;
    if (phaseChangedValue)
        emit phaseChanged();
    if (completedOperationToken != m_operationToken)
        return true;
    if (visibleChangedValue)
        emit visibleChanged();
    if (completedOperationToken != m_operationToken)
        return true;
    if (selectionImageChangedValue)
        emit selectionImageVisibleChanged();
    if (completedOperationToken != m_operationToken)
        return true;
    if (selectionChangedValue)
        emit selectionChanged();
    if (completedOperationToken != m_operationToken)
        return true;
    if (thumbnailChangedValue)
        emit thumbnailVisibleChanged();
    return true;
}

bool ScreenshotController::saveSelection()
{
    return finishSelection();
}

bool ScreenshotController::finishSelection()
{
    if (m_phase == ScreenshotPhase::FrozenSelecting) {
        const QRect mapped = mappedSelection();
        const bool tiny = mapped.width() < 6 || mapped.height() < 6;
        const QImage result = tiny ? m_image : m_image.copy(mapped);
        return persistResult(result, m_operationToken);
    }
    if (m_phase != ScreenshotPhase::LiveSelecting)
        return false;

    m_pendingSelection = normalizedRect(m_selection);
    m_pendingOverlayWidth = m_overlayWidth;
    m_pendingOverlayHeight = m_overlayHeight;
    const std::uint64_t token = m_operationToken;
    setVisible(false);
    if (token != m_operationToken || m_phase != ScreenshotPhase::LiveSelecting)
        return false;
    setSelectionImageVisible(false);
    if (token != m_operationToken || m_phase != ScreenshotPhase::LiveSelecting)
        return false;
    setPhase(ScreenshotPhase::WaitingLiveCapture);
    if (token != m_operationToken || m_phase != ScreenshotPhase::WaitingLiveCapture)
        return false;
    armWatchdog(m_uiTimeoutMs);
    if (m_uiBarrier) {
        m_uiBarrier->synchronize(
            [this, token](bool success, QString reason) {
                handleUiBarrierFinished(token, success, std::move(reason));
            });
    } else {
        handleUiBarrierFinished(token, true, {});
    }
    return true;
}

void ScreenshotController::dismissThumbnail()
{
    setThumbnailVisible(false);
}

void ScreenshotController::openPreview()
{
    if (m_phase != ScreenshotPhase::Idle || m_image.isNull() || m_resultPath.isEmpty())
        return;
    setThumbnailVisible(false);
    setPreviewVisible(true);
}

void ScreenshotController::closePreview()
{
    if (!m_previewVisible)
        return;
    setPreviewVisible(false);
    if (!m_resultPath.isEmpty())
        setThumbnailVisible(true);
}
