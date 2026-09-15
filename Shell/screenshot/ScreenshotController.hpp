#pragma once

#include <QImage>
#include <QObject>
#include <QRectF>
#include <QString>

#include <cstdint>
#include <functional>

#include "screenshot/ScreenshotUiBarrier.hpp"

class TyphonScreenCaptureClient;

class ScreenshotController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible NOTIFY visibleChanged)
    Q_PROPERTY(QString imageSource READ imageSource NOTIFY imageChanged)
    Q_PROPERTY(ScreenshotIntent intent READ intent NOTIFY intentChanged)
    Q_PROPERTY(QRectF selection READ selection WRITE setSelection NOTIFY selectionChanged)
    Q_PROPERTY(bool selectionImageVisible READ selectionImageVisible NOTIFY selectionImageVisibleChanged)
    Q_PROPERTY(ScreenshotPhase phase READ phase NOTIFY phaseChanged)
    Q_PROPERTY(bool thumbnailVisible READ thumbnailVisible NOTIFY thumbnailVisibleChanged)
    Q_PROPERTY(bool previewVisible READ previewVisible NOTIFY previewVisibleChanged)
    Q_PROPERTY(QRectF thumbnailRect READ thumbnailRect WRITE setThumbnailRect NOTIFY thumbnailRectChanged)
    Q_PROPERTY(int imageWidth READ imageWidth NOTIFY imageChanged)
    Q_PROPERTY(int imageHeight READ imageHeight NOTIFY imageChanged)
    Q_PROPERTY(QString resultPath READ resultPath NOTIFY resultChanged)
    Q_PROPERTY(QString resultUri READ resultUri NOTIFY resultChanged)

public:
    enum class ScreenshotIntent {
        QuickFullscreen,
        FrozenRegion,
        LiveRegion,
    };
    Q_ENUM(ScreenshotIntent)

    enum class ScreenshotPhase {
        Idle,
        PreparingCapture,
        WaitingCapture,
        FrozenSelecting,
        LiveSelecting,
        WaitingLiveCapture,
    };
    Q_ENUM(ScreenshotPhase)

    explicit ScreenshotController(TyphonScreenCaptureClient *client, QObject *parent = nullptr);

    bool isVisible() const { return m_visible; }
    QString imageSource() const;
    ScreenshotIntent intent() const { return m_intent; }
    QImage image() const { return m_image; }
    QRectF selection() const { return m_selection; }
    bool selectionImageVisible() const { return m_selectionImageVisible; }
    ScreenshotPhase phase() const { return m_phase; }
    bool thumbnailVisible() const { return m_thumbnailVisible; }
    bool previewVisible() const { return m_previewVisible; }
    QRectF thumbnailRect() const { return m_thumbnailRect; }
    int imageWidth() const { return m_image.width(); }
    int imageHeight() const { return m_image.height(); }
    QString resultPath() const { return m_resultPath; }
    QString resultUri() const;

    Q_INVOKABLE bool startQuickCapture();
    Q_INVOKABLE bool startFrozenRegionCapture();
    Q_INVOKABLE bool startLiveRegionCapture();
    Q_INVOKABLE bool capture();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool saveSelection();
    Q_INVOKABLE bool finishSelection();
    Q_INVOKABLE void dismissThumbnail();
    Q_INVOKABLE void openPreview();
    Q_INVOKABLE void closePreview();
    Q_INVOKABLE void setOverlaySize(qreal width, qreal height);
    void setSelection(const QRectF &selection);
    void setThumbnailRect(const QRectF &rect);
    void setUiBarrier(ScreenshotUiBarrier *barrier) { m_uiBarrier = barrier; }

signals:
    void visibleChanged();
    void imageChanged();
    void intentChanged();
    void selectionChanged();
    void selectionImageVisibleChanged();
    void phaseChanged();
    void thumbnailVisibleChanged();
    void previewVisibleChanged();
    void thumbnailRectChanged();
    void resultChanged();
    void captureFailed(QString reason);
    void saved(QString path);

private slots:
    void handleCaptureReady(QImage image, std::uint64_t generation);
    void handleCaptureFailed(QString reason, std::uint64_t generation);

private:
    friend class ScreenshotControllerTest;
    friend class ScreenshotOverlayQmlSmokeTest;
    friend class ScreenshotThumbnailQmlTest;
    bool begin(ScreenshotIntent intent);
    void handleUiBarrierFinished(std::uint64_t token, bool success, QString reason);
    void requestCapture(std::uint64_t token);
    void setPhase(ScreenshotPhase phase);
    void setVisible(bool visible);
    void setSelectionImageVisible(bool visible);
    void setThumbnailVisible(bool visible);
    void setPreviewVisible(bool visible);
    void failOperation(const QString &reason, std::uint64_t token);
    void returnToIdle();
    QRect mappedSelection() const;
    QRect mappedSelection(const QImage &image, const QRectF &selection, qreal width,
                          qreal height) const;
    QString nextOutputPath() const;
    bool persistResult(const QImage &result, std::uint64_t token);

    TyphonScreenCaptureClient *m_client = nullptr;
    ScreenshotUiBarrier *m_uiBarrier = nullptr;
    QImage m_image;
    QRectF m_selection;
    QRectF m_pendingSelection;
    qreal m_overlayWidth = 0.0;
    qreal m_overlayHeight = 0.0;
    qreal m_pendingOverlayWidth = 0.0;
    qreal m_pendingOverlayHeight = 0.0;
    QRectF m_thumbnailRect;
    QString m_resultPath;
    ScreenshotIntent m_intent = ScreenshotIntent::QuickFullscreen;
    ScreenshotPhase m_phase = ScreenshotPhase::Idle;
    std::uint64_t m_operationToken = 0;
    std::uint64_t m_requestGeneration = 0;
    std::uint64_t m_imageGeneration = 0;
    std::function<bool()> m_captureRequest;
    std::uint64_t m_captureGeneration = 0;
    bool m_visible = false;
    bool m_selectionImageVisible = false;
    bool m_thumbnailVisible = false;
    bool m_previewVisible = false;
};

using ScreenshotIntent = ScreenshotController::ScreenshotIntent;
using ScreenshotPhase = ScreenshotController::ScreenshotPhase;

Q_DECLARE_METATYPE(ScreenshotController::ScreenshotIntent)
Q_DECLARE_METATYPE(ScreenshotController::ScreenshotPhase)
