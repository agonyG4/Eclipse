#include "screenshot/ScreenshotController.hpp"

#include "platform/typhon/TyphonScreenCaptureClient.hpp"

#include <QClipboard>
#include <QDateTime>
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
    if (m_phase != ScreenshotPhase::Idle || (!m_client && !m_captureRequest))
        return false;

    const bool oldUiVisible = m_thumbnailVisible || m_previewVisible || m_visible;
    ++m_operationToken;
    const std::uint64_t token = m_operationToken;
    if (m_intent != intent) {
        m_intent = intent;
        emit intentChanged();
    }
    m_selection = {};
    m_pendingSelection = {};
    emit selectionChanged();
    setVisible(false);
    setSelectionImageVisible(false);
    setThumbnailVisible(false);
    setPreviewVisible(false);
    setPhase(ScreenshotPhase::PreparingCapture);

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
        if (!success) {
            failOperation(reason.isEmpty() ? QStringLiteral("screenshot UI synchronization failed")
                                           : reason,
                          token);
            return;
        }
        if (m_intent == ScreenshotIntent::LiveRegion) {
            setPhase(ScreenshotPhase::LiveSelecting);
            setSelectionImageVisible(false);
            setVisible(true);
            return;
        }
        requestCapture(token);
        return;
    }

    if (m_phase != ScreenshotPhase::WaitingLiveCapture)
        return;
    if (!success) {
        failOperation(reason.isEmpty() ? QStringLiteral("screenshot UI synchronization failed")
                                       : reason,
                      token);
        return;
    }
    requestCapture(token);
}

void ScreenshotController::requestCapture(std::uint64_t token)
{
    if (token != m_operationToken || (m_phase != ScreenshotPhase::PreparingCapture
                                      && m_phase != ScreenshotPhase::WaitingLiveCapture))
        return;

    setPhase(ScreenshotPhase::WaitingCapture);
    m_requestGeneration = m_captureRequest ? m_captureGeneration
                                           : m_client->connectionGeneration();
    const bool accepted = m_captureRequest ? m_captureRequest() : m_client->requestCapture();
    if (!accepted && token == m_operationToken && isCaptureWaiting(m_phase))
        failOperation(QStringLiteral("capture unavailable"), token);
}

void ScreenshotController::handleCaptureReady(QImage image, std::uint64_t generation)
{
    if (m_phase != ScreenshotPhase::WaitingCapture || generation != m_requestGeneration)
        return;
    const std::uint64_t token = m_operationToken;
    if (image.isNull()) {
        failOperation(QStringLiteral("empty screenshot"), token);
        return;
    }

    if (m_intent == ScreenshotIntent::QuickFullscreen) {
        persistResult(image, token);
        return;
    }

    m_image = image.copy();
    ++m_imageGeneration;
    emit imageChanged();

    if (m_intent == ScreenshotIntent::FrozenRegion) {
        setPhase(ScreenshotPhase::FrozenSelecting);
        setSelectionImageVisible(true);
        setVisible(true);
        return;
    }

    const QRect mapped = mappedSelection(m_image, m_pendingSelection, m_pendingOverlayWidth,
                                         m_pendingOverlayHeight);
    const bool tiny = mapped.width() < 6 || mapped.height() < 6;
    const QImage result = tiny ? m_image : m_image.copy(mapped);
    persistResult(result, token);
}

void ScreenshotController::handleCaptureFailed(QString reason, std::uint64_t generation)
{
    if (!isCaptureWaiting(m_phase) || generation != m_requestGeneration)
        return;
    failOperation(reason, m_operationToken);
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

void ScreenshotController::failOperation(const QString &reason, std::uint64_t token)
{
    if (token != m_operationToken)
        return;
    emit captureFailed(reason);
    returnToIdle();
}

void ScreenshotController::returnToIdle()
{
    ++m_operationToken;
    setPhase(ScreenshotPhase::Idle);
    setVisible(false);
    setSelectionImageVisible(false);
    m_selection = {};
    m_pendingSelection = {};
    emit selectionChanged();
}

void ScreenshotController::cancel()
{
    if (isCaptureWaiting(m_phase) && m_client)
        m_client->cancelCapture();
    ++m_operationToken;
    setPhase(ScreenshotPhase::Idle);
    setVisible(false);
    setSelectionImageVisible(false);
    m_selection = {};
    m_pendingSelection = {};
    emit selectionChanged();
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
            failOperation(QStringLiteral("invalid screenshot image"), token);
        return false;
    }

    const QString path = nextOutputPath();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !result.save(&file, "PNG") || !file.commit()) {
        failOperation(QStringLiteral("screenshot save failed"), token);
        return false;
    }
    if (auto *clipboard = QGuiApplication::clipboard())
        clipboard->setImage(result);

    m_image = result.copy();
    ++m_imageGeneration;
    emit imageChanged();
    if (m_resultPath != path) {
        m_resultPath = path;
        emit resultChanged();
    }
    emit saved(path);
    returnToIdle();
    setThumbnailVisible(true);
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
    setSelectionImageVisible(false);
    setPhase(ScreenshotPhase::WaitingLiveCapture);
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
